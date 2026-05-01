#pragma once

#include "core/Clock.h"
#include "core/ErrorPolicy.h"
#include "config/Config.h"
#include "infra/FfmpegContextManager.h"
#include "infra/Logger.h"

#include <memory>

namespace streamer {

/// <summary>
/// 模块作用：集中管理全局依赖。
/// 用途：在各节点间传递配置、日志、时钟、策略和 FFmpeg 上下文。
/// </summary>
struct AppContext {
    /**
     * @brief 日志接口
     */
    LoggerPtr logger;

    /**
     * @brief 配置提供者接口
     */
    ConfigProviderPtr configProvider;

    /**
     * @brief 时间戳提供器
     */
    std::shared_ptr<ITimestampProvider> timestampProvider;

    /**
     * @brief A/V 时钟同步器
     */
    std::shared_ptr<IClockSync> clockSync;

    /**
     * @brief 错误处理策略
     */
    std::shared_ptr<IErrorPolicy> errorPolicy;

    /**
     * @brief 重连退避策略
     */
    std::shared_ptr<IReconnectPolicy> reconnectPolicy;

    /**
     * @brief FFmpeg 统一上下文管理器
     */
    FFmpegContextManagerPtr ffmpeg;
};

} // namespace streamer
