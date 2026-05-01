#pragma once

#include "core/MediaTypes.h"

#include <memory>
#include <string>
#include <sstream>

namespace streamer {

/// <summary>
/// 模块作用：提供统一日志抽象。
/// 用途：隔离日志输出实现，便于后续接入文件/ETW/第三方日志系统。
/// </summary>

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : \
                      (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__))

#define LOG_LEVEL(level, logger)\
    if(level >= logger->getLevel())\
        streamer::LogEventWarp(streamer::LogEvent::ptr(new streamer::LogEvent(logger, level, __FILENAME__, __LINE__))).getSS()

#define LOG_DEBUG(logger) LOG_LEVEL(streamer::LogLevel::Debug, logger)
#define LOG_INFO(logger) LOG_LEVEL(streamer::LogLevel::Info, logger)
#define LOG_WARN(logger) LOG_LEVEL(streamer::LogLevel::Warn, logger)
#define LOG_ERROR(logger) LOG_LEVEL(streamer::LogLevel::Error, logger)


/// <summary>
/// 日志等级。
/// </summary>
enum class LogLevel {
    /// <summary>追踪级别。</summary>
    Trace,
    /// <summary>调试级别。</summary>
    Debug,
    /// <summary>信息级别。</summary>
    Info,
    /// <summary>告警级别。</summary>
    Warn,
    /// <summary>错误级别。</summary>
    Error
};

/// <summary>
/// 日志接口。
/// </summary>
class ILogger {
public:
    typedef std::shared_ptr<ILogger> ptr;

    ILogger(LogLevel level);
    /**
     * @brief 虚析构
     *
     * @note 保证派生类析构函数能够正确被调用
     */
    virtual ~ILogger() = default;

    /**
     * @brief 记录日志
     *
     * @param level 日志等级
     * @param msg 日志内容
     *
     * @return void
     */
    virtual void Log(LogLevel level, const std::string& msg) = 0;

    const LogLevel& getLevel() const { return m_level; }
protected:
    LogLevel m_level;
};

/// <summary>
/// 控制台日志实现。
/// </summary>
class ConsoleLogger final : public ILogger {
public:
    typedef std::shared_ptr<ConsoleLogger> ptr;

    ConsoleLogger(LogLevel level);

    /**
     * @brief 输出日志到控制台
     *
     * @param level 日志等级
     * @param msg 日志内容
     *
     * @return void
     */
    void Log(LogLevel level, const std::string& msg) override;

};

// 使用 `ILogger::ptr` 作为共享指针别名，避免重复别名定义
/// 日志对象共享指针别名（兼容旧代码）。
using LoggerPtr = std::shared_ptr<ILogger>;

// 日志事件
class LogEvent
{
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(ILogger::ptr logger,
        LogLevel level,
        const std::string& filename,
        int32_t line);

    ~LogEvent();

    // 获取字符流引用
    std::stringstream& getSS();
    // 获取事件级别
    const std::string& getLevel() { return LeveltoString(); }
    // 日志级别 toString
    const std::string LeveltoString();

private:
    // 接收信息的字符流
    std::stringstream m_ss;
    // 对应的logger
    ILogger::ptr m_logger;
    // 事件日志级别
    LogLevel m_level;
};


// 日志事件管理器
class LogEventWarp
{
public:
    typedef std::shared_ptr<LogEventWarp> ptr;

    LogEventWarp(LogEvent::ptr e);
    std::stringstream& getSS();
private:
    LogEvent::ptr m_event;
};



} // namespace streamer
