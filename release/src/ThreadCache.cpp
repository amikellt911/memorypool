#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace llt_memoryPool
{

void* ThreadCache::allocate(size_t size)
{

    // 处理0大小的分配请求
    if (size == 0)
    {
        size = ALIGNMENT; // 至少分配一个对齐大小
     }
    
    if (size > MAX_BYTES)
    {
        // 大对象直接从系统分配
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);


    // 检查线程本地自由链表
    // 如果 freeList_[index] 不为空，表示该链表中有可用内存块
    if (void* ptr = freeList_[index])
    {
        freeList_[index] = *reinterpret_cast<void**>(ptr); // 将freeList_[index]指向的内存块的下一个内存块地址（取决于内存块的实现）
        // 更新自由链表大小
        freeListSize_[index]--;
        return ptr;
    }

    // 如果线程本地自由链表为空，则从中心缓存获取一批内存
    fetchFromCentralCache(index);
    return allocate(size);
}

void ThreadCache::deallocate(void* ptr, size_t size)
{

    if (size > MAX_BYTES)
    {

        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);

    // 插入到线程本地自由链表
    *reinterpret_cast<void**>(ptr) = freeList_[index];
    freeList_[index] = ptr;

     // 更新自由链表大小
    freeListSize_[index]++; // 增加对应大小类的自由链表大小

    // 判断是否需要将部分内存回收给中心缓存
    //bug:getsize是这个index的字节大小，而不是尺寸
    // if (freeListSize_[index]>SizeClass::getSize(index))
    // {
    //     releaseExcessMemory(index);
    // }
    if(freeListSize_[index]>SizeClass::getBatchNum(SizeClass::getSize(index))*2){
        releaseExcessMemory(index);
    }
}

void ThreadCache::releaseExcessMemory(size_t index)
{
    size_t num_to_release = freeListSize_[index]/2;
    if(num_to_release==0) return;
    void* start=freeList_[index];
    void* end=start;
    //-1是因为我们要走到num_to_release的位置，而不是下一个位置
    //因为我们只要走到了num_to_release的位置，我们就可以分割了
    for(size_t i=0;i<num_to_release-1;i++)
    {
        std::cout<<"freeListSize["<<index<<"]"<<freeListSize_[index]<<std::endl;
        end=*reinterpret_cast<void**>(end);
    }
    freeList_[index]=*reinterpret_cast<void**>(end);
    *reinterpret_cast<void**>(end)=nullptr;
    freeListSize_[index]-=num_to_release;
    CentralCache::getInstance().releaseListToSpans(start, num_to_release, SizeClass::getSize(index));
}

ThreadCache::~ThreadCache()
{
    // 遍历所有自由链表
    for (size_t i = 0; i < FREE_LIST_SIZE; ++i) {
        if (freeList_[i] != nullptr) {
            releaseAllMemory(i);
        }
    }
}

void ThreadCache::releaseAllMemory(size_t index)
{
    if(freeList_[index]==nullptr) return;
    void* start = freeList_[index];
    size_t num_to_release = freeListSize_[index];
    size_t bytes=SizeClass::getSize(index);
    CentralCache::getInstance().releaseListToSpans(start, num_to_release, bytes);
    freeList_[index]=nullptr;
    freeListSize_[index]=0;
}

void ThreadCache::fetchFromCentralCache(size_t index)
{
    void* start=nullptr;
    void* end=nullptr;
    size_t size = (index + 1) * ALIGNMENT;
    // 根据对象内存大小计算批量获取的数量
    size_t batchNum = SizeClass::getBatchNum(size);

    // 从中心缓存批量获取内存
    size_t fetchNum=CentralCache::getInstance().fetchRange(start,end,index, batchNum);
    std::cout<<"fetchNum:"<<fetchNum<<std::endl;
    if (fetchNum==0) {
        return;
    }

    if(freeList_[index]==nullptr)
    {  
        freeList_[index] = start;
    }
    else
    {
        *reinterpret_cast<void**>(findTail(freeList_[index])) = start;
    }
    // 更新自由链表大小
    freeListSize_[index] += fetchNum; // 增加对应大小类的自由链表大小
    return;
}


// 计算批量获取内存块的数量

void* ThreadCache::findTail(void* head)
{
    while(*reinterpret_cast<void**>(head))
    {
        head=*reinterpret_cast<void**>(head);
    }
    return head;
}
} // namespace memoryPool