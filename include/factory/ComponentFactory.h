#pragma once

#include "core/Interfaces.h"

#include <memory>
#include <string>

namespace streamer {

/// <summary>
/// 模块作用：组件创建入口。
/// 用途：按类型字符串装配可替换实现，避免业务层直接依赖具体类。
/// </summary>
class ComponentFactory {
public:
    /// <summary>
    /// 创建屏幕采集器。
    /// </summary>
    /// <param name="type">采集器类型（如 dxgi）。</param>
    /// <returns>采集器实例；不支持时返回空。</returns>
    static IScreenCapture::ptr CreateScreenCapture(const std::string& type);

    /**
    * @brief 创建音频采集器
    *
    * @param type 采集器类型（如 dshow）
    * @return 采集器实例；不支持时返回空
    */
    static IAudioCapture::ptr CreateAudioCapture(const std::string& type);

    /**
    * @brief 创建编码器
    *
    * @param type 编码器类型（如 ffmpeg/x264/nvenc）
    * @return 编码器实例；不支持时返回空
    */
    static IEncoder::ptr CreateEncoder(const std::string& type);

    /**
    * @brief 创建封装器
    *
    * @param type 封装器类型（如 rtmp/flv）
    * @return 封装器实例；不支持时返回空
    */
    static IMuxer::ptr CreateMuxer(const std::string& type);

    /**
    * @brief 创建推流器
    *
    * @param type 推流类型（如 rtmp/srt）
    * @return 推流器实例；不支持时返回空
    */
    static IStreamer::ptr CreateStreamer(const std::string& type);
};

} // namespace streamer
