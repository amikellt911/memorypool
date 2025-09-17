#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>
#include <vector>

namespace llt_memoryPool
{

// 每次从PageCache获取span大小（以页为单位）
static const size_t SPAN_PAGES = 8;

CentralCache::CentralCache()
{
    for(size_t i = 0; i < FREE_LIST_SIZE; ++i)
    {
        span_lists_[i]=new SpanList();
    } 
}

CentralCache::~CentralCache()
{
    for(size_t i = 0; i < FREE_LIST_SIZE; ++i)
    {
        delete span_lists_[i];
    }
}

size_t CentralCache::fetchRange(void*& start,void*& end,size_t index, size_t batchNum)
{
    
    size_t fetchNum = 0;
    SpanList& sp=*span_lists_[index];
    Span* target_span=nullptr;
    std::lock_guard<std::mutex> lock(span_lists_mutex_[index]);
    //span_lists_mutex_[index].lock();
    for(Span* span=sp.begin();span!=sp.end();span=span->next)
    {
        if(!span->isFull())
        {
            target_span=span;
            break;
        }
    }
    //span_lists_mutex_[index].unlock();
    if(target_span==nullptr)
    {
        size_t num_pages=SizeClass::getPages(index);
        //解锁？
        target_span = PageCache::getInstance().allocateSpan(num_pages);
        //target_span->lock_.lock();
        target_span->size_class=index;
        if(target_span==nullptr)
        {
            return 0;
        }
        char* ptr=static_cast<char*>(PageCache::getPageAddress(target_span));
        size_t object_size=SizeClass::getSize(index);
        size_t nums_object=target_span->getTotalObjects();
        void* head=nullptr;
        for(size_t i=0;i<nums_object;++i)
        {
            void* current=ptr+i*object_size;
            *reinterpret_cast<void**>(current)=head;
            head=current;
        }
        target_span->objects=head;
        //target_span->lock_.unlock();
        //span_lists_mutex_[index].lock();
        sp.push_front(target_span);
        //span_lists_mutex_[index].unlock();
    }
    //span_lists_mutex_[index].unlock();
    //target_span->lock_.lock();
    fetchNum=std::min(target_span->getFreeObjects(),batchNum);
    start=target_span->objects;
    end=start;
    for(size_t i=0;i<fetchNum-1;++i)
    {
        if (end == nullptr) {
            // The list is shorter than fetchNum. This indicates a likely bug in span's
            // object tracking. To prevent a crash, we must stop here and return what we have.
            // The list is now start -> ... -> end (which is the last valid node)
            *reinterpret_cast<void**>(start) = nullptr; // Properly terminate the list we are returning
            fetchNum = i + 1; // We actually fetched i+1 items
            target_span->objects = nullptr; // The span's free list is now empty
            target_span->use_count += fetchNum;
            target_span->location=true;
            return fetchNum;
        }
        end=*reinterpret_cast<void**>(end);
    }
 
    if (end != nullptr) {
        target_span->objects=*reinterpret_cast<void**>(end);
        *reinterpret_cast<void**>(end)=nullptr;
    } else {
        // This can happen if the list had exactly fetchNum-1 items.
        target_span->objects = nullptr;
    }

    target_span->use_count+=fetchNum;
    target_span->location=true;
    //target_span->lock_.unlock();
    if(target_span->isFull())
    {   
        sp.erase(target_span);
        sp.push_back(target_span);
    }
    return fetchNum;
}

void CentralCache::releaseListToSpans(void* start, size_t size, size_t bytes)
{
    int index=SizeClass::getIndex(bytes);
    SpanList& sp=*span_lists_[index];
    void* current=start;
    std::lock_guard<std::mutex> lock(span_lists_mutex_[index]);
    while(current!=nullptr)
    {
        void* next=*reinterpret_cast<void**>(current);
        Span* span=PageCache::getInstance().mapAddressToSpan(current);
        //span->lock_.lock();
        assert(index==span->size_class);
        if(span==nullptr)
        {
            return;
        }
        *static_cast<void**>(current)=span->objects;
        span->objects=current;
        span->use_count--;
        span->lock_.unlock();
        if(span->use_count==0){
            //std::lock_guard<std::mutex> lock(span_lists_mutex_[index]);
            //std::lock_guard<std::mutex> lock1(span->lock_);
            sp.erase(span);
            span->objects=nullptr;
            span->size_class=0;
            //感觉location这个变量没用到
            span->location=false;
            PageCache::getInstance().deallocateSpan(span);
        }
        current=next;
    }
}



} // namespace memoryPool