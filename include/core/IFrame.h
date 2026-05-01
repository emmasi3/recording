#pragma once

#include "core/MediaTypes.h"
#include "core/FrameWrapper.h"

#include <memory>

namespace streamer {

/// <summary>
/// 模块作用：定义统一帧抽象。
/// 用途：解耦采集、预处理、编码节点之间的数据载体。
/// </summary>
class IFrame {
public:
    typedef std::shared_ptr<IFrame> ptr;
    /**
     * @brief 虚析构，确保多态删除安全
     */
    virtual ~IFrame() = default;

    /**
     * @brief 获取帧元信息
     *
     * @return 只读元信息引用
     */
    virtual const FrameMeta& Meta() const = 0;

    /**
     * @brief 获取帧数据缓存
     *
     * @return 封装的底层帧对象
     */
    virtual FrameWrapperPtr Buffer() const = 0;
};

/// <summary>
/// 默认原始帧实现。
/// </summary>
class RawFrame final : public IFrame {
public:
    /// <summary>
    /// 构造原始帧。
    /// </summary>
    /// <param name="meta">帧元信息。</param>
    /// <param name="data">帧二进制数据。</param>
    RawFrame(FrameMeta meta, FrameWrapperPtr frame)
        : meta_(std::move(meta)), frame_(std::move(frame)) 
    {
    }

    /// <summary>返回帧元信息。</summary>
    const FrameMeta& Meta() const override { return meta_; }

    /// <summary>返回帧数据缓存。</summary>
    FrameWrapperPtr Buffer() const override { return frame_; }

private:
    /// <summary>帧元数据。</summary>
    FrameMeta meta_{};
    /// <summary>底层帧封装（AVFrame）。</summary>
    FrameWrapperPtr frame_{nullptr};
};

/// 帧对象共享指针别名（也可以使用 `IFrame::ptr`）。
using FramePtr = std::shared_ptr<IFrame>;

} // namespace streamer
