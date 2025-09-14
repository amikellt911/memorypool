#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

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
    std::lock_guard<std::mutex> lock(span_lists_mutex_[index]);
    size_t fetchNum = 0;
    SpanList& sp=*span_lists_[index];
    Span* target_span=nullptr;
    for(Span* span=sp.begin();span!=sp.end();span=span->next)
    {
        if(!span->isFull())
        {
            target_span=span;
            break;
        }
    }
    if(target_span==nullptr)
    {
        size_t num_pages=SizeClass::getPages(index);
        target_span = PageCache::getInstance().allocateSpan(num_pages);
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
        sp.push_front(target_span);
    }
    fetchNum=std::min(target_span->getFreeObjects(),batchNum);
    start=target_span->objects;
    end=start;
    for(size_t i=0;i<fetchNum-1;++i)
    {
        end=*reinterpret_cast<void**>(end);
    }
    target_span->objects=*reinterpret_cast<void**>(end);
    *reinterpret_cast<void**>(end)=nullptr;
    target_span->use_count+=fetchNum;
    target_span->location=true;
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
    std::lock_guard<std::mutex> lock(span_lists_mutex_[index]);
    SpanList& sp=*span_lists_[index];
    void* current=start;
    while(current!=nullptr)
    {
        void* next=*reinterpret_cast<void**>(current);
        Span* span=PageCache::getInstance().mapAddressToSpan(current);
        assert(index==span->size_class);
        if(span==nullptr)
        {
            return;
        }
        *static_cast<void**>(current)=span->objects;
        span->objects=current;
        span->use_count--;
        if(span->use_count==0){
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