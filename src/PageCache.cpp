#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace llt_memoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    Logger::getInstance().debug("[PageCache:allocateSpan] 开始分配Span，请求页数: " + std::to_string(numPages));
    
    std::lock_guard<std::mutex> lock(mutex_);
    Logger::getInstance().debug("[PageCache:allocateSpan] 获取互斥锁成功");

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        Logger::getInstance().debug("[PageCache:allocateSpan] 找到合适的空闲Span，大小: " + std::to_string(it->first) + " 页");
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next;
            Logger::getInstance().debug("[PageCache:allocateSpan] Span链表有后续节点，更新链表头");
        }
        else
        {
            freeSpans_.erase(it);
            Logger::getInstance().debug("[PageCache:allocateSpan] Span链表无后续节点，删除链表");
        }

        // 如果span大于需要的numPages则进行分割
        if (span->numPages > numPages) 
        {
            Logger::getInstance().debug("[PageCache:allocateSpan] Span大小超过需求，进行分割");
            Span* newSpan = new Span;
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + 
                                numPages * PAGE_SIZE;
            newSpan->numPages = span->numPages - numPages;
            newSpan->next = nullptr;

            // 将超出部分放回空闲Span*列表头部
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;
            list = newSpan;
            Logger::getInstance().debug("[PageCache:allocateSpan] 分割完成，返回剩余 " + 
                                   std::to_string(newSpan->numPages) + " 页到空闲列表");

            span->numPages = numPages;
        }

        // 记录span信息用于回收
        spanMap_[span->pageAddr] = span;
        Logger::getInstance().debug("[PageCache:allocateSpan] 从空闲列表分配Span成功，地址: " + 
                               std::to_string(reinterpret_cast<uintptr_t>(span->pageAddr)));
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    Logger::getInstance().debug("[PageCache:allocateSpan] 没有合适的空闲Span，向系统申请内存");
    void* memory = systemAlloc(numPages);
    if (!memory) {
        Logger::getInstance().error("[PageCache:allocateSpan] 系统内存分配失败");
        return nullptr;
    }

    // 创建新的span
    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录span信息用于回收
    spanMap_[memory] = span;
    Logger::getInstance().debug("[PageCache:allocateSpan] 从系统分配Span成功，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(memory)));
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    Logger::getInstance().debug("[PageCache:deallocateSpan] 开始释放Span，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(ptr)) + 
                           "，页数: " + std::to_string(numPages));
                           
    std::lock_guard<std::mutex> lock(mutex_);
    Logger::getInstance().debug("[PageCache:deallocateSpan] 获取互斥锁成功");

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) {
        Logger::getInstance().warn("[PageCache:deallocateSpan] 未找到对应Span，不是PageCache分配的内存");
        return;
    }

    Span* span = it->second;
    Logger::getInstance().debug("[PageCache:deallocateSpan] 找到对应Span，页数: " + std::to_string(span->numPages));

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);
    
    if (nextIt != spanMap_.end())
    {
        Logger::getInstance().debug("[PageCache:deallocateSpan] 找到相邻Span，尝试合并");
        Span* nextSpan = nextIt->second;
        
        // 1. 首先检查nextSpan是否在空闲链表中
        bool found = false;
        auto& nextList = freeSpans_[nextSpan->numPages];
        
        // 检查是否是头节点
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
            Logger::getInstance().debug("[PageCache:deallocateSpan] 相邻Span是链表头节点，从链表中移除");
        }
        else if (nextList) // 只有在链表非空时才遍历
        {
            Span* prev = nextList;
            while (prev->next)
            {
                if (prev->next == nextSpan)
                {   
                    // 将nextSpan从空闲链表中移除
                    prev->next = nextSpan->next;
                    found = true;
                    Logger::getInstance().debug("[PageCache:deallocateSpan] 相邻Span在链表中间，从链表中移除");
                    break;
                }
                prev = prev->next;
            }
        }

        // 2. 只有在找到nextSpan的情况下才进行合并
        if (found)
        {
            // 合并span
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
            Logger::getInstance().debug("[PageCache:deallocateSpan] 合并完成，新Span页数: " + std::to_string(span->numPages));
        }
        else {
            Logger::getInstance().debug("[PageCache:deallocateSpan] 相邻Span不在空闲列表中，不进行合并");
        }
    }

    // 将合并后的span通过头插法插入空闲列表
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
    Logger::getInstance().debug("[PageCache:deallocateSpan] Span释放完成，插入空闲列表，大小: " + 
                           std::to_string(span->numPages) + " 页");
}

void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;
    Logger::getInstance().debug("[PageCache:systemAlloc] 向系统申请内存，大小: " + std::to_string(size) + " 字节");

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        Logger::getInstance().error("[PageCache:systemAlloc] 系统内存分配失败，mmap返回MAP_FAILED");
        return nullptr;
    }

    // 清零内存
    memset(ptr, 0, size);
    Logger::getInstance().debug("[PageCache:systemAlloc] 系统内存分配成功，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(ptr)));
    return ptr;
}

} // namespace memoryPool