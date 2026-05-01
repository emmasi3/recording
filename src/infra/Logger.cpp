#include "infra/Logger.h"

#include <iostream>

extern "C"
{
#include <libavutil/log.h>
}

// 匿名空间初始化 log 全局等级
namespace
{
    struct av_log_init
    {
        av_log_init()
        {
            av_log_set_level(AV_LOG_DEBUG);
        }
    };

    static av_log_init __av_log_init;
}

namespace streamer {

    // ILogger
    ILogger::ILogger(LogLevel level)
        :m_level(level)
    {
    }

    // ConsoleLogger
    ConsoleLogger::ConsoleLogger(LogLevel level)
        :ILogger(level)
    {
    }

    void ConsoleLogger::Log(LogLevel level, const std::string& msg) {
        int av_level = AV_LOG_DEBUG;

        switch (level)
        {
        case LogLevel::Debug:
            av_level = AV_LOG_DEBUG;
            break;
        case LogLevel::Info:
            av_level = AV_LOG_INFO;
            break;
        case LogLevel::Warn:
            av_level = AV_LOG_WARNING;
            break;
        case LogLevel::Error:
            av_level = AV_LOG_ERROR;
            break;
        default:
            av_level = AV_LOG_DEBUG;
            break;
        }

        av_log(NULL, av_level, "%s\n", msg.c_str());
    }

    // LogEvent
    LogEvent::LogEvent(ILogger::ptr logger, LogLevel level, const std::string& filename, int32_t line)
        :m_logger(logger),
        m_level(level)
    {
        m_ss << "filename:" << filename << "  " << "line:" << line << "    ";
    }

    LogEvent::~LogEvent()
    {
        // 析构时直接给到日志器的log方法
        m_logger->Log(m_level, m_ss.str());
    }

    std::stringstream& LogEvent::getSS()
    {
        return m_ss;
    }

    const std::string LogEvent::LeveltoString()
    {
        std::stringstream ss;
        ss.str("");
        switch (m_level)
        {
        case LogLevel::Debug:
            ss << "DEBUG";
            break;
        case LogLevel::Info:
            ss << "INFO";
            break;
        case LogLevel::Warn:
            ss << "WARNING";
            break;
        case LogLevel::Error:
            ss << "ERROR";
            break;
        default:
            ss << "DEBUG";
            break;
        }

        return ss.str();
    }



    // LogEventWarp
    LogEventWarp::LogEventWarp(LogEvent::ptr e)
        :m_event(e)
    {
    }

    std::stringstream& LogEventWarp::getSS()
    {
        return m_event->getSS();
    }

} // namespace streamer
