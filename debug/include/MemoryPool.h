#pragma once
// 只包含需要的头文件
#include "ThreadCache.h"
#include "logger.h"

namespace llt_memoryPool
{

class MemoryPool
{
public:
    static void* allocate(size_t size)
    {
        LogDebug("[MemoryPool:allocate] 分配内存请求，大小: " + std::to_string(size) + " 字节");
        void* ptr = ThreadCache::getInstance()->allocate(size);
        LogDebug("[MemoryPool:allocate] 内存分配完成，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)));
        return ptr;
    }

    static void deallocate(void* ptr, size_t size)
    {
        LogDebug("[MemoryPool:deallocate] 释放内存请求，地址: " + std::to_string(reinterpret_cast<uintptr_t>(ptr)) + "，大小: " + std::to_string(size) + " 字节");
        ThreadCache::getInstance()->deallocate(ptr, size);
    }
    
    // 设置日志级别
    static void setLogLevel(LogLevel level)
    {
        SetLogLevel(level);
    }
    
    // 启用或禁用DEBUG日志
    static void enableDebugLogs(bool enabled)
    {
        EnableDebugLogs(enabled);
    }
    
    // 完全禁用所有日志（性能模式）
    static void disableAllLogs()
    {
        SetLogLevel(LogLevel::ERROR);  // 只保留ERROR级别
        EnableDebugLogs(false);        // 禁用DEBUG日志
    }
    
    // 恢复默认日志设置（正常模式）
    static void restoreDefaultLogs()
    {
        SetLogLevel(LogLevel::INFO);   // 默认INFO级别
        EnableDebugLogs(false);        // 默认禁用DEBUG日志
    }
};

} // namespace llt_memoryPool