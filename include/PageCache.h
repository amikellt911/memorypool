#pragma once
#include "Common.h"
#include <unordered_map>
#include <mutex>

namespace llt_memoryPool
{

class PageCache
{
public:
    static PageCache& getInstance()
    {
        static PageCache instance;
        //LogDebug("[PageCache:getInstance] 获取页缓存实例");
        return instance;
    }
    PageCache(const PageCache&)=delete;
    PageCache& operator=(const PageCache&)=delete;

    // 分配指定页数的span
    Span* allocateSpan(size_t numPages);

    // 释放span
    void deallocateSpan(Span* ptr);

    static void* getPageAddress(Span* span);

    Span* mapAddressToSpan(void* ptr);

    static inline size_t AddressToPageID(void* ptr) {
    return reinterpret_cast<uintptr_t>(ptr) >> PageShift;
}
private:
    PageCache()=default;
    ~PageCache()=default;
    //向操作系统申请新一块内存
    Span* newSpan(size_t num_pages);
    //void mergeSpan(Span* span);
    //Span* splitSpan(Span* span, size_t num_pages);
private:
    static const size_t MaxPages = 256; // 256/4
    SpanList free_lists_[MaxPages];
    std::unordered_map<size_t, Span*> span_map_;
    std::mutex mutex_;
};

} // namespace llt_memoryPool