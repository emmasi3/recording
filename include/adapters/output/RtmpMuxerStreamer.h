#pragma once

#include "core/Interfaces.h"

namespace streamer {

/// <summary>
/// 模块作用：RTMP 封装器骨架。
/// 用途：负责将编码包写入目标容器（如 FLV over RTMP）。
/// </summary>
class RtmpMuxer final : public IMuxer {
public:
    typedef std::shared_ptr<RtmpMuxer> ptr;
    /**
     * @brief 打开封装上下文
     *
     * @return 是否打开成功
     */
    bool Open() override;

    /**
     * @brief 写入一个编码包
     *
     * @param packet 输入编码包
     * @return 写入是否成功
     */
    bool WritePacket(const PacketWrapperPtr& packet) override;

    bool WritePacket(AVPacket* packet) override;

    /**
     * @brief 关闭封装上下文
     */
    void Close() override;
};

/// <summary>
/// 模块作用：RTMP 推流器骨架。
/// 用途：负责建立网络连接并发送封装后的媒体包。
/// </summary>
class RtmpStreamer final : public IStreamer {
public:
    typedef std::shared_ptr<RtmpStreamer> ptr;
    /**
     * @brief 连接 RTMP 服务端
     *
     * @return 是否连接成功
     */
    bool Connect() override;

    /**
     * @brief 发送一个编码包
     *
     * @param packet 输入编码包
     * @return 发送是否成功
     */
    bool SendPacket(const PacketWrapperPtr& packet) override;

    /**
     * @brief 断开 RTMP 连接
     */
    void Disconnect() override;
};

} // namespace streamer
