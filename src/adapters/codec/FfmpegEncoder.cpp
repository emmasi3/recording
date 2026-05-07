#include "adapters/codec/FfmpegEncoder.h"
#include "adapters/capture/DxgiScreenCapture.h"
#include "adapters/capture/DshowAudioCapture.h"

namespace streamer 
{

	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

	VideoFfmpegEncoder::VideoFfmpegEncoder(int out_stream_index, AVRational out_time_base)
		:m_dxgiCap(nullptr),
		m_pkt(nullptr),
		m_out_stream_index(out_stream_index),
		m_out_time_base(out_time_base)
	{
		m_dxgiCap = DxgiScreenCapture::createNew(m_out_time_base.den);
		// 分配私有 AVPacket 包
		m_pkt = av_packet_alloc();
	}

	VideoFfmpegEncoder::~VideoFfmpegEncoder()
	{
		Close();
	}

	IEncoder::ptr VideoFfmpegEncoder::createNew(int out_stream_index, AVRational out_time_base)
	{
		IEncoder::ptr ptr = std::make_shared<VideoFfmpegEncoder>(out_stream_index, out_time_base);
		// 加这个判断，目的: 我不确定它调用的是构造函数是默认还是有参的，你可能说：禁用它的默认构造不就行了
		// 昂，std::make_shared() 创建的是一个智能指针，它的指针可以为 nullptr, 也就是它的构造函数列表可以不填东西
		// 这和要接管的资源对象 delete 禁用构造函数没有一点关系
		if (!ptr)
		{
			LOG_WARN(g_logger) << "std::make_shared<VideoFfmpegEncoder>() return nullptr";
			return nullptr;
		}

        VideoFfmpegEncoder::ptr Ff_ptr = std::dynamic_pointer_cast<VideoFfmpegEncoder>(ptr);
		if (!Ff_ptr)
		{
			LOG_ERROR(g_logger) << "FfmpegEncoder::createNew() -- dynamic_pointer_cast<VideoFfmpegEncoder> return nullptr!";
			return nullptr;
		}

		DxgiScreenCapture::ptr dxgi_ptr = std::dynamic_pointer_cast<DxgiScreenCapture>(Ff_ptr->m_dxgiCap);
		if (!dxgi_ptr)
		{
			LOG_ERROR(g_logger) << "FfmpegEncoder::createNew() -- dynamic_pointer_cast<DxgiScreenCapture> return nullptr!";
			return nullptr;
		}

		HwDevice_d3d11::ptr hwd3d_ptr = std::dynamic_pointer_cast<HwDevice_d3d11>(dxgi_ptr->get_hw_device());
		if (!hwd3d_ptr)
		{
          LOG_ERROR(g_logger) << "FfmpegEncoder::createNew() -- dynamic_pointer_cast<HwDevice_d3d11> return nullptr!";
			return nullptr;
		}

		// 初始化编码器
		if (!ptr->Open())
		{
			return nullptr;
		}

		// 初始化硬件帧池 -- DxgiScreenCapture::init_hwFramePool()
		int ret = dxgi_ptr->init_hwFramePool(Ff_ptr->getCtx(), hwd3d_ptr->get_TEXTURE_BUFFER_SIZE());
		if (ret < 0)
		{
			return nullptr;
		}

		return ptr;
	}

	bool VideoFfmpegEncoder::Open() {
		int ret = InitNVENCEncoder(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), m_dxgiCap->getFps());
		if (ret < 0)
		{
			return false;
		}
		return true;
	}

	std::vector<PacketWrapperPtr> VideoFfmpegEncoder::Encode(const FramePtr& frame) 
	{
		int ret = 0;
		std::vector<PacketWrapperPtr> vec;

		ret = avcodec_send_frame(m_ctx, frame->Buffer()->Get());
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "Failed to Send the frame to encoder";
			return {};
		}

		while (true)
		{
			AVPacket* pkt = nullptr;
			pkt = av_packet_alloc();
			// 分配内存后，用独享指针接管
			PacketWrapper::PacketPtr pktPtr(pkt);

			ret = avcodec_receive_packet(m_ctx, pkt);
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			{
				return std::move(vec);
			}
			else if (ret < 0)
			{
				return {};
			}

			// 设置pkt包参数
			pkt->stream_index = m_out_stream_index;
			// 转化pkt的时间戳 -- 符合mp4格式的时间基(某些格式有自己的标准)
			av_packet_rescale_ts(pkt, m_ctx->time_base,
				m_out_time_base);

           vec.emplace_back(std::make_shared<PacketWrapper>(std::move(pktPtr)));
		}

		return std::move(vec);
	}

	int VideoFfmpegEncoder::Encode(const FramePtr& frame, std::function<int(AVPacket*)> func)
	{
		int ret = 0;
		char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		// 检查原始帧，并更新原始帧索引
		if (!frame)
		{
			LOG_ERROR(g_logger) << "Rawframe is nullptr";
			return -1;
		}
		// 计算原始帧 pts
		if(frame->Buffer()->Get())
			frame->Buffer()->Get()->pts = av_rescale_q(m_encode_frame_index++, { 1, m_ctx->framerate.num }, m_ctx->time_base);

		// 送入编码器
		ret = avcodec_send_frame(m_ctx, frame->Buffer()->Get());
		if (ret < 0)
		{
			av_strerror(ret, errbuf, sizeof(errbuf));
			LOG_ERROR(g_logger) << "avcodec_send_frame() failure, ret: " << ret << " errbuf: " << errbuf;
			return ret;
		}

		do
		{
			// 循环接收AVPacket包
			ret = avcodec_receive_packet(m_ctx, m_pkt);
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret < 0)
			{
				LOG_ERROR(g_logger) << "avcodec_receive_packet() return: " << ret;
				return ret;
			}

			// 处理 pkt 参数
			m_pkt->stream_index = m_out_stream_index;
			av_packet_rescale_ts(m_pkt, m_ctx->time_base,
				m_out_time_base);

			// 放入 func 处理
			ret = func(m_pkt);
			if(ret < 0)
			{
				av_packet_unref(m_pkt);
				LOG_ERROR(g_logger) << "std::function<int(AVPacket*)> -- func(m_pkt) return: " << ret;
				return ret;
			}

			av_packet_unref(m_pkt);
		} while (true);

		return 0;
	}

	int VideoFfmpegEncoder::Flush(std::function<int(AVPacket*)> func)
	{
		int ret = 0;
		char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

		// 送入编码器
		ret = avcodec_send_frame(m_ctx, nullptr);
		if (ret < 0)
		{
			av_strerror(ret, errbuf, sizeof(errbuf));
			LOG_ERROR(g_logger) << "avcodec_send_frame() failure, ret: " << ret << " errbuf: " << errbuf;
			return ret;
		}

		do
		{
			// 循环接收AVPacket包
			ret = avcodec_receive_packet(m_ctx, m_pkt);
			if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret < 0)
			{
				LOG_ERROR(g_logger) << "avcodec_receive_packet() return: " << ret;
				return ret;
			}

			// 处理 pkt 参数
			m_pkt->stream_index = m_out_stream_index;
			av_packet_rescale_ts(m_pkt, m_ctx->time_base,
				m_out_time_base);

			// 放入 func 处理
			ret = func(m_pkt);
			if (ret < 0)
			{
				av_packet_unref(m_pkt);
				LOG_ERROR(g_logger) << "std::function<int(AVPacket*)> -- func(m_pkt) return: " << ret;
				return ret;
			}

			av_packet_unref(m_pkt);
		} while (true);

		return 0;
	}

	void VideoFfmpegEncoder::Close() 
	{
		if (m_pkt)
		{
			av_packet_free(&m_pkt);
		}

		if (m_ctx)
		{
			if (m_ctx->hw_frames_ctx)
			{
				av_buffer_unref(&m_ctx->hw_frames_ctx);
			}
			avcodec_free_context(&m_ctx);
		}
	}


	int VideoFfmpegEncoder::InitNVENCEncoder(
		int width,
		int height,
		int fps)
	{
		// 获取 D3D 设备
		AVBufferRef* hw_device_ctx = std::dynamic_pointer_cast<HwDevice_d3d11>
			(std::dynamic_pointer_cast<DxgiScreenCapture>(m_dxgiCap)->get_hw_device())->get_hw_device_ctx();

		const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
		if (!codec)
		{
			LOG_ERROR(g_logger) << "h264_nvenc not Found!";
			return -1;
		}

		m_ctx = avcodec_alloc_context3(codec);
		if (!m_ctx)
		{
			LOG_ERROR(g_logger) << "avcodec_alloc_context3(codec)";
			return -1;
		}

		// 设置编码器参数（可以从其他地方来，这里选择自己写）
		m_ctx->width = width;
		m_ctx->height = height;
		m_ctx->bit_rate = 15 * 1000 * 1000;

		m_ctx->time_base = { 1, fps };
		m_ctx->framerate = { fps, 1 };

		m_ctx->gop_size = 10;
		m_ctx->max_b_frames = 0; // 特么的，直接设置为0，没什么影响！
		m_ctx->pix_fmt = AV_PIX_FMT_D3D11; // 硬件像素格式 
		m_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx); // 看官方注释

		if (codec->id == AV_CODEC_ID_H264) // 私有设置，可以没有
		{
			av_opt_set(m_ctx->priv_data, "preset", "slow", 0);
		}

		/*
		* 设置编码器上下文的标志位，若不添加该标志，会导致 Nginx-rtmp 服务器直接返回 “codec: invalid video codec header size=5”。
		* 但是对于 .h264 这种格式的文件，该选项不能够勾选，每一帧必须有单独的 header——info，否则大多数播放器无法解析 .h264 文件，
		* 但是对于大多数 封装格式 mp4、flv、mkv、mov···只需要开始有一个 header——info，就可以解析完成了，这和具体的格式有关
		*/
		m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		// 创建 D3D11 硬件帧池
		int ret = InitHWFrames_D3D11(hw_device_ctx, width, height);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "InitHWFrames_D3D11 failed!";
			return -1;
		}

		ret = avcodec_open2(m_ctx, codec, NULL);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "avcodec_open2(ctx, codec, NULL)";
			return -1;
		}

		return 0;
	}


	int VideoFfmpegEncoder::InitHWFrames_D3D11(
		AVBufferRef* hw_device_ctx,
		int width,
		int height)
	{
		// 硬件帧池大小
		int TEXTURE_BUFFER_SIZE = std::dynamic_pointer_cast<HwDevice_d3d11>
			(std::dynamic_pointer_cast<DxgiScreenCapture>(m_dxgiCap)->get_hw_device())->get_TEXTURE_BUFFER_SIZE();

		int ret = 0;
		char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

		AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx);
		AVHWFramesContext* frames_ctx =
			(AVHWFramesContext*)hw_frames_ref->data;

		frames_ctx->format = AV_PIX_FMT_D3D11; // GPU Texture
		frames_ctx->sw_format = AV_PIX_FMT_NV12;  // NVENC 支持
		frames_ctx->width = width;
		frames_ctx->height = height;
		frames_ctx->initial_pool_size = TEXTURE_BUFFER_SIZE;

		// 3. 设置 D3D11 私有字段 BindFlags
		AVD3D11VAFramesContext* d3d11_ctx =
			(AVD3D11VAFramesContext*)frames_ctx->hwctx;

		d3d11_ctx->BindFlags =
			D3D11_BIND_RENDER_TARGET |   // VideoProcessor 输出必须
			D3D11_BIND_SHADER_RESOURCE;  // 编码链路可保留

		// 该方法会调用相应的 硬件Texture2D API，填充 frames_ctx 对应的帧池，在送入之前，一定要设置合理的
		// hw_frames_ref 参数，例如：D3D11的话，参数要符合 CreateTexture2D() 的参数要求
		ret = av_hwframe_ctx_init(hw_frames_ref);
		if (ret < 0)
		{
			av_strerror(ret, err_buf, sizeof(err_buf));
			av_log(NULL, AV_LOG_ERROR, "av_hwframe_ctx_init() is failed：%s\n", err_buf);
			av_buffer_unref(&hw_frames_ref);
			return -1;
		}

		// ctx->hw_device_ctx, 这个字段的解释中提到，如果用户自己设置输入帧，那么就需要设置 hw_frames_ctx，而不是hw_device_ctx字段
		m_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
		av_buffer_unref(&hw_frames_ref);

		return 0;
	}

	/////////////////// AUDIO //////////////////////


	AudioFfmpegEncoder::AudioFfmpegEncoder(AVCodecID audio_codec_id, AVRational time_base, uint64_t aOutIndex)
		:m_audioCap(nullptr),
		m_audioBitrate(192000),
		m_nbSamples(1024),
		m_audio_codec_id(audio_codec_id),
		m_pkt(nullptr),
		m_aOut_time_base(time_base),
		m_aOutIndex(aOutIndex)
	{
		{
			m_pkt = av_packet_alloc();
			if (!m_pkt)
				LOG_ERROR(g_logger) << "m_pkt -- alloc -- failed";
		}
	}

	AudioFfmpegEncoder::~AudioFfmpegEncoder()
	{
		Close();
	}

	IEncoder::ptr AudioFfmpegEncoder::createNew(AVCodecID audio_codec_id, AVRational time_base, uint64_t aOutIndex)
	{
		IEncoder::ptr ptr = std::make_shared<AudioFfmpegEncoder>(audio_codec_id, time_base, aOutIndex);

		// 打开音频编码器
		if (!ptr->Open())
		{
			return nullptr;
		}

		// 打开指定的音频原始帧提供者设备
		AudioFfmpegEncoder::ptr audio_ptr = std::dynamic_pointer_cast<AudioFfmpegEncoder>(ptr);
		audio_ptr->m_audioCap = DshowAudioCapture::createNew(ptr);

		return ptr;
	}

	bool AudioFfmpegEncoder::Open()
	{
		// 查找编码器
		const AVCodec* codec = avcodec_find_encoder_by_name("libfdk_aac");
		if (!codec)
		{
			LOG_ERROR(g_logger) << "Failed to find the encoder -- libfdk_aac!";
			codec = avcodec_find_encoder(m_audio_codec_id);
			if (!codec)
			{
				LOG_ERROR(g_logger) << "Failed to find encoder by id -- error!";
				return false;
			}
		}

		// 上下文
		m_ctx = avcodec_alloc_context3(codec);
		if (!m_ctx)
		{
			LOG_ERROR(g_logger) << "no memory to alloc to m_aEncodeCtx!";
			return false;
		}

		// 设置参数
		m_ctx->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		m_ctx->sample_rate = 44100;
		av_channel_layout_default(&m_ctx->ch_layout, 2);
		m_ctx->bit_rate = m_audioBitrate;

		m_ctx->time_base = AVRational{ 1, m_ctx->sample_rate };
		m_ctx->codec_tag = 0;
		m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		// 打开编码器
		int ret = avcodec_open2(m_ctx, codec, nullptr);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Failed to open the aCodec_Out!\n");
			return false;
		}

		// 获取编码器要求的帧采样点数量
		m_nbSamples = m_ctx->frame_size;

		return true;
	}

	std::vector<PacketWrapperPtr> AudioFfmpegEncoder::Encode(const FramePtr& frame)
	{
		LOG_WARN(g_logger) << "std::vector<PacketWrapperPtr> AudioFfmpegEncoder::Encode(const FramePtr& frame) is unuseful, please use to "
			"int AudioFfmpegEncoder::Encode(const FramePtr& frame, std::function<int(AVPacket*)> func), thanks";
		return std::vector<PacketWrapperPtr>();
	}

	int AudioFfmpegEncoder::Encode(const FramePtr& frame, std::function<int(AVPacket*)> func)
	{
		int ret = 0;

		AVFrame* aframe = frame->Buffer()->Get();

		// 设置参数（编码前）
		aframe->pts = m_nbSamples * m_encode_frame_index++;

		// 送入原始帧 -- 编码器，直到成功送入 || 发生致命错误为止 --> 退出循环
		do 
		{
			ret = avcodec_send_frame(m_ctx, aframe);
			if (ret < 0)
			{
				// 如果是送不进去的失败，立即调用receive -- API，防止队列满溢
				if (ret == AVERROR(EAGAIN))
				{
					int ret_1 = avcodec_receive_packet(m_ctx, m_pkt);
					if (ret_1 < 0 && ret_1 != AVERROR(EAGAIN) && ret_1 != AVERROR_EOF)
					{
						return ret_1;
					}
					// 填充pkt流信息
					m_pkt->stream_index = m_aOutIndex;
					av_packet_rescale_ts(m_pkt, m_ctx->time_base, m_aOut_time_base);
					//m_aCurPts = m_pkt->pts; 记录最新一帧时间戳，此处应该由 FfmpegMuxer 记录
					// 送入回调函数，处理该数据包（逻辑由调用者提供）
					if (func(m_pkt) < 0)
					{
						return -1;
					}
					// 释放引用计数
					av_packet_unref(m_pkt);
				}
				// 注释表明无需再向编码器发送新的一帧，退出即可
				else if (ret == AVERROR_EOF)
				{
					break;
				}
				// 不是这几种错误，直接返回失败 -- 致命错误
				else
				{
					LOG_ERROR(g_logger) << "avcodec_send_frame(m_ctx, frame) return ret = " << ret 
						<< " != AVERROR(EAGAIN)、AVERROR_EOF";
					return ret;
				}
			}
		} while (ret < 0);

		// 接收编码数据包
		do
		{
			ret = avcodec_receive_packet(m_ctx, m_pkt);
			if (ret < 0)
			{
				// 暂无可输出的数据包，退出循环
				if (ret == AVERROR(EAGAIN))
				{
					break;
				}
				// 编码器已经被刷新，不会再输出数据包，退出循环
				else if (ret == AVERROR_EOF)
				{
					break;
				}
				// 发生其余致命错误，退出函数
				else
				{
					return -1;
				}
			}
			// 处理 m_pkt 数据包
			m_pkt->stream_index = m_aOutIndex;
			av_packet_rescale_ts(m_pkt, m_ctx->time_base, m_aOut_time_base);
			// 处理 m_pkt 回调函数
			if (func(m_pkt) < 0)
			{
				return -1;
			}
			// 释放引用计数
			av_packet_unref(m_pkt);

		} while (ret == 0);

		return 0;
	}

	int AudioFfmpegEncoder::Flush(std::function<int(AVPacket*)> func)
	{
		int ret = 0;

		// 送入原始帧 -- 编码器，直到成功送入 || 发生致命错误为止 --> 退出循环
		do
		{
			ret = avcodec_send_frame(m_ctx, nullptr);
			if (ret < 0)
			{
				// 如果是送不进去的失败，立即调用receive -- API，防止队列满溢
				if (ret == AVERROR(EAGAIN))
				{
					int ret_1 = avcodec_receive_packet(m_ctx, m_pkt);
					if (ret_1 < 0 && ret_1 != AVERROR(EAGAIN) && ret_1 != AVERROR_EOF)
					{
						return ret_1;
					}
					// 填充pkt流信息
					m_pkt->stream_index = m_aOutIndex;
					av_packet_rescale_ts(m_pkt, m_ctx->time_base, m_aOut_time_base);
					//m_aCurPts = m_pkt->pts; 记录最新一帧时间戳，此处应该由 FfmpegMuxer 记录
					// 送入回调函数，处理该数据包（逻辑由调用者提供）
					if (func(m_pkt) < 0)
					{
						return -1;
					}
					// 释放引用计数
					av_packet_unref(m_pkt);
				}
				// 注释表明无需再向编码器发送新的一帧，退出即可
				else if (ret == AVERROR_EOF)
				{
					break;
				}
				// 不是这几种错误，直接返回失败 -- 致命错误
				else
				{
					LOG_ERROR(g_logger) << "avcodec_send_frame(m_ctx, frame) return ret = " << ret
						<< " != AVERROR(EAGAIN)、AVERROR_EOF";
					return ret;
				}
			}
		} while (0);

		// 接收编码数据包
		do
		{
			ret = avcodec_receive_packet(m_ctx, m_pkt);
			if (ret < 0)
			{
				// 暂无可输出的数据包，退出循环
				if (ret == AVERROR(EAGAIN))
				{
					break;
				}
				// 编码器已经被刷新，不会再输出数据包，退出循环
				else if (ret == AVERROR_EOF)
				{
					break;
				}
				// 发生其余致命错误，退出函数
				else
				{
					return -1;
				}
			}
			// 处理 m_pkt 数据包
			m_pkt->stream_index = m_aOutIndex;
			av_packet_rescale_ts(m_pkt, m_ctx->time_base, m_aOut_time_base);
			// 处理 m_pkt 回调函数
			if (func(m_pkt) < 0)
			{
				return -1;
			}
			// 释放引用计数
			av_packet_unref(m_pkt);

		} while (true);

		return 0;
	}

	void AudioFfmpegEncoder::Close()
	{
		if (m_pkt)
		{
			av_packet_free(&m_pkt);
		}

		if (m_ctx)
		{
			avcodec_free_context(&m_ctx);
		}
	}

} // namespace streamer
