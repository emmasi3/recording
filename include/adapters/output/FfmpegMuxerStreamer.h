#pragma once
#include "infra/Logger.h"
#include "adapters/codec/FfmpegEncoder.h"

namespace streamer
{
	/*
	* @brief 本地文件接口封装
	*/
	class LocalMuxer final : public IMuxer
	{
	public:
        typedef std::shared_ptr<LocalMuxer> ptr;

        LocalMuxer(const std::string& filename = std::string{});

        virtual ~LocalMuxer() noexcept;

        static IMuxer::ptr createNew(const std::string& filename = std::string{});

        /**
        * @brief 打开封装上下文
        * @note 内部逻辑 -- 按照用户要求，创建合适的媒体流，保证媒体文件正确性，防止情况：test.aac，结果还有视频流存在
        * @return 是否打开成功
        */
        virtual bool Open() override;

        /**
        * @brief 写入一个编码包
        *
        * @param packet 输入编码包
        * @return 写入是否成功
        */
        virtual bool WritePacket(const PacketWrapperPtr& packet) override;

        /**
        * @brief 写入一个编码包
        *
        * @param packet 输入编码包(裸指针)
        * @return 写入是否成功
        */
        virtual bool WritePacket(AVPacket* packet) override;

        /**
        * @brief 关闭封装上下文
        */
        virtual void Close() override;

        const std::string& getFileName() const { return m_filename; }
        void setFileName(const std::string& name) { m_filename = name; }
        // get输出上下文视频流id
        uint64_t get_video_index() const { return m_vIndex; }
        // get输出上下文音频流id
        uint64_t get_audio_index() const { return m_aIndex; }
        // 获取编码器--video
        IEncoder::ptr get_video_encoder() const { return m_vEncoder; }
        // 获取编码器--video
        IEncoder::ptr get_audio_encoder() const { return m_aEncoder; }

    private:
        // 本地媒体文件名(带后缀)
        std::string m_filename;
        bool m_close;

        // video编码器封装
        IEncoder::ptr m_vEncoder;
        // audio编码器封装
        IEncoder::ptr m_aEncoder;
        // 输出上下文视频流id
        uint64_t m_vIndex;
        // 输出上下文音频流id
        uint64_t m_aIndex;
	};

    /// <summary>
    /// 模块作用：本地推流器骨架。
    /// 用途：负责组织具体的消费逻辑，保证本地媒体流写入正确性
    /// </summary>
    class LocalFileStreamer final : public IStreamer 
    {
    public:
        typedef std::shared_ptr<LocalFileStreamer> ptr;

        LocalFileStreamer(const std::string& filename = std::string{});
        virtual ~LocalFileStreamer();

        static IStreamer::ptr createNew(const std::string& filename = std::string{});

        /**
         * @brief 注入必要的组件以供管线消费调度
         */
        void SetMuxer(LocalMuxer::ptr muxer) { m_muxer = muxer; }
        void SetVideoEncoder(IEncoder::ptr vEncoder) { m_vEncoder = vEncoder; }
        void SetAudioEncoder(IEncoder::ptr aEncoder) { m_aEncoder = aEncoder; }

        /**
        * @brief 连接 RTMP 服务端或启动本地文件写入流（初始化各组件并启动线程）
        *
        * @return 是否连接成功
        */
        bool Connect() override;

        /**
        * @brief 发送一个编码包（暂时可不用，本类内部自驱拉取并发送）
        */
        bool SendPacket(const PacketWrapperPtr& packet) override;

        /**
        * @brief 断开流并收尾
        */
        void Disconnect() override;

        /*
        * @brief 获取本地流文件相对路径
        */
        const std::string get_localfile_name() const { return m_filename; }

    private:
        /**
        * @brief 本地消费者核心工作线程，对标 AVRecordImpl::MuxThreadProc
        *        负责同步音视频时间戳、分别拉取并写入数据
        * @note 有简单的音视频同步策略
        */
        void MuxThreadProc();

        /*
        * @brief 将消费者任务送入线程队列中，开始消费
        * @param bool Immediately 是否立即开启该线程函数，默认为 false，注册后统一开启
        * @return true 成功，否则失败
        */
        bool send_MuxThreadProc_to_threads(bool Immediately = false);

    private:
        IMuxer::ptr m_muxer;
        const std::string m_filename;
        IEncoder::ptr m_vEncoder;
        IEncoder::ptr m_aEncoder;

        // 送入视频流上一帧时间戳
        int64_t m_last_video_pts = 0;
        // 送入音频流上一帧时间戳
        int64_t m_last_audio_pts = 0;
    };


}