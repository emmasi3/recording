#pragma once

#include "libav_h.h"
#include <memory>
#include <string>

namespace streamer {

/// <summary>
/// 模块作用：统一管理 FFmpeg 资源生命周期。
/// 用途：通过 RAII 封装避免 format/codec/frame 上下文泄漏。
/// </summary>

/// <summary>AVFormatContext 释放器。</summary>
struct AVFormatContextDeleter {
    /// <summary>释放 format 上下文。</summary>
    void operator()(AVFormatContext* p) const noexcept;
};

/// <summary>AVCodecContext 释放器。</summary>
struct AVCodecContextDeleter {
    /// <summary>释放 codec 上下文。</summary>
    void operator()(AVCodecContext* p) const noexcept;
};

/// <summary>AVFrame 释放器。</summary>
struct AVFrameDeleter {
    /// <summary>释放 frame 对象。</summary>
    void operator()(AVFrame* p) const noexcept;
};

/// <summary>Format 上下文智能指针别名。</summary>
using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
/// <summary>Codec 上下文智能指针别名。</summary>
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
/// <summary>Frame 智能指针别名。</summary>
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;


/// <summary>
/// FFmpeg 统一上下文管理器。
/// </summary>
class FFmpegContextManager {
public:
    /**
     * @brief 初始化 FFmpeg 全局组件
     *
     * @return 是否初始化成功
     */
    bool Init();

    /**
     * @brief 释放 FFmpeg 全局资源
     */
    void Shutdown();

    /**
     * @brief 创建 format 上下文
     *
     * @return format 上下文智能指针
     */
    AVFormatContextPtr CreateFormatContext();

    /**
     * @brief 创建 codec 上下文
     *
     * @return codec 上下文智能指针
     */
    AVCodecContextPtr CreateCodecContext();

    /**
     * @brief 创建 frame 对象
     *
     * @return frame 智能指针
     */
    AVFramePtr CreateFrame();
};

/// FFmpeg 管理器共享指针别名（也可以使用 `FFmpegContextManager::ptr`）。
using FFmpegContextManagerPtr = std::shared_ptr<FFmpegContextManager>;

} // namespace streamer
