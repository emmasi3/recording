#include "adapters/capture/DshowAudioCapture.h"
#include "infra/Logger.h"
#include "event/ThreadEventSDL.h"

namespace streamer {

	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

    DshowAudioCapture::DshowAudioCapture(IEncoder::ptr encoder)
		:m_encoder(encoder),
		m_frame(nullptr),
		m_queue(nullptr),
		m_decoder(nullptr)
    {
		
    }

    DshowAudioCapture::~DshowAudioCapture()
    {
        Close();
    }

    IAudioCapture::ptr DshowAudioCapture::createNew(IEncoder::ptr encoder)
    {
        IAudioCapture::ptr ptr = std::make_shared<DshowAudioCapture>(encoder);
		DshowAudioCapture::ptr audio_ptr = std::dynamic_pointer_cast<DshowAudioCapture>(ptr);

		// 打开设备 && 解码器 && 重采样 && 音频共享队列
        if (!ptr->Open())
        {
            LOG_ERROR(g_logger) << "IAudioCapture::Open() failed";
            return nullptr;
        }
     
        return ptr;
    }

    bool DshowAudioCapture::Open() 
    {
		int ret = 0;
		// 注册设备(all)
		avdevice_register_all();

		//这里是指定输入格式
		const AVInputFormat* ifmt = av_find_input_format("dshow");
		if (!ifmt)
		{
			LOG_ERROR(g_logger) << "the AVInputFormat *ifmt failed!";
			return false;
		}

		// 获取设备名称（audio= 处理过的）
		std::string audioDeviceName = get_first_dshow_audio_device_name(ifmt);
		if (audioDeviceName.empty())
		{
			LOG_ERROR(g_logger) << "get_first_dshow_audio_device_name() failed";
			return false;
		}
		
		//这一步的分配不是必须的，会在avformat_opne_input()中自动调用 alloc() 函数
		m_FmtCtx = avformat_alloc_context();

		//打开麦克风设备录音
		ret = avformat_open_input(&m_FmtCtx, audioDeviceName.c_str(), ifmt, nullptr);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "the avformat_open_input() error:";
			return false;
		}

		// 打开音频解码器
		if (!init_audio_decoder())
		{
			LOG_ERROR(g_logger) << "init_audio_decoder() failed!";
			return false;
		}

		// 初始化重采样上下文
		if (!init_swrCtx())
		{
			return false;
		}

		// 初始化音频共享队列
		if (!init_AudioFifo())
		{
			return false;
		}

		// 初始化复用帧（给编码器或者其他组件的，保证拥有固定的采样点数量，除非是最后一帧）
		m_frame = AllocAudioFrame(m_encoder->getCtx()->frame_size);
		if (!m_frame)
		{
			return false;
		}

		// 开启音频生产者线程
		if (!send_ReadFrameFrom_device_threads())
		{
			return false;
		}

		return true;
    }

	void DshowAudioCapture::Close()
	{
		if (m_frame)
			av_frame_free(&m_frame);
		if (m_swrCtx)
			swr_free(&m_swrCtx);
		if (m_FmtCtx)
			avformat_close_input(&m_FmtCtx);
	}


    void DshowAudioCapture::ReadFrameFrom_device()
    {
		// 从设备读取一帧后送入队列中 -- 生产者线程
		AVPacket* pkt = nullptr;
		AVFrame* frame = nullptr;
		AVFrame* newFrame = nullptr;
		int nbSamples = m_encoder->getCtx()->frame_size; // 编码器要求的帧大小，一般为1024，某些要求 1152···(per channel)
		int dstNbSamples = 0, maxNbSamples = 0;

		int ret = 0;

		//首先根据采样率来重新计算样本个数
		dstNbSamples = maxNbSamples = av_rescale_rnd(nbSamples, 
			m_encoder->getCtx()->sample_rate, 
			m_decoder->getCtx()->sample_rate, AV_ROUND_UP);

		//重采样所需buffer
		newFrame = AllocAudioFrame(nbSamples);
		if (!newFrame)
		{
			LOG_ERROR(g_logger) << "AllocAudioFrame is Failed!";
			return;
		}

		pkt = av_packet_alloc();
		if (!pkt)
		{
			LOG_ERROR(g_logger) << "Failed to alloc audio_packet!";
			return;
		}

		frame = av_frame_alloc();
		if (!frame)
		{
			LOG_ERROR(g_logger) << "Failed to alloc audio_frame!";
			return;
		}


		while (SDL::GetInstance()->get_state() != STATE::Term)
		{
			//读取音频帧
			ret = av_read_frame(m_FmtCtx, pkt);
			if (ret < 0)
			{
				LOG_ERROR(g_logger) << "Failed to read_frame from audio_device!";
				return;
			}

			//检查（即使 m_aIndex == m_vIndex ，也要检查，因为可能处于某些不可控的因素，导致读取到的pkt不是我们选定的音频设备记录的！！！，视频也一样）
			if (pkt->stream_index != m_index)
			{
				LOG_WARN(g_logger) << "The pkt->stream_index != m_aIndex!";
				av_packet_unref(pkt);
				continue;
			}

			//解码
			ret = avcodec_send_packet(m_decoder->getCtx(), pkt);
			if (ret < 0)
			{
				LOG_WARN(g_logger) << "Failed to send_packet to m_aDecodeCtx! the error: " << ret;
				av_packet_unref(pkt);
				continue;
			}

			//接收
			ret = avcodec_receive_frame(m_decoder->getCtx(), frame);
			if (ret < 0)
			{
				LOG_WARN(g_logger) << "Failed to receive_frame from m_aDecodeCtx! the error: " << ret;
				av_packet_unref(pkt);
				av_frame_unref(frame);
				continue;
			}

			//重采样 -- 主要转换采样格式为编码器接收的格式，以及相应的一些字段的更改
			do
			{
				//根据“延迟的（重采样器缓存的--未转换的）”+ “当前帧的样本数”---> 当做“输入重采样器”的总样本数，也就是输入，
				//然后根据采样率的比例关系，重新计算此次，应该输出多少用采样数，以便于重新分配接受者 的缓冲区大小！
				dstNbSamples = av_rescale_rnd(
					swr_get_delay(m_swrCtx, m_decoder->getCtx()->sample_rate) + frame->nb_samples,
					m_encoder->getCtx()->sample_rate,
					m_decoder->getCtx()->sample_rate,
					AV_ROUND_UP
				);

				//计算出来的 输出样本数如果大于 max 样本数，就重新分配
				if (dstNbSamples > maxNbSamples)
				{
					maxNbSamples = dstNbSamples;

					if (newFrame)
					{
						av_frame_free(&newFrame);
						newFrame = AllocAudioFrame(dstNbSamples);
					}

					if (!newFrame)
					{
						LOG_ERROR(g_logger) << "Failed to alloc_audio_newFrame_swr!";
						return;
					}
				}

				//转换
				newFrame->nb_samples = swr_convert(
					m_swrCtx,
					(uint8_t**)newFrame->data,
					dstNbSamples,
					(const uint8_t**)frame->data,
					frame->nb_samples
				);

				//检查
				if (newFrame->nb_samples < 0)
				{
					LOG_ERROR(g_logger) << "The swr_convert() return Error: " << newFrame->nb_samples;
					return;
				}

			} while (0);

			//加锁 && 写入 audio_fifo_buf
			m_queue->Push(newFrame);

			//循环1次结束后的释放工作
			av_packet_unref(pkt);
			av_frame_unref(frame);
			av_frame_make_writable(newFrame);
		}

		//结束后的收尾工作

		//刷新
		FlushAudioDecoder();

		if (pkt)
			av_packet_free(&pkt);
		if (frame)
			av_frame_free(&frame);
		if (newFrame)
			av_frame_free(&newFrame);

    }

	bool DshowAudioCapture::send_ReadFrameFrom_device_threads()
	{
		int threads_counts = SDL::GetInstance()->get_threads_counts();
		// 开始采集音频并送入队列
        SDL::GetInstance()->push_thread_to_vector(std::bind(&DshowAudioCapture::ReadFrameFrom_device, this));
		if (threads_counts == SDL::GetInstance()->get_threads_counts())
		{
			LOG_ERROR(g_logger) << "SDL::GetInstance()->push_thread_to_vector(std::bind(DshowAudioCapture::ReadFrameFrom_device, this)); failed";
			return false;
		}

		return true;
	}

	void DshowAudioCapture::FlushAudioDecoder()
	{
		int ret = -1;
		AVPacket pkt = { 0 };
		int dstNbSamples, maxDstNbSamples;
		AVFrame* rawFrame = av_frame_alloc();
		AVFrame* newFrame = AllocAudioFrame(m_encoder->getCtx()->frame_size);
		maxDstNbSamples = dstNbSamples = av_rescale_rnd(
			m_encoder->getCtx()->frame_size,
			m_encoder->getCtx()->sample_rate,
			m_decoder->getCtx()->sample_rate,
			AV_ROUND_UP);

		ret = avcodec_send_packet(m_decoder->getCtx(), nullptr);
		if (ret != 0)
		{
			LOG_ERROR(g_logger) << "flush audio avcodec_send_packet  failed, ret: " << ret;
			return;
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_frame(m_decoder->getCtx(), rawFrame);
			if (ret < 0)
			{
				if (ret == AVERROR(EAGAIN))
				{
					LOG_WARN(g_logger) << "flush audio EAGAIN avcodec_receive_frame";
					ret = 1;
					continue;
				}
				else if (ret == AVERROR_EOF)
				{
					LOG_INFO(g_logger) << "flush audio decoder finished";
					break;
				}

				LOG_ERROR(g_logger) << "flush audio avcodec_receive_frame error, ret: " << ret;
				return;
			}

			dstNbSamples = av_rescale_rnd(
				swr_get_delay(m_swrCtx, m_decoder->getCtx()->sample_rate) + rawFrame->nb_samples,
				m_encoder->getCtx()->sample_rate,
				m_decoder->getCtx()->sample_rate,
				AV_ROUND_UP);
			if (dstNbSamples > maxDstNbSamples)
			{
				av_freep(&newFrame->data[0]);
				ret = av_samples_alloc(
					newFrame->data,
					newFrame->linesize,
					m_encoder->getCtx()->ch_layout.nb_channels,
					dstNbSamples, 
					m_encoder->getCtx()->sample_fmt,
					1);
				if (ret < 0)
				{
					LOG_ERROR(g_logger) << "flush av_samples_alloc failed";
					return;
				}
				maxDstNbSamples = dstNbSamples;
			}

			newFrame->nb_samples = swr_convert(
				m_swrCtx, 
				newFrame->data, 
				dstNbSamples,
				(const uint8_t**)rawFrame->data, 
				rawFrame->nb_samples);

			if (newFrame->nb_samples < 0)
			{
				LOG_ERROR(g_logger) << "flush swr_convert failed";
				return;
			}

			// 写入 AudioFifo
			m_queue->Push(newFrame);
		}

		LOG_DEBUG(g_logger) << "生产者（音频）解码器已刷新完毕!";
	}

	FramePtr DshowAudioCapture::ReadFrame()
	{
		int ret = m_queue->WaitAndPop(m_frame);
		if (ret < 0)
		{
			return nullptr;
		}

		FramePtr ptr = std::make_shared<RawFrame>(FrameMeta{ MediaType::Audio }, std::make_shared<FrameWrapper>(m_frame, false));
		return ptr;
	}

	AVFrame* DshowAudioCapture::AllocAudioFrame(int nbSamples)
	{
		int ret = 0;
		AVFrame* frame = nullptr;

		frame = av_frame_alloc();
		if (!frame)
		{
			av_log(NULL, AV_LOG_ERROR, "Failed to alloc_newFrame_audio!\n");
			return nullptr;
		}

		//frame->ch_layout = c->ch_layout;
		ret = av_channel_layout_copy(&frame->ch_layout, &m_encoder->getCtx()->ch_layout);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Failed to copy ch_layout to newFrame->ch_layout!\n");
			av_frame_free(&frame);
			return nullptr;
		}

		frame->sample_rate = m_encoder->getCtx()->sample_rate;
		frame->nb_samples = nbSamples;
		frame->format = m_encoder->getCtx()->sample_fmt;

		if (frame->nb_samples <= 0)
		{
			av_log(NULL, AV_LOG_ERROR, "The frame of AllocAudioFrame nb_samples <= 0!\n");
			av_frame_free(&frame);
			return nullptr;
		}

		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_ERROR, "Failed to av_frame_get_buffer() AllocAudioFrame !\n");
			av_frame_free(&frame);
			return nullptr;
		}

		return frame;
	}

	const uint64_t DshowAudioCapture::get_audio_queue_size()
	{
		AudioFifoQueue::ptr queue_ptr = std::dynamic_pointer_cast<AudioFifoQueue>(m_queue);
		if (!queue_ptr)
		{
			LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<AudioFifoQueue>(m_queue) in DshowAudioCapture::get_audio_queue_size()";
			return 0;
		}

		return queue_ptr->get_audio_queue_size();
	}

	const uint64_t DshowAudioCapture::get_audio_queue_space()
	{
		AudioFifoQueue::ptr queue_ptr = std::dynamic_pointer_cast<AudioFifoQueue>(m_queue);
		if (!queue_ptr)
		{
			LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<AudioFifoQueue>(m_queue) in DshowAudioCapture::get_audio_queue_size()";
			return 0;
		}

		return queue_ptr->get_audio_queue_space();
	}

	bool DshowAudioCapture::is_audio_device(AVDeviceInfo* dev)
    {
        for (int i = 0; i < dev->nb_media_types; ++i)
        {
            if (dev->media_types[i] == AVMEDIA_TYPE_AUDIO)
            {
                return true;
            }
        }

        return false;
    }

    const std::string DshowAudioCapture::get_first_dshow_audio_device_name(const AVInputFormat* iformat)
	{
        std::string audioDeviceName = "audio=";

        AVDeviceInfoList* device_list = nullptr;

        // audio=true, video=false
        int ret = avdevice_list_input_sources(
            iformat,
            nullptr,
            nullptr,
            &device_list
        );
        if (ret < 0) {
            LOG_ERROR(g_logger) << "avdevice_list_input_sources failed";
            audioDeviceName.clear();
            return audioDeviceName;
        }

        // 标志是否找到
        bool is_find = false;

        for (int i = 0; i < device_list->nb_devices; ++i)
        {
            AVDeviceInfo* dev = device_list->devices[i];

            if (is_audio_device(dev))
            {
                audioDeviceName = audioDeviceName + dev->device_name;
                is_find = true;
                break;
            }
        }

        if (!is_find)
        {
            LOG_ERROR(g_logger) << "not Found any audio device!";
            audioDeviceName.clear();
            return audioDeviceName;
        }

        avdevice_free_list_devices(&device_list);

		return audioDeviceName;
	}

    bool DshowAudioCapture::init_audio_decoder()
    {
		int ret = 0;
		AVStream* stream = nullptr;

		//填充avforamt中的streams流信息
		ret = avformat_find_stream_info(m_FmtCtx, nullptr);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "avformat_find_stream_info() error:";
			return false;
		}

		//根据流信息寻找音频流（一般麦克风就是 0 ，第一路流）
		for (int i = 0; i < m_FmtCtx->nb_streams; i++)
		{
			stream = m_FmtCtx->streams[i];
			if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				m_index = i;
				LOG_DEBUG(g_logger) << "the stream[%d] is selected of needed!" << m_index;
				break;
			}
		}

		// 判断是否找到音频流
		if (m_index == -1)
		{
			LOG_ERROR(g_logger) << "find audio_stream_info failed";
			return false;
		}

		// 创建对应的解码器，并获取其智能指针
		m_decoder = FfmpegDecoder::createNew(m_FmtCtx->streams[m_index]->codecpar);

		return true;
    }

	bool DshowAudioCapture::init_swrCtx()
	{
		//初始化重采样（参数）
		do
		{
			AVChannelLayout in_ch_layout = m_decoder->getCtx()->ch_layout;
			AVChannelLayout out_ch_layout = m_encoder->getCtx()->ch_layout;
			swr_alloc_set_opts2(
				&m_swrCtx,
				&out_ch_layout,
				m_encoder->getCtx()->sample_fmt,
				m_encoder->getCtx()->sample_rate,
				&in_ch_layout,
				m_decoder->getCtx()->sample_fmt,
				m_decoder->getCtx()->sample_rate,
				0, nullptr
			);

			if (swr_init(m_swrCtx) < 0)
			{
				LOG_ERROR(g_logger) << "swr_init(m_swrCtx) failed!";
				return false;
			}

		} while (0);

		return true;
	}

	bool DshowAudioCapture::init_AudioFifo(int count)
	{
		m_queue = AudioFifoQueue::createNew(m_encoder->getCtx(), count);
		
		if (!m_queue)
		{
			LOG_ERROR(g_logger) << "m_queue = AudioFifoQueue::createNew(m_encoder->getCtx(), count) return nullptr";
			return false;
		}

		return true;
	}


} // namespace streamer
