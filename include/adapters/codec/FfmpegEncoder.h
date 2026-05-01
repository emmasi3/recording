#pragma once

#include "core/Interfaces.h"
#include "infra/Logger.h"

#include <Windows.h>

namespace streamer {

    /// <summary>
    /// 模块作用：FFmpeg 编码器适配器骨架。
    /// 用途：将原始音视频帧编码为可封装的压缩包。
    /// </summary>
    class VideoFfmpegEncoder final : public IEncoder 
    {
    public:
        typedef std::shared_ptr<VideoFfmpegEncoder> ptr;

        VideoFfmpegEncoder() = delete;

        VideoFfmpegEncoder(int out_stream_index, AVRational out_time_base);

        virtual ~VideoFfmpegEncoder();

        static IEncoder::ptr createNew(int out_stream_index = 0, AVRational out_time_base = { 1, 25 });

        /**
         * @brief 打开编码器资源
         *
         * @return 是否打开成功
         */
        bool Open() override;

        /**
         * @brief 编码输入帧并返回编码包列表
         *
         * @param frame 输入原始帧
         * @return 编码包集合
         */
        std::vector<PacketWrapperPtr> Encode(const FramePtr& frame) override;

        /**
        * @brief 对输入帧执行编码
        *
        * @param frame 输入原始帧
        * @param func 处理 av_receive_packet 接收到的最新一帧数据包的回调函数，要求: int(AVPacket*)
        * @return 0 success
        * @return < 0 Error
        */
        virtual int Encode(const FramePtr& frame, std::function<int(AVPacket*)> func) override;

        virtual int Flush(std::function<int(AVPacket*)> func) override;

        /**
         * @brief 关闭编码器资源
         */
        void Close() override;

        IScreenCapture::ptr getDxgiCap() const { return m_dxgiCap; }

        /*
        * @brief 修正输出上下文的视频流时间基
        */
        void set_vOut_time_base(AVRational out_time_base) { m_out_time_base = out_time_base; }

        /*
        * @brief 获取输出上下文的视频流时间基
        */
        const AVRational get_vOut_time_base() const { return m_out_time_base; }

    private:
        /*
        * @brief 设置硬件帧池大小、字段，该方法仅在 InitNVENCEncoder() 初始化编码器内调用，用户不应调用
        */
        int InitHWFrames_D3D11(
            AVBufferRef* hw_device_ctx,
            int width,
            int height);

        /*
        * @brief 初始化编码器各种资源
        */
        int InitNVENCEncoder(
            int width = GetSystemMetrics(SM_CXSCREEN),
            int height = GetSystemMetrics(SM_CYSCREEN),
            int fps = 25);


        IScreenCapture::ptr m_dxgiCap;
        // 仅在 Encode(const FramePtr& frame, std::function<int(AVPacket*)> func) override 中使用
        AVPacket* m_pkt;
        int m_out_stream_index;
        AVRational m_out_time_base;
    };

    // 音频编码器
    class AudioFfmpegEncoder final : public IEncoder
    {
    public:
        typedef std::shared_ptr<AudioFfmpegEncoder> ptr;

        /*
        * @param audio_codec_id 输出上下文默认的音频编码器id
        * @param time_base 输出上下文的音频时间基
        * @param aOutIndex 输出上下文的音频流id
        */
        AudioFfmpegEncoder(AVCodecID audio_codec_id, AVRational time_base, uint64_t aOutIndex);

        virtual ~AudioFfmpegEncoder();

        static IEncoder::ptr createNew(AVCodecID audio_codec_id, AVRational time_base, uint64_t aOutIndex);

        /**
        * @brief 打开编码器资源
        *
        * @return 是否打开成功
        */
        bool Open() override;

        /**
        * @brief 编码输入帧并返回编码包列表
        *
        * @param frame 输入原始帧
        * @return 编码包集合
        */
        std::vector<PacketWrapperPtr> Encode(const FramePtr& frame) override;

        /**
        * @brief 对输入帧执行编码
        *
        * @param frame 输入原始帧
        * @param func 处理 av_receive_packet 接收到的最新一帧数据包的回调函数，要求: int(AVPacket*)
        * @return 0 success
        * @return < 0 Error
        */
        virtual int Encode(const FramePtr& frame, std::function<int(AVPacket*)> func) override;

        virtual int Flush(std::function<int(AVPacket*)> func) override;

        /**
        * @brief 关闭编码器资源
        */
        void Close() override;

        /*
        * @brief 获取编码器设置的比特率
        */
        uint64_t get_audioBitrate() const { return m_audioBitrate; }

        /*
        * @brief 获取编码器要求的采样点数
        */
        uint64_t get_nbSamples() const { return m_nbSamples; }

        /*
        * @brief 修改得到的输出上下文时间基
        */
        void set_aOut_time_base(AVRational time_base) { m_aOut_time_base = time_base; }

        /*
        * @brief 获取输出上下文的视频流时间基
        */
        const AVRational get_aOut_time_base() const { return m_aOut_time_base; }

        /*
        * @brief 获取组件智能指针 -- 音频录制组件
        */
        IAudioCapture::ptr get_audioCap_shared() const { return m_audioCap; }

    private:
        IAudioCapture::ptr m_audioCap;
        uint64_t m_audioBitrate;
        // 采样点数(per channel) -- 编码器要求
        uint64_t m_nbSamples;
        // 用于接收编码器输出的 m_pkt，在构造函数中初始化并分配内存，Close() 释放;
        AVPacket* m_pkt;

        // 输出上下文的编码器ID，在默认的 libfdk_aac 无法使用时，使用媒体文件给出的，由 FfpmegMuxer 提供
        AVCodecID m_audio_codec_id;
        // 音频流id(输出上下文)，由 FfpmegMuxer 提供
        uint64_t m_aOutIndex;
        // 输出上下文音频流时间戳，由 FfpmegMuxer 提供
        AVRational m_aOut_time_base;
    };

} // namespace streamer
