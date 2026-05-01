#pragma once
#include "core/Interfaces.h"
#include "libav_h.h"

namespace streamer
{
	class FfmpegDecoder final : public IDecoder
	{
	public:
		typedef std::shared_ptr<FfmpegDecoder> ptr;

		FfmpegDecoder(AVCodecParameters* codecpar);

		virtual ~FfmpegDecoder();

		static IDecoder::ptr createNew(AVCodecParameters* codecpar);

        /**
        * @brief 打开解码器
        *
        * @return 是否打开成功
        */
        virtual bool Open() override;

        /**
        * @brief 对输入帧执行解码
        *
        * @param packet 输入编码帧
        * @return 解码包集合（可能一帧产生多个包）
        */
        virtual std::vector<FramePtr> Decode(const PacketWrapper& packet) override;

        /**
        * @brief 对输入帧执行解码
        *
        * @param frame 输入原始帧
        * @param func 处理 av_read_frame 接收到的最新一帧数据包的回调函数，要求: int(AVFrame*)
        * @return 0 success
        * @return < 0 Error
        */
        virtual int Decode(const FramePtr& frame, std::function<int(AVPacket*)> func) override;

        /**
        * @brief 刷新解码器，拿出剩余的帧
        *
        * @param func 处理 av_read_frame 接收到的最新一帧数据包的回调函数，要求: int(AVFrame*)
        * @return 0 success
        * @return < 0 Error
        */
        virtual int Flush(std::function<int(AVPacket*)> func) override;

        /**
        * @brief 关闭编码器
        */
        virtual void Close() override;

	private:
        /*
        * @brief 外部传递给该解码器用来初始化的参数(ffmpeg)，在此项目中由 DshowAudioCapture::init_audioDecoder() 提供
        */
        AVCodecParameters* m_codecpar;
	};


}