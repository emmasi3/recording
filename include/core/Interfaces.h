#pragma once

#include "core/IFrame.h"
#include "core/PacketWrapper.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace streamer {

struct AppContext;

/// <summary>
/// 模块作用：定义采集、编码、封装、推流、管线节点统一抽象。
/// 用途：保证设备层、编码层、输出层可替换。
/// </summary>

/// <summary>
/// 管线节点生命周期接口。
/// </summary>
class IPipelineNode {
public:
    typedef std::shared_ptr<IPipelineNode> ptr;
    /**
     * @brief 虚析构
     */
    virtual ~IPipelineNode() = default;

    /**
     * @brief 节点初始化
     *
     * @param ctx 应用上下文
     * @return 是否初始化成功
     */
    virtual bool Init(AppContext& ctx) = 0;

    /**
     * @brief 启动节点处理
     *
     * @return 是否启动成功
     */
    virtual bool Start() = 0;

    /**
     * @brief 停止节点处理
     */
    virtual void Stop() = 0;

    /**
     * @brief 释放节点资源
     */
    virtual void Release() = 0;

    /**
     * @brief 返回节点名称
     *
     * @return 节点名称字符串
     */
    virtual std::string Name() const = 0;
};

/// <summary>
/// 屏幕采集接口。
/// </summary>
class IScreenCapture {
public:
    typedef std::shared_ptr<IScreenCapture> ptr;

    IScreenCapture(int fps = 25) : m_fps(fps) {}

    /**
     * @brief 虚析构
     */
    virtual ~IScreenCapture() = default;

    /**
    * @brief 打开采集设备
    *
    * @return 是否打开成功
    */
    virtual bool Open() = 0;

    /**
    * @brief 不应使用该方法，请使用 ReadFrame()读取，该方法由生产者线程调用
    * @param i 帧索引
    * @return 视频帧对象；失败或无数据可返回空
    */
    virtual FramePtr ReadFrame(int i) = 0;

    /*
    * @brief 从视频队列中读取一帧，阻塞读取
    * @return FramePtr 视频帧封装
    */
    virtual FramePtr ReadFrame() = 0;

    /**
    * @brief 关闭采集设备
    */
    virtual void Close() = 0;

    virtual bool isPass(int i, bool timeout = false) = 0;

    void setFps(int fps) noexcept { m_fps = fps; }
    int getFps() const { return m_fps; }

protected:
    int m_fps;
};

/// <summary>
/// 音频采集接口。
/// </summary>
class IAudioCapture {
public:
    typedef std::shared_ptr<IAudioCapture> ptr;

    IAudioCapture() : m_index(-1), m_FmtCtx(nullptr), m_swrCtx(nullptr){}

    /**
     * @brief 虚析构
     */
    virtual ~IAudioCapture() = default;

    /**
     * @brief 打开音频输入
     *
     * @return 是否打开成功
     */
    virtual bool Open() = 0;

    /**
     * @brief 读取一帧音频
     *
     * @return 音频帧对象；失败或无数据可返回空
     */
    virtual FramePtr ReadFrame() = 0;

    /**
     * @brief 关闭音频输入
     */
    virtual void Close() = 0;

    AVFormatContext* getFmtCtx() const { return m_FmtCtx; }

    /*
    * @brief 获取设备音频流id
    */
    int getIndex() const { return m_index; }

    /*
    * @brief 获取重采样上下文
    */
    SwrContext* get_swrCtx() const { return m_swrCtx; }

protected:
    AVFormatContext* m_FmtCtx;
    // 设备音频流 id（独属于设备，和输出上下文那个不是一回事，别混了昂）
    int m_index;
    // 重采样上下文
    SwrContext* m_swrCtx;
};

/// <summary>
/// 编码器接口。
/// </summary>
class IEncoder {
public:
    typedef std::shared_ptr<IEncoder> ptr;
    IEncoder() : m_ctx(nullptr), m_encode_frame_index(0) {}

    /**
     * @brief 虚析构
     */
    virtual ~IEncoder() = default;

    /**
     * @brief 打开编码器
     *
     * @return 是否打开成功
     */
    virtual bool Open() = 0;

    /**
     * @brief 对输入帧执行编码
     *
     * @param frame 输入原始帧
     * @return 编码包集合（可能一帧产生多个包）
     */
    virtual std::vector<PacketWrapperPtr> Encode(const FramePtr& frame) = 0;

    /**
    * @brief 对输入帧执行编码
    *
    * @param frame 输入原始帧
    * @param func 处理 av_receive_packet 接收到的最新一帧数据包的回调函数，要求: int(AVPacket*)
    * @return 0 success
    * @return < 0 Error
    */
    virtual int Encode(const FramePtr& frame, std::function<int(AVPacket*)> func) = 0;

    /**
    * @brief 刷新编码器，拿出剩余的帧
    *
    * @param func 处理 av_receive_packet 接收到的最新一帧数据包的回调函数，要求: int(AVPacket*)
    * @return 0 success
    * @return < 0 Error
    */
    virtual int Flush(std::function<int(AVPacket*)> func) = 0;

    /**
     * @brief 关闭编码器
     */
    virtual void Close() = 0;

    AVCodecContext* getCtx() const { return m_ctx; }

    /*
    * @brief 仅用于查看，必须在类内(基类 or 派生类)修改
    */
    const uint64_t get_encode_frame_index() const { return m_encode_frame_index; }

protected:
    // 编码器上下文
    AVCodecContext* m_ctx;
    // 送进编码器的帧索引(递增)
    uint64_t m_encode_frame_index;
};

/// <summary>
/// 解码器接口。
/// </summary>
class IDecoder {
public:
    typedef std::shared_ptr<IDecoder> ptr;

    IDecoder() : m_ctx(nullptr), m_decode_frame_index(0) {}

    /**
     * @brief 虚析构
     */
    virtual ~IDecoder() = default;

    /**
    * @brief 打开解码器
    *
    * @return 是否打开成功
    */
    virtual bool Open() = 0;

    /**
    * @brief 对输入帧执行解码
    *
    * @param packet 输入编码帧
    * @return 解码包集合（可能一帧产生多个包）
    */
    virtual std::vector<FramePtr> Decode(const PacketWrapper& packet) = 0;

    /**
    * @brief 对输入帧执行解码
    *
    * @param frame 输入原始帧
    * @param func 处理 av_read_frame 接收到的最新一帧数据包的回调函数，要求: int(AVFrame*)
    * @return 0 success
    * @return < 0 Error
    */
    virtual int Decode(const FramePtr& frame, std::function<int(AVPacket*)> func) = 0;

    /**
    * @brief 刷新解码器，拿出剩余的帧
    *
    * @param func 处理 av_read_frame 接收到的最新一帧数据包的回调函数，要求: int(AVFrame*)
    * @return 0 success
    * @return < 0 Error
    */
    virtual int Flush(std::function<int(AVPacket*)> func) = 0;

    /**
     * @brief 关闭编码器
     */
    virtual void Close() = 0;

    AVCodecContext* getCtx() const { return m_ctx; }

    /*
    * @brief 仅用于查看，必须在类内(基类 or 派生类)修改
    */
    const uint64_t get_decode_frame_index() const { return m_decode_frame_index; }

protected:
    // 解码器上下文
    AVCodecContext* m_ctx;
    // 送进解码器的帧索引(递增)
    uint64_t m_decode_frame_index;
};

/// <summary>
/// 封装器接口。
/// </summary>
class IMuxer {
public:
    typedef std::shared_ptr<IMuxer> ptr;

    IMuxer() : m_ctx(nullptr) {}

    /**
     * @brief 虚析构
     */
    virtual ~IMuxer() = default;

    /**
     * @brief 打开封装上下文
     *
     * @return 是否打开成功
     */
    virtual bool Open() = 0;

    /**
     * @brief 写入一个编码包
     *
     * @param packet 输入编码包
     * @return 写入是否成功
     */
    virtual bool WritePacket(const PacketWrapperPtr& packet) = 0;

    /**
    * @brief 写入一个编码包
    *
    * @param packet 输入编码包
    * @return 写入是否成功
    */
    virtual bool WritePacket(AVPacket* packet) = 0;

    /**
     * @brief 关闭封装上下文
     */
    virtual void Close() = 0;

protected:
    AVFormatContext* m_ctx;
};

/// <summary>
/// 输出推流接口。
/// </summary>
class IStreamer {
public:
    typedef std::shared_ptr<IStreamer> ptr;
    /**
     * @brief 虚析构
     */
    virtual ~IStreamer() = default;

    /**
     * @brief 建立输出连接
     *
     * @return 连接是否成功
     */
    virtual bool Connect() = 0;

    /**
     * @brief 发送一个编码包
     *
     * @param packet 输入编码包
     * @return 发送是否成功
     */
    virtual bool SendPacket(const PacketWrapperPtr& packet) = 0;

    /**
     * @brief 断开输出连接
     */
    virtual void Disconnect() = 0;
};

} // namespace streamer
