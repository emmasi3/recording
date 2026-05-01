#pragma once

#include <cstdint>
#include <memory>
extern "C"
{
    struct AVPacket;
    struct AVRational;
}

namespace streamer {


/// <summary>
/// 模块作用：统一封装编码输出包。
/// 用途：在业务层隔离 FFmpeg AVPacket 生命周期。
/// </summary>

/// <summary>
/// AVPacket 智能释放器。
/// </summary>
struct AVPacketDeleter {
    /// <summary>释放 AVPacket 资源。</summary>
    void operator()(AVPacket* pkt) const noexcept;
};

/// <summary>编码包 RAII 包装对象。</summary>
class PacketWrapper {
public:
    typedef std::shared_ptr<PacketWrapper> ptr;
    /// <summary>底层包智能指针类型。</summary>
    using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

    /// <summary>默认构造。</summary>
    PacketWrapper() = default;
    /// <summary>使用底层包构造。explicit -- 禁止隐式转换</summary>
    explicit PacketWrapper(PacketPtr pkt);

    /// <summary>移动构造。</summary>
    PacketWrapper(PacketWrapper&&) noexcept = default;
    /// <summary>移动赋值。</summary>
    PacketWrapper& operator=(PacketWrapper&&) noexcept = default;

    /// <summary>禁用拷贝构造。</summary>
    PacketWrapper(const PacketWrapper&) = delete;
    /// <summary>禁用拷贝赋值。</summary>
    PacketWrapper& operator=(const PacketWrapper&) = delete;

    /// <summary>返回底层包裸指针（不转移所有权）。</summary>
    AVPacket* Get() const noexcept { return m_pkt.get(); }

    /// <summary>获取 PTS（ms）。</summary>
    int64_t PtsMs() const noexcept { return m_pts; }
    /// <summary>获取 DTS（ms）。</summary>
    int64_t DtsMs() const noexcept { return m_dts; }
    /// <summary>设置 PTS / DTS（ms）。</summary>
    void SetPtsDtsUs(int64_t pts, int64_t dts);

public:
    /*
    * @brief 在送入编码器之前转换时间戳
    */
    void Packet_rescale_ts(AVRational src, AVRational dst);

    // 释放引用计数
    void Pakcet_unref();

private:
    /// <summary>底层包资源。在该类中用 unique 接管，在AVPacketDeleter删除器中释放，不用操心</summary>
    PacketPtr m_pkt{nullptr};
    /// <summary>显示时间戳（ms）。需要和 AVPacket 中的时间戳同步处理</summary>
    int64_t m_pts{0};
    /// <summary>解码时间戳（ms）。</summary>
    int64_t m_dts{0};
};

/// 共享包指针别名（也可以使用 `PacketWrapper::ptr`）。
using PacketWrapperPtr = std::shared_ptr<PacketWrapper>;

} // namespace streamer
