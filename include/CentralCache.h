#pragma once
#include "Common.h"
#include <mutex>

namespace llt_memoryPool
{

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }
    //之前没有batchNum，只能自适应获得合适大小
    //*&，指针的引用，不需要写二级指针了。
    size_t fetchRange(void*& start,void*& end,size_t index, size_t batchNum);
    void releaseListToSpans(void* start, size_t size,size_t bytes);
    CentralCache(const CentralCache&)=delete;
    CentralCache& operator=(const CentralCache&)=delete;

private:
    // 相互是还所有原子指针为nullptr
    //=default，default会默认nullptr
    CentralCache();
    ~CentralCache();


private:
    // 中心缓存的自由链表
    //mutex是不可拷贝的，而array的默认构造又必须要拷贝，所以有问题，所以可以从指针间接持有
    std::array<SpanList*, FREE_LIST_SIZE> span_lists_;
    std::array<std::mutex, FREE_LIST_SIZE> span_lists_mutex_;
};

} // namespace llt_memoryPool