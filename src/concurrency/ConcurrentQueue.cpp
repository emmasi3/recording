#include "concurrency/ConcurrentQueue.h"
#include "infra/Logger.h"

namespace streamer
{
	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

	AudioFifoQueue::AudioFifoQueue()
		:m_aFifoBuf(nullptr),
		m_nbSamples(0)
	{

	}

	AudioFifoQueue::~AudioFifoQueue()
	{
		Close();
	}

	IConcurrentQueue<AVFrame*>::ptr AudioFifoQueue::createNew(AVCodecContext* ctx, int count)
	{
		IConcurrentQueue<AVFrame*>::ptr ptr = std::make_shared<AudioFifoQueue>();
		AudioFifoQueue::ptr audio_ptr = std::dynamic_pointer_cast<AudioFifoQueue>(ptr);
		// 初始化队列
		if (!audio_ptr->Init_Audio_Fifo(ctx, count))
		{
			LOG_ERROR(g_logger) << "audio_ptr->Init_Audio_Fifo(ctx, count) failed";
			return nullptr;
		}

		return ptr;
	}

	void AudioFifoQueue::Push(AVFrame* item)
	{
		do
		{
			// 加锁
			std::unique_lock<std::mutex> lock(m_mtx);

			// 关键修复：若本次写入超过当前总容量，先扩容，避免 wait 永远 false
			const int curSize = av_audio_fifo_size(m_aFifoBuf);
			const int curCap = curSize + av_audio_fifo_space(m_aFifoBuf);
			if (item->nb_samples > curCap)
			{
				const int newCap = curSize + item->nb_samples + m_nbSamples; // 留一点余量
				if (av_audio_fifo_realloc(m_aFifoBuf, newCap) < 0)
				{
					LOG_ERROR(g_logger) << "av_audio_fifo_realloc failed, need=" << item->nb_samples;
					return;
				}
			}
			// 条件变量
			m_cvNotFull.wait(lock, [this, item] {
				return av_audio_fifo_space(m_aFifoBuf) >= item->nb_samples;
				});
			// 写入
			int ret = av_audio_fifo_write(m_aFifoBuf, (void* const*)item->data, item->nb_samples);
			if (ret < 0)
			{
				LOG_ERROR(g_logger) << "Failed to write newFrame to m_aFifoBuf!";
				return;
			}

		} while (0);
		// 通知不空
		m_cvNotEmpty.notify_one();
		
	}

	std::optional<AVFrame*> AudioFifoQueue::TryPop()
	{
		LOG_WARN(g_logger) << "AudioFifoQueue::TryPop() do not complete, please use WaitAndPop, so return nullptr";
		return nullptr;
	}

	AVFrame* AudioFifoQueue::WaitAndPop()
	{
		LOG_WARN(g_logger) << "AVFrame* AudioFifoQueue::WaitAndPop() is not useful, please use -- "
			<< "int AudioFifoQueue::WaitAndPop(AVFrame * item)";
		return nullptr;
	}

	int AudioFifoQueue::WaitAndPop(AVFrame* item)
	{
		{
			std::unique_lock<std::mutex> lock(m_mtx);

			m_cvNotEmpty.wait(lock, [this, item] {
				return av_audio_fifo_size(m_aFifoBuf) >= item->nb_samples;
				});

			// 读取
			int ret = av_audio_fifo_read(m_aFifoBuf, (void* const*)item->data, item->nb_samples);
			if (ret < 0)
			{
				LOG_ERROR(g_logger) << "Failed to read newFrame to m_aFifoBuf!";
				return -1;
			}
		}
		// 不满
		m_cvNotFull.notify_one();

		return 0;
	}

	void AudioFifoQueue::Close()
	{
		if(m_aFifoBuf)
			av_audio_fifo_free(m_aFifoBuf);
	}

	bool AudioFifoQueue::Init_Audio_Fifo(AVCodecContext* aEncodeCtx, int count)
	{
		m_nbSamples = aEncodeCtx->frame_size; // 一个音频帧的采样点个数（1个声道的）
		if (!m_nbSamples)
		{
			LOG_WARN(g_logger) << "the m_aDecodeCtx->frame_size is NULL";
			aEncodeCtx->frame_size = 1024;
			m_nbSamples = 1024;
		}
		if (aEncodeCtx->codec->name == "libmp3lame")
		{
			aEncodeCtx->frame_size = 1152;
			m_nbSamples = 1152;
		}

		//这里存储的是解码（解码器）并且“重采样（用的是编码器的参数）”麦克风采集的 PCM 原始数据
		this->m_aFifoBuf = av_audio_fifo_alloc(aEncodeCtx->sample_fmt, aEncodeCtx->ch_layout.nb_channels, count * m_nbSamples);
		if (!m_aFifoBuf)
		{
			LOG_ERROR(g_logger) << "Failed to alloc FifoBuf!";
			return false;
		}

		return true;
	}

	const uint64_t AudioFifoQueue::get_audio_queue_size()
	{
		uint64_t size = -1;
		{
			// 加锁
			std::lock_guard<std::mutex> lock(m_mtx);
			size = av_audio_fifo_size(m_aFifoBuf);
		}

		return size;
	}

	const uint64_t AudioFifoQueue::get_audio_queue_space()
	{
		uint64_t space = -1;
		{
			// 加锁
			std::lock_guard<std::mutex> lock(m_mtx);
			space = av_audio_fifo_space(m_aFifoBuf);
		}

		return space;
	}

}