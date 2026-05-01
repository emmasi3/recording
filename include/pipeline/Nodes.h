#pragma once

#include "concurrency/ConcurrentQueue.h"
#include "core/Interfaces.h"
#include "pipeline/PipelineNodeBase.h"

#include <memory>

namespace streamer {

/// <summary>
/// 模块作用：定义采集、编码、推流节点骨架。
/// 用途：以节点方式组成可编排媒体处理管线。
/// </summary>

/// <summary>
/// 屏幕采集节点。
/// </summary>
class ScreenCaptureNode final : public PipelineNodeBase {
public:
    /**
     * @brief 构造屏幕采集节点
     *
     * @param cap 屏幕采集实现
     * @param out 输出帧队列
     */
    ScreenCaptureNode(std::unique_ptr<IScreenCapture> cap, std::shared_ptr<IConcurrentQueue<FramePtr>> out);

    /**
     * @brief 返回节点名称
     */
    std::string Name() const override { return "ScreenCaptureNode"; }

protected:
    /**
     * @brief 打开采集资源
     */
    bool OnInit() override;

    /**
     * @brief 关闭采集资源
     */
    void OnStop() override;

    /**
     * @brief 释放节点资源
     */
    void OnRelease() override;

    /**
     * @brief 循环读取屏幕帧并输出
     */
    void ProcessLoop() override;

private:
    /// <summary>屏幕采集器实例。</summary>
    std::unique_ptr<IScreenCapture> cap_;
    /// <summary>输出帧队列。</summary>
    std::shared_ptr<IConcurrentQueue<FramePtr>> out_;
};

/// <summary>
/// 音频采集节点。
/// </summary>
class AudioCaptureNode final : public PipelineNodeBase {
public:
    /**
     * @brief 构造音频采集节点
     *
     * @param cap 音频采集实现
     * @param out 输出帧队列
     */
    AudioCaptureNode(std::unique_ptr<IAudioCapture> cap, std::shared_ptr<IConcurrentQueue<FramePtr>> out);

    /**
     * @brief 返回节点名称
     */
    std::string Name() const override { return "AudioCaptureNode"; }

protected:
    /**
     * @brief 打开采集资源
     */
    bool OnInit() override;

    /**
     * @brief 关闭采集资源
     */
    void OnStop() override;

    /**
     * @brief 释放节点资源
     */
    void OnRelease() override;

    /**
     * @brief 循环读取音频帧并输出
     */
    void ProcessLoop() override;

private:
    /// <summary>音频采集器实例。</summary>
    std::unique_ptr<IAudioCapture> cap_;
    /// <summary>输出帧队列。</summary>
    std::shared_ptr<IConcurrentQueue<FramePtr>> out_;
};

/// <summary>
/// 编码节点。
/// </summary>
class EncodeNode final : public PipelineNodeBase {
public:
    /**
     * @brief 构造编码节点
     *
     * @param encoder 编码器实现
     * @param in 输入帧队列
     * @param out 输出包队列
     */
    EncodeNode(
        std::unique_ptr<IEncoder> encoder,
        std::shared_ptr<IConcurrentQueue<FramePtr>> in,
        std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> out);

    /**
     * @brief 返回节点名称
     */
    std::string Name() const override { return "EncodeNode"; }

protected:
    /**
     * @brief 打开编码器
     */
    bool OnInit() override;

    /**
     * @brief 关闭编码器
     */
    void OnStop() override;

    /**
     * @brief 释放节点资源
     */
    void OnRelease() override;

    /**
     * @brief 循环消费帧并输出编码包
     */
    void ProcessLoop() override;

private:
    /// <summary>编码器实例。</summary>
    std::unique_ptr<IEncoder> encoder_;
    /// <summary>输入原始帧队列。</summary>
    std::shared_ptr<IConcurrentQueue<FramePtr>> in_;
    /// <summary>输出编码包队列。</summary>
    std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> out_;
};

/// <summary>
/// 推流节点。
/// </summary>
class StreamNode final : public PipelineNodeBase {
public:
    /**
     * @brief 构造推流节点
     *
     * @param muxer 封装器实现
     * @param streamer 输出推流实现
     * @param in 输入包队列
     */
    StreamNode(
        std::unique_ptr<IMuxer> muxer,
        std::unique_ptr<IStreamer> streamer,
        std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> in);

    /**
     * @brief 返回节点名称
     */
    std::string Name() const override { return "StreamNode"; }

protected:
    /**
     * @brief 打开封装器并建立连接
     */
    bool OnInit() override;

    /**
     * @brief 停止输入并关闭连接
     */
    void OnStop() override;

    /**
     * @brief 释放节点资源
     */
    void OnRelease() override;

    /**
     * @brief 循环写包并发送
     */
    void ProcessLoop() override;

private:
    /// <summary>封装器实例。</summary>
    std::unique_ptr<IMuxer> muxer_;
    /// <summary>推流器实例。</summary>
    std::unique_ptr<IStreamer> streamer_;
    /// <summary>输入编码包队列。</summary>
    std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> in_;
};

} // namespace streamer
