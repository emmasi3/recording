#include "concurrency/ConcurrentQueue.h"
#include "infra/Logger.h"

#include <chrono>
#include <thread>
#include <atomic>

/*
* @brief 测试 AudioFifoQueue 模块的写入和读取是否正常，重点排查是否会引发死锁
* 1、加 4 s 的延迟时间，目的：测试 createNew(,4); 这个参数的情况下，ffmpeg 专属的音频共享队列封装逻辑是否逻辑合理，
*	会不会引发死锁
* 2、本来想测试在无限循环中通过 SDL 的键盘事件处理，而不是简单的通过 for 循环指定次数处理，然后查看是否在 STATE::Term 命令下发后
*	，队列是否会被取空
*/

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

static const AVCodec* find_audio_encoder_for_test()
{
  const AVCodec* codec = avcodec_find_encoder_by_name("libfdk_aac");
	if (codec) return codec;

	codec = avcodec_find_encoder_by_name("libmp3lame");
	if (codec) return codec;

	codec = avcodec_find_encoder_by_name("pcm_s16le");
	if (codec) return codec;

	codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (codec) return codec;

	codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
	if (codec) return codec;

	return nullptr;
}

static AVFrame* make_audio_frame(int nb_samples, int channels, int sample_rate, AVSampleFormat fmt, int16_t fill)
{
	AVFrame* frame = av_frame_alloc();
	if (!frame)
	{
		return nullptr;
	}

	frame->nb_samples = nb_samples;
	frame->format = static_cast<int>(fmt);
	frame->sample_rate = sample_rate;
	av_channel_layout_default(&frame->ch_layout, channels);

	if (av_frame_get_buffer(frame, 0) < 0)
	{
		av_frame_free(&frame);
		return nullptr;
	}

	if (av_frame_make_writable(frame) < 0)
	{
		av_frame_free(&frame);
		return nullptr;
	}

	int16_t* pcm = reinterpret_cast<int16_t*>(frame->data[0]);
	int samples = nb_samples * channels;
	for (int i = 0; i < samples; ++i)
	{
        pcm[i] = fill;
	}

	return frame;
}

int main()
{
	LOG_INFO(g_logger) << "AudioFifoQueue test start";

	const AVCodec* codec = find_audio_encoder_for_test();
	if (!codec)
	{
		LOG_ERROR(g_logger) << "No audio encoder found for test";
		return -1;
	}

	// 编码器上下文
	AVCodecContext* ctx = avcodec_alloc_context3(codec);
	if (!ctx)
	{
		LOG_ERROR(g_logger) << "avcodec_alloc_context3 failed";
		return -1;
	}

	ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	ctx->sample_rate = 48000;
	av_channel_layout_default(&ctx->ch_layout, 2);
	ctx->frame_size = 1024;

	streamer::SDL::GetInstance()->set_state(streamer::STATE::Start);

	// 最多存储 2 帧音频数据，具体看实现
	auto q_base = streamer::AudioFifoQueue::createNew(ctx, 4);
	if (!q_base)
	{
		LOG_ERROR(g_logger) << "AudioFifoQueue::createNew failed";
		avcodec_free_context(&ctx);
		return -1;
	}

	auto q = std::dynamic_pointer_cast<streamer::AudioFifoQueue>(q_base);
	if (!q)
	{
		LOG_ERROR(g_logger) << "dynamic_pointer_cast<AudioFifoQueue> failed";
		avcodec_free_context(&ctx);
		return -1;
	}

	constexpr int kFrameCount = 50;
	std::atomic<int> bad_frames{ 0 };

	// 生产者
	std::thread producer([&]() {
		for (int i = 0; i < kFrameCount; ++i)
		{
			AVFrame* frame = make_audio_frame(ctx->frame_size, ctx->ch_layout.nb_channels, ctx->sample_rate, static_cast<AVSampleFormat>(ctx->sample_fmt), static_cast<int16_t>(i + 1));
			if (!frame)
			{
				LOG_ERROR(g_logger) << "producer: make_audio_frame failed at " << i;
				++bad_frames;
				continue;
			}

			q->Push(frame);
			LOG_INFO(g_logger) << "producer push frame " << i;
			av_frame_free(&frame);
			// std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

	// 消费者
	std::thread consumer([&]() {
		// 1、让消费者等一阵子，测试生产者是否是否会死锁
		// std::this_thread::sleep_for(std::chrono::seconds(4));
		for (int i = 0; i < kFrameCount; ++i)
		{
			AVFrame* out = make_audio_frame(ctx->frame_size, ctx->ch_layout.nb_channels, ctx->sample_rate, static_cast<AVSampleFormat>(ctx->sample_fmt), 0);
			if (!out)
			{
				LOG_ERROR(g_logger) << "consumer: make_audio_frame failed at " << i;
				++bad_frames;
				continue;
			}

			int ret = q->WaitAndPop(out);
			if (ret < 0)
			{
				LOG_ERROR(g_logger) << "consumer: WaitAndPop failed at " << i;
				++bad_frames;
				av_frame_free(&out);
				continue;
			}

			int16_t expect = static_cast<int16_t>(i + 1);
			int16_t* pcm = reinterpret_cast<int16_t*>(out->data[0]);
			int samples = out->nb_samples * out->ch_layout.nb_channels;
			for (int s = 0; s < samples; ++s)
			{
				if (pcm[s] != expect)
				{
					LOG_ERROR(g_logger) << "consumer: sample mismatch frame=" << i << " idx=" << s << " got=" << pcm[s] << " expect=" << expect;
					++bad_frames;
					break;
				}
			}

			LOG_INFO(g_logger) << "consumer pop frame " << i;
			av_frame_free(&out);
		}
	});

	producer.join();
	consumer.join();

	q.reset();
	streamer::SDL::GetInstance()->set_state(streamer::STATE::Term);

	avcodec_free_context(&ctx);

	if (bad_frames.load() > 0)
	{
		LOG_ERROR(g_logger) << "AudioFifoQueue test failed, bad_frames=" << bad_frames.load();
		return -1;
	}

	LOG_INFO(g_logger) << "AudioFifoQueue test passed";
	return 0;
}