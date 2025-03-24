#pragma once
// 只包含需要的头文件
#include "ThreadCache.h"

namespace llt_memoryPool
{

class MemoryPool
{
public:
    static void* allocate(size_t size)
    {
        //LogDebug("[MemoryPool:allocate] 分配内存请求，大小: " + std::to_string(size) + " 字节");
        void* ptr = ThreadCache::getInstance()->allocate(size);
        //LogDebug("[MemoryPool:allocate] 内存分配完成，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)));
        return ptr;
    }

    static void deallocate(void* ptr, size_t size)
    {
        //LogDebug("[MemoryPool:deallocate] 释放内存请求，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)) + "，大小: " + std::to_string(size) + " 字节");
        ThreadCache::getInstance()->deallocate(ptr, size);
    }

};

} // namespace llt_memoryPool