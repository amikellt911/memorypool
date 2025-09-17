#pragma once
#include "Common.h"

namespace llt_memoryPool 
{

// 线程本地缓存
class ThreadCache
{
public:
    static ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance;
        //LogDebug("[ThreadCache:getInstance] 获取线程本地缓存实例");
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
    ThreadCache(const ThreadCache&)=delete;
    ThreadCache& operator=(const ThreadCache&)=delete;
private:
    //之前不是default，是将freelist置nullptr和freelistSize置0
    ThreadCache() {
        // 初始化数组
        for (size_t i = 0; i < FREE_LIST_SIZE; ++i) {
            freeList_[i] = nullptr;
            freeListSize_[i] = 0;
        }
    }
    ~ThreadCache();
    // 从中心缓存获取内存
    void fetchFromCentralCache(size_t index);
    // 归还内存到中心缓存
    void releaseListToCentralCache(size_t index);
    // 判断是否需要归还内存给中心缓存
    // 场景A：当链表过长时，释放【一半】的内存块
    void releaseExcessMemory(size_t index);

    // 场景B：释放【所有】的内存块(析构)
    void releaseAllMemory(size_t index);

    void* findTail(void* head);
private:
    // 每个线程的自由链表数组
    std::array<void*, FREE_LIST_SIZE> freeList_;   
    std::array<size_t, FREE_LIST_SIZE> freeListSize_; // 自由链表大小统计
};

} // namespace memoryPool