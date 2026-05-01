#pragma once

#include "core/MediaTypes.h"
#include "factory/singleton.h"
#include "infra/Logger.h"

#include <cstdint>
#include <memory>

namespace streamer {

/// <summary>
/// 模块作用：提供统一时间戳来源与 A/V 时间基对齐能力。
/// 用途：确保采集、编码、封装阶段时间戳一致性。
/// </summary>

/// <summary>
/// 时间戳提供者接口。
/// </summary>
class ITimestampProvider {
public:
    typedef std::shared_ptr<ITimestampProvider> ptr;
    /**
     * @brief 虚析构，保证多态安全释放
     */
    virtual ~ITimestampProvider() = default;

    /**
     * @brief 获取当前单调时间戳（微秒）
     *
     * @return 当前时间戳（微秒）
     */
    virtual int64_t NowUs() const = 0;

    /**
    * @brief 获取当前单调时间戳（ms），主要是担心 us 的话，时间戳可能会溢出（针对 QPC）
    *
    * @return 当前时间戳（ms）
    */
    virtual int64_t NowMs(bool is_update = true) const = 0;
};

/// <summary>
/// 基于 steady_clock 的时间戳实现。
/// </summary>
class SteadyTimestampProvider final : public ITimestampProvider {
public:
    /**
     * @brief 返回单调时钟时间戳（微秒）
     *
     * @return 单调时钟时间戳（微秒）
     */
    int64_t NowUs() const override;
    /**
     * @brief 返回单调时钟时间戳（毫秒）
     *
     * @return 单调时钟时间戳（毫秒）
     */
    int64_t NowMs(bool is_update = true) const override;
};

/// <summary>
/// 时钟同步接口。
/// </summary>
class IClockSync {
public:
    typedef std::shared_ptr<IClockSync> ptr;
    /**
     * @brief 虚析构
     */
    virtual ~IClockSync() = default;

    /**
     * @brief 对输入时间戳进行归一化
     *
     * @param rawPtsUs 原始时间戳（微秒）
     * @param type 媒体类型
     * @return 归一化后的时间戳（微秒）
     */
    virtual int64_t NormalizePts(int64_t rawPtsUs, MediaType type) = 0;
};


/// <summary>
/// 透传同步器（不改写时间戳）。
/// </summary>
class PassthroughClockSync final : public IClockSync {
public:
    /**
     * @brief 直接返回输入时间戳（透传，不改写）
     *
     * @param rawPtsUs 原始时间戳（微秒）
     * @param type 媒体类型
     * @return 传入的原始时间戳（微秒）
     */
    int64_t NormalizePts(int64_t rawPtsUs, MediaType type) override;
};

/*
* @brief 获取高精度时间戳，QPC::GetInstance()->NowUs() 即可
*/
class QPCTimestampProvider final : public ITimestampProvider
{
public:
    typedef std::shared_ptr<QPCTimestampProvider> ptr;
    QPCTimestampProvider();
    /**
    * @brief 获取当前单调时间戳（微秒）
    * @return 当前时间戳（微秒）
    */
    virtual int64_t NowUs() const override;
    /**
    * @brief 获取当前单调时间戳（ms）
    * @return 当前时间戳（ms）
    */
    virtual int64_t NowMs(bool is_update = true) const override;
    /**
    * @brief 获取当前 QPC 的频率
    * @return 频率
    */
    int64_t getFrequency() const { return QPCTimestampProvider::Frequency; }
    /**
    * @brief 获取上一次记录的计数器数字，一般没什么用
    * @return QPCTimestampProvider::PreviousTs
    */
    int64_t getPreviousTs() const { return QPCTimestampProvider::PreviousTs; }
private:
    /*
    * @brief 初始化QPC频率
    */
    void getFrequencys() noexcept;
private:
    // QPC 计时器频率
    static inline int64_t Frequency = -1;
    // QPC 上一次记录数字
    static inline int64_t PreviousTs = 0;
};

// 单例模式 QPC::GetInstance()->···
typedef Singleton<QPCTimestampProvider> QPC;

} // namespace streamer
