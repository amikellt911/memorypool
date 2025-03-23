#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <atomic>
#include <thread>
namespace llt_memoryPool
{
    enum class LogLevel{
        INFO,
        WARN,
        ERROR,
        DEBUG
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
            void log(LogLevel level, const std::string& message);
    };
}