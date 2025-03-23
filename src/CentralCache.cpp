#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace llt_memoryPool
{

// 每次从PageCache获取span大小（以页为单位）
static const size_t SPAN_PAGES = 8;

void* CentralCache::fetchRange(size_t index, size_t batchNum)
{
    Logger::getInstance().debug("[CentralCache:fetchRange] 开始获取内存范围，索引: " + std::to_string(index) + 
                           "，请求批量数: " + std::to_string(batchNum));
    
    // 索引检查，当索引大于等于FREE_LIST_SIZE时，说明申请内存过大应直接向系统申请
    if (index >= FREE_LIST_SIZE || batchNum == 0) {
        Logger::getInstance().warn("[CentralCache:fetchRange] 索引超出范围或批量数为0，返回nullptr");
        return nullptr;
    }

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
    }
    Logger::getInstance().debug("[CentralCache:fetchRange] 获取锁成功，索引: " + std::to_string(index));

    void* result = nullptr;
    try 
    {
        // 尝试从中心缓存获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);

        if (!result)
        {
            Logger::getInstance().debug("[CentralCache:fetchRange] 中心缓存为空，从页缓存获取新内存");
            // 如果中心缓存为空，从页缓存获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            if (!result)
            {
                Logger::getInstance().error("[CentralCache:fetchRange] 从页缓存获取内存失败");
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 将从PageCache获取的内存块切分成小块
            char* start = static_cast<char*>(result);
            size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;
            size_t allocBlocks = std::min(batchNum, totalBlocks);
            
            Logger::getInstance().debug("[CentralCache:fetchRange] 从页缓存获取内存成功，共 " + 
                                   std::to_string(totalBlocks) + " 块，分配 " + 
                                   std::to_string(allocBlocks) + " 块");
            
            // 构建返回给ThreadCache的内存块链表
            if (allocBlocks > 1) 
            {  
                // 确保至少有两个块才构建链表
                // 构建链表
                for (size_t i = 1; i < allocBlocks; ++i) 
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (allocBlocks - 1) * size) = nullptr;
                Logger::getInstance().debug("[CentralCache:fetchRange] 构建返回链表完成");
            }

            // 构建保留在CentralCache的链表
            if (totalBlocks > allocBlocks)
            {
                void* remainStart = start + allocBlocks * size;
                for (size_t i = allocBlocks + 1; i < totalBlocks; ++i)
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (totalBlocks - 1) * size) = nullptr;

                centralFreeList_[index].store(remainStart, std::memory_order_release);
                Logger::getInstance().debug("[CentralCache:fetchRange] 剩余 " + std::to_string(totalBlocks - allocBlocks) + 
                                       " 块存入中心缓存");
            }
        } 
        else // 如果中心缓存有index对应大小的内存块
        {
            Logger::getInstance().debug("[CentralCache:fetchRange] 中心缓存有可用内存块");
            // 从现有链表中获取指定数量的块
            void* current = result;
            void* prev = nullptr;
            size_t count = 0;

            while (current && count < batchNum)
            {
                prev = current;
                current = *reinterpret_cast<void**>(current);
                count++;
            }

            if (prev) // 当前centralFreeList_[index]链表上的内存块大于batchNum时需要用到 
            {
                *reinterpret_cast<void**>(prev) = nullptr;
            }

            centralFreeList_[index].store(current, std::memory_order_release);
            Logger::getInstance().debug("[CentralCache:fetchRange] 从中心缓存获取 " + 
                                   std::to_string(count) + " 块，剩余块更新到链表");
        }
    }
    catch (const std::exception& e) 
    {
        Logger::getInstance().error("[CentralCache:fetchRange] 异常: " + std::string(e.what()));
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    Logger::getInstance().debug("[CentralCache:fetchRange] 释放锁，返回内存地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(result)));
    return result;
}

void CentralCache::returnRange(void* start, size_t size, size_t index)
{
    Logger::getInstance().debug("[CentralCache:returnRange] 开始归还内存，起始地址: " + 
                           std::to_string(reinterpret_cast<uintptr_t>(start)) + 
                           "，大小: " + std::to_string(size) + 
                           "，索引: " + std::to_string(index));
                           
    // 当索引大于等于FREE_LIST_SIZE时，说明内存过大应直接向系统归还
    if (!start || index >= FREE_LIST_SIZE) {
        Logger::getInstance().warn("[CentralCache:returnRange] 无效参数，不进行归还");
        return;
    }

    while (locks_[index].test_and_set(std::memory_order_acquire)) 
    {
        std::this_thread::yield();
    }
    Logger::getInstance().debug("[CentralCache:returnRange] 获取锁成功，索引: " + std::to_string(index));

    try 
    {
        // 找到要归还的链表的最后一个节点
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < size) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        Logger::getInstance().debug("[CentralCache:returnRange] 归还链表长度: " + std::to_string(count));

        // 将归还的链表连接到中心缓存的链表头部
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current;  // 将原链表头接到归还链表的尾部
        centralFreeList_[index].store(start, std::memory_order_release);  // 将归还的链表头设为新的链表头
        
        Logger::getInstance().debug("[CentralCache:returnRange] 链表归还成功");
    }
    catch (const std::exception& e) 
    {
        Logger::getInstance().error("[CentralCache:returnRange] 异常: " + std::string(e.what()));
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
    Logger::getInstance().debug("[CentralCache:returnRange] 释放锁");
}

void* CentralCache::fetchFromPageCache(size_t size)
{   
    Logger::getInstance().debug("[CentralCache:fetchFromPageCache] 从页缓存获取内存，请求大小: " + std::to_string(size) + " 字节");
    
    // 1. 计算实际需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
    Logger::getInstance().debug("[CentralCache:fetchFromPageCache] 计算需要页数: " + std::to_string(numPages));

    // 2. 根据大小决定分配策略
    void* result = nullptr;
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE) 
    {
        // 小于等于32KB的请求，使用固定8页
        Logger::getInstance().debug("[CentralCache:fetchFromPageCache] 小对象请求，使用固定 " + 
                               std::to_string(SPAN_PAGES) + " 页");
        result = PageCache::getInstance().allocateSpan(SPAN_PAGES);
    } 
    else 
    {
        // 大于32KB的请求，按实际需求分配
        Logger::getInstance().debug("[CentralCache:fetchFromPageCache] 大对象请求，按实际需求分配 " + 
                               std::to_string(numPages) + " 页");
        result = PageCache::getInstance().allocateSpan(numPages);
    }
    
    if (!result) {
        Logger::getInstance().error("[CentralCache:fetchFromPageCache] 从页缓存获取内存失败");
    } else {
        Logger::getInstance().debug("[CentralCache:fetchFromPageCache] 从页缓存获取内存成功，地址: " + 
                               std::to_string(reinterpret_cast<uintptr_t>(result)));
    }
    
    return result;
}

} // namespace memoryPool