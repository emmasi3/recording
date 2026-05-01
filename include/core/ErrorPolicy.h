#pragma once

#include "core/MediaTypes.h"

#include <cstdint>

namespace streamer {

/// <summary>
/// 模块作用：定义错误处理与重连退避策略抽象。
/// 用途：在编排层实现可替换的恢复逻辑。
/// </summary>

/// <summary>
/// 错误处理策略接口。
/// </summary>
class IErrorPolicy {
public:
    /// <summary>虚析构。</summary>
    virtual ~IErrorPolicy() = default;

    /// <summary>
    /// 根据错误信息返回处理动作。
    /// </summary>
    /// <param name="error">结构化错误信息。</param>
    /// <returns>错误处理动作。</returns>
    virtual ErrorAction OnError(const ErrorInfo& error) = 0;
};

/// <summary>
/// 重连策略接口。
/// </summary>
class IReconnectPolicy {
public:
    /// <summary>虚析构。</summary>
    virtual ~IReconnectPolicy() = default;

    /// <summary>
    /// 计算下次重连等待时间。
    /// </summary>
    /// <returns>等待毫秒数。</returns>
    virtual uint32_t NextBackoffMs() = 0;

    /// <summary>
    /// 重置策略内部状态。
    /// </summary>
    virtual void Reset() = 0;
};

/// <summary>
/// 默认错误策略实现。
/// </summary>
class DefaultErrorPolicy final : public IErrorPolicy {
public:
    /// <summary>根据错误码返回默认动作。</summary>
    ErrorAction OnError(const ErrorInfo& error) override;
};

/// <summary>
/// 指数退避重连策略。
/// </summary>
class ExponentialReconnectPolicy final : public IReconnectPolicy {
public:
    /// <summary>获取下次重连退避时长。</summary>
    uint32_t NextBackoffMs() override;

    /// <summary>重置为初始退避时长。</summary>
    void Reset() override;

private:
    /// <summary>当前退避时长（毫秒）。</summary>
    uint32_t currentMs_{500};
    /// <summary>最大退避时长（毫秒）。</summary>
    uint32_t maxMs_{8000};
};

} // namespace streamer
