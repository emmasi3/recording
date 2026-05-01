#include "adapters/codec/FfmpegDecoder.h"
#include "infra/Logger.h"

namespace streamer
{
	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

	FfmpegDecoder::FfmpegDecoder(AVCodecParameters* codecpar)
		:m_codecpar(codecpar)
	{

	}

	FfmpegDecoder::~FfmpegDecoder()
	{
		Close();
	}

	IDecoder::ptr FfmpegDecoder::createNew(AVCodecParameters* codecpar)
	{
		IDecoder::ptr ptr = std::make_shared<FfmpegDecoder>(codecpar);

		// 打开编码器
		if (!ptr->Open())
		{
			LOG_ERROR(g_logger) << "FfmpegDecoder::Open() failed";
			return nullptr;
		}

		return ptr;
	}

	bool FfmpegDecoder::Open()
	{
		int ret = 0;
		const AVCodec* codec = nullptr;

		//确定解码器
		codec = avcodec_find_decoder(m_codecpar->codec_id);
		if (!codec)
		{
			LOG_ERROR(g_logger) << "the codec of mkfeng is failed!";
			return false;
		}

		//分配解码器上下文
		m_ctx = avcodec_alloc_context3(codec);
		if (!m_ctx)
		{
			LOG_ERROR(g_logger) << "the codecctx of mkfeng is failed!";
			return false;
		}

		//填充解码上下文的，解码器参数
		ret = avcodec_parameters_to_context(m_ctx, m_codecpar);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "the codecctx of mkfeng is failed!";
			return false;
		}

		//打开解码器
		if (avcodec_open2(m_ctx, codec, nullptr) < 0)
		{
			LOG_ERROR(g_logger) << "Failed to open audioDevice! avcodec_open2()";
			return false;
		}

		return true;
	}

	std::vector<FramePtr> FfmpegDecoder::Decode(const PacketWrapper& packet)
	{
		return std::vector<FramePtr>();
	}

	int FfmpegDecoder::Decode(const FramePtr& frame, std::function<int(AVPacket*)> func)
	{
		return 0;
	}

	int FfmpegDecoder::Flush(std::function<int(AVPacket*)> func)
	{
		return 0;
	}

	void FfmpegDecoder::Close()
	{

	}
}