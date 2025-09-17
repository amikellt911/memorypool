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

   
        // 基本日志记录函数
        void log(LogLevel level, const std::string& message, bool isThread = true);
        
        // 便捷日志记录方法
        void info(const std::string& message, bool isThread = true) {
                log(LogLevel::INFO, message, isThread);

        }
        
        void warn(const std::string& message, bool isThread = true) {
                log(LogLevel::WARN, message, isThread);
        }
        
        void error(const std::string& message, bool isThread = true) {
                log(LogLevel::ERROR, message, isThread);
        }
        
        void debug(const std::string& message, bool isThread = true) {
                log(LogLevel::DEBUG, message, isThread);
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
    
    
    
} // namespace llt_memoryPool