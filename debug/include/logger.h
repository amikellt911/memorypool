#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <atomic>
#include <thread>
#include <sstream>

namespace llt_memoryPool
{
    // 日志级别枚举
    enum class LogLevel {
        ERROR = 0,  // 最高级别
        WARN = 1,
        INFO = 2,
        DEBUG = 3   // 最低级别
    };

    class Logger
    {
    private:
        std::ofstream logFile;
        std::string logFilePath;
        std::string logFileName;
        std::string logFileTime;
        std::atomic_flag logFileLock;
        LogLevel minLogLevel{LogLevel::INFO}; // 默认只输出INFO及以上级别的日志
        bool enableDebugLogs{false};         // 是否启用DEBUG日志的开关
    
    private:
        std::string getCurrentTime();
        
    public:
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        static Logger& getInstance() {
            static Logger instance;
            return instance;
        }
        
        Logger(const std::string& filePath="../build/log/") {
            logFileTime = getCurrentTime();
            logFilePath = filePath;
            logFileName = "log_" + logFileTime + ".txt";
            logFile.open(logFilePath + logFileName, std::ios::app);
            if (!logFile.is_open()) {
                std::cerr << "Failed to open log file: " << logFileName << std::endl;
            }
        }
        
        ~Logger() {
            logFile.close();
        }

        // 设置最小日志级别
        void setMinLogLevel(LogLevel level) {
            minLogLevel = level;
        }
        
        // 获取当前最小日志级别
        LogLevel getMinLogLevel() const {
            return minLogLevel;
        }
        
        // 启用或禁用DEBUG日志
        void setDebugLogsEnabled(bool enabled) {
            enableDebugLogs = enabled;
        }
        
        // 获取DEBUG日志启用状态
        bool isDebugLogsEnabled() const {
            return enableDebugLogs;
        }
        
        // 判断给定级别是否应该被记录
        bool shouldLog(LogLevel level) const {
            // DEBUG级别特殊处理
            if (level == LogLevel::DEBUG && !enableDebugLogs) {
                return false;
            }
            return level <= minLogLevel;
        }
        
        // 基本日志记录函数
        void log(LogLevel level, const std::string& message, bool isThread = true);
        
        // 便捷日志记录方法
        void info(const std::string& message, bool isThread = true) {
            if (shouldLog(LogLevel::INFO)) {
                log(LogLevel::INFO, message, isThread);
            }
        }
        
        void warn(const std::string& message, bool isThread = true) {
            if (shouldLog(LogLevel::WARN)) {
                log(LogLevel::WARN, message, isThread);
            }
        }
        
        void error(const std::string& message, bool isThread = true) {
            if (shouldLog(LogLevel::ERROR)) {
                log(LogLevel::ERROR, message, isThread);
            }
        }
        
        void debug(const std::string& message, bool isThread = true) {
            if (shouldLog(LogLevel::DEBUG)) {
                log(LogLevel::DEBUG, message, isThread);
            }
        }
    };

    // 全局函数式API，替代原宏定义
    inline void LogInfo(const std::string& message) {
        Logger::getInstance().info(message);
    }
    
    inline void LogWarn(const std::string& message) {
        Logger::getInstance().warn(message);
    }
    
    inline void LogError(const std::string& message) {
        Logger::getInstance().error(message);
    }
    
    inline void LogDebug(const std::string& message) {
        Logger::getInstance().debug(message);
    }
    
    // 设置日志级别的全局函数
    inline void SetLogLevel(LogLevel level) {
        Logger::getInstance().setMinLogLevel(level);
    }
    
    // 启用或禁用DEBUG日志的全局函数
    inline void EnableDebugLogs(bool enabled) {
        Logger::getInstance().setDebugLogsEnabled(enabled);
    }
    
} // namespace llt_memoryPool