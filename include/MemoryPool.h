#pragma once
#include "ThreadCache.h"
#include "logger.h"
#include "LogMacros.h"

namespace llt_memoryPool
{

class MemoryPool
{
public:
    static void* allocate(size_t size)
    {
        LOG_DEBUG("[MemoryPool:allocate] 分配内存请求，大小: " + std::to_string(size) + " 字节");
        void* ptr = ThreadCache::getInstance()->allocate(size);
        LOG_DEBUG("[MemoryPool:allocate] 内存分配完成，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)));
        return ptr;
    }

    static void deallocate(void* ptr, size_t size)
    {
        LOG_DEBUG("[MemoryPool:deallocate] 释放内存请求，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)) + "，大小: " + std::to_string(size) + " 字节");
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
    
    // 设置日志级别
    static void setLogLevel(LogLevel level)
    {
        SET_LOG_LEVEL(level);
    }
};

} // namespace memoryPool