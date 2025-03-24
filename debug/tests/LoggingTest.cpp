#include <iostream>
#include "../include/logger.h"

// 获取日志状态的辅助函数
bool GetDebugLogsEnabled() {
    return llt_memoryPool::Logger::getInstance().isDebugLogsEnabled();
}

// 获取日志级别的辅助函数
llt_memoryPool::LogLevel GetLogLevel() {
    return llt_memoryPool::Logger::getInstance().getMinLogLevel();
}

int main() {
    std::cout << "==== 日志系统测试 ====" << std::endl;
    
    // 测试DEBUG日志开关
    std::cout << "初始DEBUG日志状态: " << (GetDebugLogsEnabled() ? "启用" : "禁用") << std::endl;
    std::cout << "初始日志级别: " << static_cast<int>(GetLogLevel()) << std::endl;
    
    // 测试各种日志级别
    llt_memoryPool::LogInfo("这是INFO级别日志");
    llt_memoryPool::LogWarn("这是WARN级别日志");
    llt_memoryPool::LogError("这是ERROR级别日志");
    llt_memoryPool::LogDebug("如果看不到这条消息，说明DEBUG日志默认禁用");
    
    // 启用DEBUG日志
    std::cout << "启用DEBUG日志..." << std::endl;
    llt_memoryPool::EnableDebugLogs(true);
    std::cout << "当前DEBUG日志状态: " << (GetDebugLogsEnabled() ? "启用" : "禁用") << std::endl;
    
    llt_memoryPool::LogDebug("现在应该可以看到DEBUG日志了");
    
    // 测试日志级别过滤
    std::cout << "设置日志级别为ERROR..." << std::endl;
    llt_memoryPool::SetLogLevel(llt_memoryPool::LogLevel::ERROR);
    std::cout << "当前日志级别: " << static_cast<int>(GetLogLevel()) << std::endl;
    
    llt_memoryPool::LogInfo("【级别过滤】INFO级别消息应该被过滤掉");
    llt_memoryPool::LogWarn("【级别过滤】WARN级别消息应该被过滤掉");
    llt_memoryPool::LogError("【级别过滤】ERROR级别消息应该显示");
    llt_memoryPool::LogDebug("【级别过滤】DEBUG级别消息应该被过滤掉，即使已启用");
    
    // 禁用DEBUG日志
    std::cout << "禁用DEBUG日志..." << std::endl;
    llt_memoryPool::EnableDebugLogs(false);
    std::cout << "当前DEBUG日志状态: " << (GetDebugLogsEnabled() ? "启用" : "禁用") << std::endl;
    
    // 恢复日志级别为INFO
    llt_memoryPool::SetLogLevel(llt_memoryPool::LogLevel::INFO);
    std::cout << "当前日志级别: " << static_cast<int>(GetLogLevel()) << std::endl;
    
    llt_memoryPool::LogInfo("INFO日志应该显示");
    llt_memoryPool::LogDebug("DEBUG日志应该被过滤掉，因为已禁用");
    
    std::cout << "==== 测试完成 ====" << std::endl;
    return 0;
} 