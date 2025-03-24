#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace llt_memoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    LogDebug("[PageCache:allocateSpan] 开始分配Span，请求页数: " + std::to_string(numPages));
    
    std::lock_guard<std::mutex> lock(mutex_);
    LogDebug("[PageCache:allocateSpan] 获取互斥锁成功");

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    auto it = freeSpans_.lower_bound(numPages);
    if (it != freeSpans_.end())
    {
        LogDebug("[PageCache:allocateSpan] 找到合适的空闲Span，大小: " + std::to_string(it->first) + " 页");
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next;
            LogDebug("[PageCache:allocateSpan] Span链表有后续节点，更新链表头");
        }
        else
        {
            freeSpans_.erase(it);
            LogDebug("[PageCache:allocateSpan] Span链表无后续节点，删除链表");
        }

        // 如果span大于需要的numPages则进行分割
        if (span->numPages > numPages)
        {
            LogDebug("[PageCache:allocateSpan] 分割Span，原大小: " + std::to_string(span->numPages) + 
                             " 页，请求大小: " + std::to_string(numPages) + " 页");
                             
            // 创建新的span，包含剩余页
            Span* remainSpan = new Span;
            remainSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            remainSpan->numPages = span->numPages - numPages;
            remainSpan->next = nullptr;
            
            // 更新span map
            spanMap_[remainSpan->pageAddr] = remainSpan;
            
            // 将剩余span插入到合适的空闲列表
            auto remainIt = freeSpans_.find(remainSpan->numPages);
            if (remainIt != freeSpans_.end())
            {
                remainSpan->next = remainIt->second;
                remainIt->second = remainSpan;
                LogDebug("[PageCache:allocateSpan] 剩余Span插入到现有链表");
            }
            else
            {
                freeSpans_[remainSpan->numPages] = remainSpan;
                LogDebug("[PageCache:allocateSpan] 剩余Span创建新链表");
            }
            
            // 更新原span大小
            span->numPages = numPages;
        }
        
        LogDebug("[PageCache:allocateSpan] 返回Span，地址: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(span->pageAddr)) + 
                       "，大小: " + std::to_string(span->numPages) + " 页");
                       
        return span->pageAddr;
    }
    else
    {
        LogDebug("[PageCache:allocateSpan] 未找到合适的空闲Span，向系统申请内存");
        void* ptr = systemAlloc(numPages);
        if (ptr)
        {
            // 创建新的span
            Span* span = new Span;
            span->pageAddr = ptr;
            span->numPages = numPages;
            span->next = nullptr;
            
            // 更新span map
            spanMap_[ptr] = span;
            
            LogDebug("[PageCache:allocateSpan] 系统内存分配成功，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(ptr)) + 
                           "，大小: " + std::to_string(numPages) + " 页");
        }
        else
        {
            LogError("[PageCache:allocateSpan] 系统内存分配失败");
        }
        return ptr;
    }
}

void PageCache::deallocateSpan(void* ptr, size_t numPages)
{
    LogDebug("[PageCache:deallocateSpan] 开始释放Span，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(ptr)) + 
                           "，页数: " + std::to_string(numPages));
                           
    std::lock_guard<std::mutex> lock(mutex_);
    LogDebug("[PageCache:deallocateSpan] 获取互斥锁成功");

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) {
        LogWarn("[PageCache:deallocateSpan] 未找到对应Span，不是PageCache分配的内存");
        return;
    }

    Span* span = it->second;
    LogDebug("[PageCache:deallocateSpan] 找到对应Span，页数: " + std::to_string(span->numPages));

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
    auto nextIt = spanMap_.find(nextAddr);
    if (nextIt != spanMap_.end())
    {
        Span* nextSpan = nextIt->second;
        LogDebug("[PageCache:deallocateSpan] 找到后续相邻Span，地址: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(nextAddr)) + 
                       "，页数: " + std::to_string(nextSpan->numPages));
        
        // 从空闲列表中移除后续span
        auto freeIt = freeSpans_.find(nextSpan->numPages);
        if (freeIt != freeSpans_.end())
        {
            if (freeIt->second == nextSpan)
            {
                freeSpans_[nextSpan->numPages] = nextSpan->next;
                if (!nextSpan->next)
                {
                    freeSpans_.erase(nextSpan->numPages);
                }
            }
            else
            {
                Span* prev = freeIt->second;
                while (prev && prev->next != nextSpan)
                {
                    prev = prev->next;
                }
                if (prev)
                {
                    prev->next = nextSpan->next;
                }
            }
        }
        
        // 合并span
        span->numPages += nextSpan->numPages;
        spanMap_.erase(nextAddr);
        delete nextSpan;
        
        LogDebug("[PageCache:deallocateSpan] 合并后续Span，新大小: " + std::to_string(span->numPages) + " 页");
    }
    
    // 尝试向前合并
    void* prevEnds = nullptr;
    Span* prevSpan = nullptr;
    
    for (auto& entry : spanMap_)
    {
        void* endAddr = static_cast<char*>(entry.first) + entry.second->numPages * PAGE_SIZE;
        if (endAddr == ptr)
        {
            prevEnds = entry.first;
            prevSpan = entry.second;
            break;
        }
    }
    
    if (prevSpan)
    {
        LogDebug("[PageCache:deallocateSpan] 找到前置相邻Span，地址: " + 
                       std::to_string(reinterpret_cast<uintptr_t>(prevEnds)) + 
                       "，页数: " + std::to_string(prevSpan->numPages));
        
        // 从空闲列表中移除前置span
        auto freeIt = freeSpans_.find(prevSpan->numPages);
        if (freeIt != freeSpans_.end())
        {
            if (freeIt->second == prevSpan)
            {
                freeSpans_[prevSpan->numPages] = prevSpan->next;
                if (!prevSpan->next)
                {
                    freeSpans_.erase(prevSpan->numPages);
                }
            }
            else
            {
                Span* prev = freeIt->second;
                while (prev && prev->next != prevSpan)
                {
                    prev = prev->next;
                }
                if (prev)
                {
                    prev->next = prevSpan->next;
                }
            }
        }
        
        // 合并span
        prevSpan->numPages += span->numPages;
        spanMap_.erase(ptr);
        span = prevSpan;
        
        LogDebug("[PageCache:deallocateSpan] 合并前置Span，新大小: " + std::to_string(span->numPages) + " 页");
    }
    
    // 插入到空闲列表
    auto freeIt = freeSpans_.find(span->numPages);
    if (freeIt != freeSpans_.end())
    {
        span->next = freeIt->second;
        freeIt->second = span;
    }
    else
    {
        span->next = nullptr;
        freeSpans_[span->numPages] = span;
    }
    
    LogDebug("[PageCache:deallocateSpan] Span释放完成，插入空闲列表，大小: " + 
                           std::to_string(span->numPages) + " 页");
}

void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;
    LogDebug("[PageCache:systemAlloc] 向系统申请内存，大小: " + std::to_string(size) + " 字节");

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        LogError("[PageCache:systemAlloc] 系统内存分配失败，mmap返回MAP_FAILED");
        return nullptr;
    }

    // 清零内存
    memset(ptr, 0, size);
    LogDebug("[PageCache:systemAlloc] 系统内存分配成功，地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(ptr)));
    return ptr;
}

} // namespace llt_memoryPool