#include "../include/PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace llt_memoryPool
{
    void* PageCache::getPageAddress(Span* span)
    {
        if(span==nullptr)
        return nullptr;
        return span->start_address;
    }

    Span* PageCache::mapAddressToSpan(void* ptr)
    { 
        size_t page_id=reinterpret_cast<size_t>(ptr)>>PageShift;
        auto it=span_map_.find(page_id);
        if(it!=span_map_.end())
        {
            return it->second;
        }
        else {
            return nullptr;
        }
    }

    Span* PageCache::allocateSpan(size_t numPages)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        SpanList* sp=nullptr;
        Span* span=nullptr;
        if(!free_lists_[numPages-1].empty())
        {
            sp=&free_lists_[numPages-1];
            span=sp->begin();
            free_lists_[numPages-1].erase(span);
            return span;
        }
        for(size_t i=numPages;i<MaxPages;i++)
        {
            if(!free_lists_[i].empty())
            {
                sp=&free_lists_[i];
                break;
            }
        }
        
        if(sp==nullptr)
        {
            //先申请一大块内存
            span=newSpan(numPages);
        }
        else{
            span=sp->begin();
        }

        if(span->num_pages>=numPages)
        {
            Span* remain_span=new Span();
            char* address_=static_cast<char*>(span->start_address);
            remain_span->num_pages=span->num_pages-numPages;
            remain_span->start_address=address_+numPages*PAGE_SIZE;
            span->num_pages=numPages;
            size_t start_pages=AddressToPageID(span->start_address);
            for(size_t i=0;i<remain_span->num_pages;i++)
            {
                span_map_[start_pages+numPages+i]=remain_span;
            }
            if(remain_span->num_pages>0)
            free_lists_[remain_span->num_pages-1].push_front(remain_span);
        }
        return span;
    }

    Span* PageCache::newSpan(size_t numPages)
    {
        size_t size_alloc=std::max(numPages,MinSystemAllocPages)*PAGE_SIZE;
        void* ptr=mmap(nullptr,size_alloc,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(ptr==MAP_FAILED)
        {
            return nullptr;
        }
        Span* new_span=new Span();
        size_t start_page=reinterpret_cast<size_t>(ptr)>>PageShift;
        size_t actual_pages=size_alloc>>PageShift;

        new_span->start_address=ptr;
        new_span->num_pages=actual_pages;
        new_span->location=false;
        for(size_t i=0;i<actual_pages;i++)
        {
            span_map_[start_page+i]=new_span;
        }
        return new_span;
    }

    void PageCache::deallocateSpan(Span* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t current_id=AddressToPageID(ptr->start_address);
        size_t prev_id=current_id-1;
        auto it=span_map_.find(prev_id);
        char* current_address=static_cast<char*>(ptr->start_address);
        if(it!=span_map_.end())
        {
            Span* prev_span=it->second;
            char* prev_address=static_cast<char*>(prev_span->start_address);
            if(prev_span->location==false&&prev_address+prev_span->num_pages*PAGE_SIZE==current_address)
            {
                free_lists_[prev_span->num_pages-1].erase(prev_span);
                //归还的时候，它还不在空闲列表中
                // free_lists_[ptr->num_pages-1].erase(ptr);
                prev_span->num_pages+=ptr->num_pages;
                for(size_t i=0;i<ptr->num_pages;i++)
                {
                    span_map_[current_id+i]=prev_span;
                }
                delete ptr;
                ptr=prev_span;
            }
        }
        current_address=static_cast<char*>(ptr->start_address);
        current_id=AddressToPageID(ptr->start_address);
        size_t next_id=current_id+ptr->num_pages;
        it=span_map_.find(next_id);
        if(it!=span_map_.end())
        { 
            Span* next_span=it->second;
            char* next_address=static_cast<char*>(next_span->start_address);
            if(next_span->location==false&&next_address==current_address+ptr->num_pages*PAGE_SIZE)
            {
                free_lists_[next_span->num_pages-1].erase(next_span);
                ptr->num_pages+=next_span->num_pages;
                for(size_t i=0;i<next_span->num_pages;i++)
                {
                    span_map_[next_id+i]=ptr;
                }
                delete next_span;
            }
        }
        ptr->location=false;
        ptr->use_count=0;
        ptr->objects=nullptr;
        ptr->size_class=0;
        free_lists_[ptr->num_pages-1].push_front(ptr);
    } 
    
    
} // namespace llt_memoryPool