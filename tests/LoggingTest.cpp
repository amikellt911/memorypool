#include <iostream>
#include "../include/logger.h"
#include "../include/LogMacros.h"

int main() {
    std::cout << "==== 日志系统测试 ====" << std::endl;
    
    // 输出ENABLE_DEBUG_LOGS的值
    std::cout << "ENABLE_DEBUG_LOGS = " << ENABLE_DEBUG_LOGS << std::endl;
    
    // 设置日志级别测试
    std::cout << "初始日志级别: " << static_cast<int>(llt_memoryPool::Logger::getInstance().getMinLogLevel()) << std::endl;
    
    // 测试各种日志级别
    LOG_INFO("这是INFO级别日志");
    LOG_WARN("这是WARN级别日志");
    LOG_ERROR("这是ERROR级别日志");
    LOG_DEBUG("如果看不到这条消息，说明DEBUG日志已被禁用");
    
    // 测试日志级别过滤
    std::cout << "设置日志级别为ERROR..." << std::endl;
    SET_LOG_LEVEL(llt_memoryPool::LogLevel::ERROR);
    std::cout << "当前日志级别: " << static_cast<int>(llt_memoryPool::Logger::getInstance().getMinLogLevel()) << std::endl;
    
    LOG_INFO("【级别过滤】INFO级别消息应该被过滤掉");
    LOG_WARN("【级别过滤】WARN级别消息应该被过滤掉");
    LOG_ERROR("【级别过滤】ERROR级别消息应该显示");
    LOG_DEBUG("【级别过滤】DEBUG级别消息应该被过滤掉");
    
    std::cout << "==== 测试完成 ====" << std::endl;
    return 0;
} 