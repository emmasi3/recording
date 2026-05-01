#include "adapters/capture/DshowAudioCapture.h"
#include "event/ThreadEventSDL.h"
#include "infra/Logger.h"
#include "adapters/codec/FfmpegEncoder.h"
#include "adapters/output/FfmpegMuxerStreamer.h"

#include <chrono>
#include <thread>

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));


class LocalFile
{
public:
    LocalFile(streamer::IMuxer::ptr muxer)
        :m_muxer(muxer)
    {

    }

    int Write_packet_to_local_file(AVPacket* pkt)
    {
        if (m_muxer->WritePacket(pkt))
        {
            return 0;
        }

        return -1;
    }

private:
    streamer::IMuxer::ptr m_muxer;

};

static int Write_to_file_pcm(FILE* f, AVFrame* frame)
{
    ////写入pcm来看看音频对不对
    int bytesPerSample = av_get_bytes_per_sample((AVSampleFormat)frame->format);
    
    // AV_SAMPLE_FMT_S16 音频格式 ffplay -f s16le -ar 44100 -ac 2 ./test_DshowCapture.pcm 播放pcm文件要用ffplay

    if (av_sample_fmt_is_planar((AVSampleFormat)frame->format))
    {
        for (int ch = 0; ch < frame->ch_layout.nb_channels; ++ch)
        {
            fwrite(frame->data[ch], 1, frame->nb_samples * bytesPerSample, f);
        }
    }
    else
    {
        // packed/interleaved: 只在 data[0]，包含所有声道
        fwrite(frame->data[0], 1, frame->nb_samples * bytesPerSample * 2, f);
    }

    return 0;
}

int main()
{
    LOG_INFO(g_logger) << "test_DshowAudioCapture start";

    streamer::SDL::GetInstance()->set_state(streamer::STATE::Start);
    // 创建封装器
    streamer::IMuxer::ptr localfile = streamer::LocalMuxer::createNew("./data_out/audio/test_DshowCapture.aac");
    auto localfile_ptr = std::dynamic_pointer_cast<streamer::LocalMuxer>(localfile);
    LocalFile file_aac(localfile);
    // 获取编码器
    streamer::IEncoder::ptr encoder = localfile_ptr->get_audio_encoder();
    // 获取音频采集组件 From encoder
    streamer::IAudioCapture::ptr cap = nullptr;
    // 创建pcm文件，测试音频是否合理
    FILE* f = nullptr;
    if (fopen_s(&f, "./data_out/audio/test_DshowCapture.pcm", "wb") < 0)
    {
        LOG_ERROR(g_logger) << "failed to open FILE";
        return false;
    }
    // 获取音频采集器
    do
    {
        streamer::AudioFfmpegEncoder::ptr a_encoder_ptr = std::dynamic_pointer_cast<streamer::AudioFfmpegEncoder>(encoder);
        if (!a_encoder_ptr)
        {
            LOG_ERROR(g_logger) << "dynamic_pointer_cast<streamer::AudioFfmpegEncoder>(encoder) failed";
            return -1;
        }

        cap = a_encoder_ptr->get_audioCap_shared();

        if (!cap)
        {
            LOG_ERROR(g_logger) << "DshowAudioCapture::createNew failed, please check dshow input device/microphone";
            streamer::SDL::GetInstance()->set_state(streamer::STATE::Term);
            streamer::SDL::GetInstance()->Stop();
            return -1;
        }
    } while (0);

    constexpr int kNeedFrames = 200;
    int okFrames = 0;

    for (int i = 0; i < kNeedFrames; ++i)
    {
        auto frame = cap->ReadFrame();
        if (!frame || !frame->Buffer() || !frame->Buffer()->Get())
        {
            LOG_ERROR(g_logger) << "ReadFrame failed at i=" << i;
            break;
        }

        AVFrame* af = frame->Buffer()->Get();
        if (af->nb_samples <= 0 || af->sample_rate <= 0)
        {
            LOG_ERROR(g_logger) << "invalid audio frame at i=" << i
                << " nb_samples=" << af->nb_samples
                << " sample_rate=" << af->sample_rate;
            break;
        }

        Write_to_file_pcm(f, af);

        encoder->Encode(frame, std::bind(&LocalFile::Write_packet_to_local_file, &file_aac, std::placeholders::_1));

        ++okFrames;
        LOG_INFO(g_logger) << "audio frame ok i=" << i
            << " nb_samples=" << af->nb_samples
            << " sample_rate=" << af->sample_rate
            << " channels=" << af->ch_layout.nb_channels;
    }

    // 发布信号 -- 终止 -- 停止录音
    streamer::SDL::GetInstance()->set_state(streamer::STATE::Term);

    // 处理音频缓冲区中的剩余内容
    constexpr int nbSamples = 1024;
    streamer::DshowAudioCapture::ptr audio_capture_ptr = std::dynamic_pointer_cast<streamer::DshowAudioCapture>(cap);
    do
    {
        // 如果里面的数据直接不满 per channel frame_size，意味着结束信号发出 && 无数据可取 --> 队列为空，退出循环，结束！
        if (nbSamples > audio_capture_ptr->get_audio_queue_size())
        {
            LOG_DEBUG(g_logger) << "The audio_fifo is empty, and STATE = Term";
            break;
        }

        // 读取
        auto frame = cap->ReadFrame();
        if (!frame || !frame->Buffer() || !frame->Buffer()->Get())
        {
            LOG_ERROR(g_logger) << "ReadFrame failed at =" << okFrames + 1;
            break;
        }

        AVFrame* af = frame->Buffer()->Get();
        if (af->nb_samples <= 0 || af->sample_rate <= 0)
        {
            LOG_ERROR(g_logger) << "af->nb_samples <= 0 || af->sample_rate <= 0, ERROR";
            break;
        }

        Write_to_file_pcm(f, af);

        encoder->Encode(frame, std::bind(&LocalFile::Write_packet_to_local_file, &file_aac, std::placeholders::_1));

        ++okFrames;
        LOG_INFO(g_logger) << "audio frame ok i=" << okFrames
            << " nb_samples=" << af->nb_samples
            << " sample_rate=" << af->sample_rate
            << " channels=" << af->ch_layout.nb_channels;

    } while (true);

    // 刷新编码器缓冲区，保证不丢帧(audio)
    encoder->Flush(std::bind(&LocalFile::Write_packet_to_local_file, &file_aac, std::placeholders::_1));

    // 阻塞并等待线程资源释放
    streamer::SDL::GetInstance()->Stop();
    // 关闭文件 IO
    if(f)
        fclose(f);

    return 0;
}
