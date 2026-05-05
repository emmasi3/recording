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

    RtmpMuxer(const std::string& url = std::string{});
    virtual ~RtmpMuxer() noexcept;

    static IMuxer::ptr createNew(const std::string& url = std::string{});
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

    const std::string& getUrl() const { return m_url; }
    void setUrl(const std::string& url) { m_url = url; }
    uint64_t get_video_index() const { return m_vIndex; }
    uint64_t get_audio_index() const { return m_aIndex; }
    IEncoder::ptr get_video_encoder() const { return m_vEncoder; }
    IEncoder::ptr get_audio_encoder() const { return m_aEncoder; }

private:
    std::string m_url;
    bool m_close;
    IEncoder::ptr m_vEncoder;
    IEncoder::ptr m_aEncoder;
    uint64_t m_vIndex;
    uint64_t m_aIndex;
    
    struct AVFormatContext* m_ctx = nullptr;
};

/// <summary>
/// 模块作用：RTMP 推流器骨架。
/// 用途：负责建立网络连接并发送封装后的媒体包。
/// </summary>
class RtmpStreamer final : public IStreamer 
{
public:
    typedef std::shared_ptr<RtmpStreamer> ptr;

    RtmpStreamer(const std::string& url = std::string{});
    virtual ~RtmpStreamer();

    static IStreamer::ptr createNew(const std::string& url = std::string{});

    void SetMuxer(RtmpMuxer::ptr muxer) { m_muxer = muxer; }
    void SetVideoEncoder(IEncoder::ptr vEncoder) { m_vEncoder = vEncoder; }
    void SetAudioEncoder(IEncoder::ptr aEncoder) { m_aEncoder = aEncoder; }

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

    const std::string get_url() const { return m_url; }

private:
    void MuxThreadProc();
    bool send_MuxThreadProc_to_threads();

private:
    IMuxer::ptr m_muxer;
    const std::string m_url;
    IEncoder::ptr m_vEncoder;
    IEncoder::ptr m_aEncoder;
    int64_t m_last_video_pts = 0;
    int64_t m_last_audio_pts = 0;
};

} // namespace streamer
