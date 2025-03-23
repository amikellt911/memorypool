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
    enum class LogLevel{
        ERROR,
        WARN,
        INFO,
        DEBUG,
    };

    class Logger
    {
        private:
            std::ofstream logFile;
            std::string logFilePath;
            std::string logFileName;
            std::string logFileTime;
            std::atomic_flag logFileLock;
            LogLevel minLogLevel{LogLevel::DEBUG}; // 默认输出所有级别的日志
        private:
            std::string getCurrentTime();
        public:
            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;
            static Logger& getInstance(){
                static Logger instance;
                return instance;
            }
            Logger(const std::string& filePath="../build/log/")
            {
                logFileTime = getCurrentTime();
                logFilePath = filePath;
                logFileName = "log_" + logFileTime + ".txt";
                logFile.open(logFilePath + logFileName,std::ios::app);
                if(!logFile.is_open()){
                    std::cerr << "Failed to open log file: " << logFileName << std::endl;
                }
            }
            ~Logger(){
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
            
            // 判断给定级别是否应该被记录
            bool shouldLog(LogLevel level) const {
                // 注意：LogLevel的定义中，Error=0, Warn=1, Info=2, Debug=3
                // 所以较小的值表示更高的优先级
                return level <= minLogLevel;
            }
            
            void log(LogLevel level, const std::string& message, bool isThread = true);
            
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
}