#pragma once

#include <cstdint>
#include <string>


namespace streamer {

/// <summary>
/// 模块作用：定义媒体域中的基础类型。
/// 用途：为采集、编码、封装、推流全链路提供统一数据描述与错误语义。
/// </summary>

/// <summary>
/// 媒体类型标识。
/// </summary>
enum class MediaType {
    /// <summary>视频流。</summary>
    Video = 0,
    /// <summary>音频流。</summary>
    Audio = 1,
};

/// <summary>
/// 原始视频像素格式。
/// </summary>
enum class PixelFormat {
    /// <summary>未知格式。</summary>
    Unknown,
    /// <summary>BGRA 32bit 格式。</summary>
    BGRA,
    /// <summary>NV12 半平面格式。</summary>
    NV12,
    /// <summary>YUV420P 平面格式。</summary>
    YUV420P
};

/// <summary>
/// 原始音频采样格式。
/// </summary>
enum class SampleFormat {
    /// <summary>未知格式。</summary>
    Unknown,
    /// <summary>有符号 16 位整型。</summary>
    S16,
    /// <summary>浮点平面格式。</summary>
    FLTP
};

/// <summary>
/// 原始帧元信息。
/// </summary>
struct FrameMeta {
    /// <summary>当前帧所属媒体类型。</summary>
    MediaType mediaType{MediaType::Video};
    /// <summary>显示时间戳（微秒）。</summary>
    int64_t ptsUs{0};
    /// <summary>解码时间戳（微秒）。</summary>
    int64_t dtsUs{0};
    /// <summary>视频宽度（像素）。</summary>
    int width{0};
    /// <summary>视频高度（像素）。</summary>
    int height{0};
    /// <summary>音频采样率（Hz）。</summary>
    int sampleRate{0};
    /// <summary>音频通道数。</summary>
    int channels{0};
    /// <summary>视频像素格式。</summary>
    PixelFormat pixelFormat{PixelFormat::Unknown};
    /// <summary>音频采样格式。</summary>
    SampleFormat sampleFormat{SampleFormat::Unknown};
};

/// <summary>
/// 错误码定义。
/// </summary>
enum class ErrorCode {
    /// <summary>无错误。</summary>
    None,
    /// <summary>初始化失败。</summary>
    InitFailed,
    /// <summary>设备打开失败。</summary>
    DeviceOpenFailed,
    /// <summary>编码失败。</summary>
    EncodeFailed,
    /// <summary>封装失败。</summary>
    MuxFailed,
    /// <summary>网络错误。</summary>
    NetworkFailed,
    /// <summary>状态非法。</summary>
    InvalidState,
    /// <summary>未知错误。</summary>
    Unknown
};

/// <summary>
/// 结构化错误信息。
/// </summary>
struct ErrorInfo {
    /// <summary>错误码。</summary>
    ErrorCode code{ErrorCode::Unknown};
    /// <summary>错误描述文本。</summary>
    std::string message;
    /// <summary>错误来源组件名。</summary>
    std::string component;
};

/// <summary>
/// 错误处理动作。
/// </summary>
enum class ErrorAction {
    /// <summary>忽略错误并继续。</summary>
    Ignore,
    /// <summary>执行重试。</summary>
    Retry,
    /// <summary>停止管线。</summary>
    Stop
};

// FFMpeg 错误码对应字符串 strerror封装
class AVStrError
{
public:
    /*
    * @brief ffmpeg -- avstrerror() 方法封装
    */
    static int strerror(int errnum, char* errbuf, size_t errbuf_size);
    /*
    * @brief 返回 ffmpge -- AV_MAX_ERROR_STRING_SIZE; 宏
    */
    static size_t maxErrorStringSize() noexcept;
};

#define AV_ERROR_MAX_STRING_SIZE streamer::AVStrError::maxErrorStringSize()

} // namespace streamer
