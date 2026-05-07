#pragma once
#include "core/Interfaces.h"
#include "libav_h.h"
#include "adapters/codec/FfmpegDecoder.h"
#include "concurrency/ConcurrentQueue.h"

namespace streamer {

    /// <summary>
    /// 模块作用：dshow 音频采集适配器骨架。
    /// 用途：实现默认输入设备采集并输出原始音频帧。
    /// </summary>
    class DshowAudioCapture final : public IAudioCapture 
    {
    public:
        typedef std::shared_ptr<DshowAudioCapture> ptr;

        DshowAudioCapture(IEncoder::ptr encoder);

        virtual ~DshowAudioCapture();

        static IAudioCapture::ptr createNew(IEncoder::ptr encoder);
        /**
        * @brief 打开 dshow 音频设备
        *
        * @return 是否打开成功
        */
        bool Open() override;

        /**
        * @brief 读取一帧音频数据
        *
        * @return 音频帧对象；无数据时可返回空，但不一定，也有可能阻塞
        */
        FramePtr ReadFrame() override;

        /**
        * @brief 关闭 dshow 音频设备
        */
        void Close() override;

        /*
        * @brief 获取解码器上下文（用于麦克风）
        */
        AVCodecContext* get_decoder_ctx() const { return m_decoder->getCtx(); }

        /*
        * @brief 根据编码器参数分配特定的 AVFrame 帧，并返回指针
        * @param nbSamples per channel 采样点数，一般为 encodec_ctx->frame_size; 用于重采样的frame除外，需另外计算
        */
        AVFrame* AllocAudioFrame(int nbSamples);

        /*
        * @brief 加锁获取队列内数据大小(采样点数)
        */
        const uint64_t get_audio_queue_size();

        /*
        * @brief 加锁获取队列内剩余空间大小(采样点数)
        */
        const uint64_t get_audio_queue_space();

        /*
        * @brief 清空队列
        */
        bool drain_audio_fifo_size();

    private:
        /*
        * @brief 判断dev是否有采集音频的功能，也就是麦克风（在类内调用，外部不可见）
        * @return true 是
        * @return false 不是
        */
        bool is_audio_device(AVDeviceInfo* dev);

        /*
        * @brief 获取本设备第一个能够采集音频的设备名（并加上前缀 audio=），用于打开设备
        */
        const std::string get_first_dshow_audio_device_name(const AVInputFormat* iformat);

        /*
        * @brief 打开音频解码器
        * @brief 在此项目中，用来初始化音频解码器并获取器智能指针到 m_decoder 中
        */
        bool init_audio_decoder();

        /*
        * @brief 初始化重采样上下文
        */
        bool init_swrCtx();

        /*
        * @brief 初始化音频共享队列
        */
        bool init_AudioFifo(int count = 4);

        /*
        * @brief 从音频设备中循环读取、重采样、送入m_queue队列中，应该被加入 “线程队列” 中
        */
        void ReadFrameFrom_device();

        /*
        * @brief 将采集音频函数送入线程队列
        * @param bool Immediately 是否立即开启该线程函数，默认为 false，注册后统一开启
        * @return 加入是否成功？
        */
        bool send_ReadFrameFrom_device_threads(bool Immediately = false);

        /*
        * @brief 刷新音频解码器
        */
        void FlushAudioDecoder();

        // 解码器 -- 音频采集器的部件，应使用 createNew() 创建实例
        IDecoder::ptr m_decoder;

        // 阻塞队列，线程安全，存放解码来的原始音频帧 -- 部件
        IConcurrentQueue<AVFrame*>::ptr m_queue;

        // 编码器，非部件，而是一些信息的外部提供者，不应调用其 createNew() 获取，而是应该被外部提供已经初始化的实例
        IEncoder::ptr m_encoder;

        // 用于从 m_queue 中读取指定一帧的容器，其初始化在 createNew -- Open() 中，依赖于 m_encoder
        AVFrame* m_frame;

    };

} // namespace streamer
