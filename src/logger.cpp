#include "../include/logger.h"

namespace llt_memoryPool
{
    std::string Logger::getCurrentTime(){
        std::time_t now = std::time(nullptr);
        std::string timeStr = std::ctime(&now);
        if(!timeStr.empty()&&timeStr.back() == '\n'){
            timeStr.pop_back();
        }
        return timeStr;
    }
    void Logger::log(LogLevel level, const std::string& message, bool isThread){
        if (!shouldLog(level)) {
            return;
        }
        
        while(logFileLock.test_and_set(std::memory_order_acquire)){
            std::this_thread::yield();
        }
        if(!logFile.is_open()){
            std::cerr << "Log file is not open" << std::endl;
            logFileLock.clear(std::memory_order_release);
            return;
        }
        std::string levelStr;
        switch(level){
            case LogLevel::INFO:
                levelStr = "INFO";
                break;
            case LogLevel::WARN:
                levelStr = "WARN";
                break;
            case LogLevel::ERROR:
                levelStr = "ERROR";
                break;
            case LogLevel::DEBUG:
                levelStr = "DEBUG";
                break;
        }
        std::string threadId = "";
        if(isThread){
           std::stringstream ss;
           ss << "ThreadID:[" << std::this_thread::get_id() << "] ";
           threadId = ss.str();
        }

        std::string logMessage =" [" + levelStr + "] " + " : [" + getCurrentTime() + "] : " + (isThread ? threadId : "") + message + "\n";
        std::cout << logMessage;
        logFile << logMessage;
        logFile.flush();
        logFileLock.clear(std::memory_order_release);
    }
}