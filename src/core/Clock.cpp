#include "core/Clock.h"

#include <chrono>
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Info));

namespace
{
    /*
    * windows 平台的精度改变封装（用于游戏直播、录屏录播···实时要求较高、帧率要求较高的场景，并且性能消耗也挺大）
    * 1、一般来说，windows的时间精度一般为 15 - 20ms，也就是 sleep_for(100ms) 之后，如果在 99 ms 错过一次唤醒，
    *   那么下次的唤醒最早也在 15 ms后，也就是 115 ms才会被唤醒，那么这个误差就有点打了，所以使用 timeBeginPeriod(1)
    *   方法，请求 1ms 的精度，也就是每 1ms 唤醒一次睡眠事件，嗯嗯，这样子 sleep_for(100ms)，那么唤醒的时间就应该是
    *   100 + 1ms 的高精度了
    */
    struct HighResolutionTimerGuard
    {
        HighResolutionTimerGuard(int ms, bool enable = false)
            :m_ms(ms),
            m_enable(enable)
        {
            if (m_ms > 0 && m_ms <= 20)
            {
                if (m_enable == true)
                {
                    MMRESULT r = timeBeginPeriod(m_ms);
                    // TIMERR_NOERROR == 0 表示成功
                    m_enable = (r == TIMERR_NOERROR);
                }
            }
        }

        ~HighResolutionTimerGuard()
        {
            if (m_ms > 0 && m_ms <= 20)
            {
                if (m_enable == true)
                {
                    MMRESULT r = timeEndPeriod(m_ms);
                    LOG_DEBUG(g_logger) << "timerEndPeriod(m_ms) return: " << 
                        ((r == TIMERR_NOERROR) ? std::string("true") : std::string("false"));
                }
            }
        }

        int m_ms;
        bool m_enable;
    };

    // 匿名空间初始化 windows 平台时间基准
    static HighResolutionTimerGuard __init_windwos_period(1, true);
}

namespace streamer {

int64_t SteadyTimestampProvider::NowUs() const {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}


int64_t SteadyTimestampProvider::NowMs(bool is_update) const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t PassthroughClockSync::NormalizePts(int64_t rawPtsUs, MediaType) {
    return rawPtsUs;
}


// QPCTimestampProvider
QPCTimestampProvider::QPCTimestampProvider()
{
    // 初始化 QPC
    getFrequencys();
}

int64_t QPCTimestampProvider::NowUs() const
{
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now)) {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }
    if (QPCTimestampProvider::Frequency <= 0) return 0;
    // Use double for intermediate calculation to avoid nonstandard types on MSVC
    double ticks = static_cast<double>(now.QuadPart);
    double freq = static_cast<double>(QPCTimestampProvider::Frequency);
    int64_t us = static_cast<int64_t>((ticks * 1000000.0) / freq);
    return us;
}

int64_t QPCTimestampProvider::NowMs(bool is_update) const
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    // 时间戳间隔
    int64_t delta_ticks = now.QuadPart - QPCTimestampProvider::PreviousTs;
    // 时间间隔 us
    int64_t delta_ms = (delta_ticks * 1000) / QPCTimestampProvider::Frequency;
    // 更新时间戳，判断是否要更新
    if(is_update)
        QPCTimestampProvider::PreviousTs = now.QuadPart;

    return delta_ms;
}

void QPCTimestampProvider::getFrequencys() noexcept
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    QPCTimestampProvider::Frequency = freq.QuadPart;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    QPCTimestampProvider::PreviousTs = now.QuadPart;
}

} // namespace streamer
