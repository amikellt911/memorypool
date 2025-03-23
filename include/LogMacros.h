#pragma once

#include "../include/logger.h"

// 只有在没有通过编译选项定义ENABLE_DEBUG_LOGS时才使用这个默认值
#ifndef ENABLE_DEBUG_LOGS
    #define ENABLE_DEBUG_LOGS 1
#endif

// 确保ENABLE_DEBUG_LOGS是一个有效的值（0或1）
#if !defined(ENABLE_DEBUG_LOGS) || (ENABLE_DEBUG_LOGS != 0 && ENABLE_DEBUG_LOGS != 1)
    #undef ENABLE_DEBUG_LOGS
    #define ENABLE_DEBUG_LOGS 1  // 默认为启用
#endif

// 基本日志宏，总是启用
#define LOG_INFO(message) llt_memoryPool::Logger::getInstance().info(message)
#define LOG_WARN(message) llt_memoryPool::Logger::getInstance().warn(message)
#define LOG_ERROR(message) llt_memoryPool::Logger::getInstance().error(message)

// DEBUG日志宏，根据ENABLE_DEBUG_LOGS决定是否启用
#if ENABLE_DEBUG_LOGS
    #define LOG_DEBUG(message) llt_memoryPool::Logger::getInstance().debug(message)
#else
    #define LOG_DEBUG(message) ((void)0) // 不执行任何操作
#endif

// 带线程ID的日志宏
#define LOG_INFO_T(message) llt_memoryPool::Logger::getInstance().info(message, true)
#define LOG_WARN_T(message) llt_memoryPool::Logger::getInstance().warn(message, true)
#define LOG_ERROR_T(message) llt_memoryPool::Logger::getInstance().error(message, true)

#if ENABLE_DEBUG_LOGS
    #define LOG_DEBUG_T(message) llt_memoryPool::Logger::getInstance().debug(message, true)
#else
    #define LOG_DEBUG_T(message) ((void)0)
#endif

// 设置日志级别的宏
#define SET_LOG_LEVEL(level) llt_memoryPool::Logger::getInstance().setMinLogLevel(level)

namespace llt_memoryPool
{
    // 这里保留命名空间，但将所有宏定义都移到外部
    // 可以在此处添加命名空间内的其他非宏定义内容
}