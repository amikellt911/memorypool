#pragma once
#include <cstddef>
#include <array>
#include <cstdlib>
#include "logger.h"
#include <mutex>

namespace llt_memoryPool 
{
// 对齐数和大小定义
constexpr size_t ALIGNMENT = 8;
constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小
constexpr size_t PAGE_SIZE = 4096; // 4K页大小
constexpr size_t PageShift =12;
constexpr size_t MinSystemAllocPages=128;
// 【约束 1】一次性从 PageHeap 批发的总内存，最好别超过一个上限
//           这个值可以比 ThreadCache 的上限大，比如 128KB
constexpr size_t MAX_BYTES_PER_SPAN = 128 * 1024; 

// 【约束 2】一个 Span 至少能容纳多少个 ThreadCache 的批量？
//我们希望一个 Span 至少能满足 8 次 ThreadCache 的 fetch 请求
constexpr size_t MIN_BATCHES_PER_SPAN = 8;

// 内存块头部信息
struct BlockHeader
{
    size_t size; // 内存块大小
    bool   inUse; // 使用标志
    BlockHeader* next; // 指向下一个内存块
};

// 大小类管理
class SizeClass 
{
public:
    static size_t roundUp(size_t bytes)
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    static size_t getIndex(size_t bytes)
    {   
        // 确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }

    static size_t getSize(size_t index)
    {
        return index * ALIGNMENT + ALIGNMENT;
    }
    static size_t getPages(size_t index)
    {
        const size_t object_size=SizeClass::getSize(index);
        size_t batchnums=getBatchNum(object_size);
        size_t desire_sizes=batchnums*MIN_BATCHES_PER_SPAN;
        size_t desire_bytes=desire_sizes*object_size;
        size_t pages_by_desire=(PAGE_SIZE+desire_bytes-1)/PAGE_SIZE;
        size_t pages_by_limit=MAX_BYTES_PER_SPAN/PAGE_SIZE;
        size_t result_pages=std::min(pages_by_desire,pages_by_limit);
        return std::max(size_t(1),result_pages);
    }

    static size_t getBatchNum(size_t size)
{
    // 基准：每次批量获取不超过4KB内存
    constexpr size_t MAX_BATCH_SIZE = 4 * 1024; // 4KB

    // 根据对象大小设置合理的基准批量数
    size_t baseNum;
    if (size <= 32) baseNum = 64;    // 64 * 32 = 2KB
    else if (size <= 64) baseNum = 32;  // 32 * 64 = 2KB
    else if (size <= 128) baseNum = 16; // 16 * 128 = 2KB
    else if (size <= 256) baseNum = 8;  // 8 * 256 = 2KB
    else if (size <= 512) baseNum = 4;  // 4 * 512 = 2KB
    else if (size <= 1024) baseNum = 2; // 2 * 1024 = 2KB
    else baseNum = 1;                   // 大于1024的对象每次只从中心缓存取1个

    // 计算最大批量数
    size_t maxNum = std::max(size_t(1), MAX_BATCH_SIZE / size);

    // 取最小值，但确保至少返回1
    size_t result = std::max(size_t(1), std::min(maxNum, baseNum));
    return result;
}

};

struct Span{
    //size_t page_id;//开始页号
    void* start_address; 
    size_t num_pages;

    Span* next;
    Span* prev;
    //false代表未分配给centralCache，目前还在pageCache中
    bool location=false;

    void* objects=nullptr;
    //index等级
    size_t size_class=0;
    size_t use_count=0;

    size_t getTotalObjects()
    {
        return (num_pages*PAGE_SIZE)/SizeClass::getSize(size_class);
    }
    // 剩余未使用对象数
    size_t getFreeObjects()
    {
        return getTotalObjects()-use_count;
    }
    bool isFull()
    {
        return getFreeObjects()==0;
    }
};

class SpanList{
    public:
    SpanList(){
        head_=new Span;
        head_->next=head_;
        head_->prev=head_;  
        size_=0;
    }
    ~SpanList()
    {
        delete head_;
    }
    //350法则
    SpanList(const SpanList&) = delete;
    SpanList& operator=(const SpanList&) = delete;
    bool empty()
    {
        return head_->next==head_;
    }
    Span* begin()
    {
        return head_->next;
    }
    Span* end()
    {
        return head_;
    }
    void push_front(Span* span)
    {
        //加锁责任上移给调用者
        //因为mutex是不可重入锁，要么就用recursive_mutex
        //std::lock_guard lg(lock_);
        span->next=head_->next;
        span->prev=head_;
        head_->next->prev=span;
        head_->next=span;
        size_++;
    }
    void push_back(Span* span)
    {
        //std::lock_guard lg(lock_);
        span->next=head_;
        span->prev=head_->prev;
        head_->prev->next=span;
        head_->prev=span;
        size_++;
    }
    void erase(Span* span)
    {
        //std::lock_guard lg(lock_);
        span->prev->next = span->next;
        span->next->prev = span->prev;
        span->next = nullptr;
        span->prev = nullptr;
        size_--;
    }
    private:
        Span* head_;
        size_t size_=0;
};

} // namespace memoryPool