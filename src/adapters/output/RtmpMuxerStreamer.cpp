#include "adapters/output/RtmpMuxerStreamer.h"
#include "infra/Logger.h"
#include "adapters/codec/FfmpegEncoder.h"
#include "adapters/capture/DxgiScreenCapture.h"
#include "adapters/capture/DshowAudioCapture.h"
#include "event/ThreadEventSDL.h"
#include <chrono>
#include <thread>
#include <functional>

namespace streamer {
    static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

    RtmpMuxer::RtmpMuxer(const std::string& url) 
        : m_url(url), m_close(false), m_vIndex(0), m_aIndex(0), m_ctx(nullptr) 
    {
    }

    RtmpMuxer::~RtmpMuxer() noexcept 
    {
        Close();
    }

    IMuxer::ptr RtmpMuxer::createNew(const std::string& url) 
    {
        IMuxer::ptr ptr = std::make_shared<RtmpMuxer>(url);

        return ptr;
    }

    bool RtmpMuxer::Open() 
    {
        int ret = 0;
        AVStream* v_outStream = nullptr;
        AVStream* a_outStream = nullptr;

        if (m_ctx)
        {
            return true;
        }

        ret = avformat_alloc_output_context2(&m_ctx, nullptr, "flv", m_url.c_str());
        if (ret < 0 || !m_ctx || !m_ctx->oformat)
        {
            return false;
        }

        bool needVideo = (m_ctx->oformat->video_codec != AV_CODEC_ID_NONE);
        bool needAudio = (m_ctx->oformat->audio_codec != AV_CODEC_ID_NONE);

        if (!needVideo && !needAudio)
        {
            LOG_ERROR(g_logger) << "m_ctx video && audio not needed?";
            return false;
        }

        if (needVideo)
        {
            v_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!v_outStream)
            {
                LOG_ERROR(g_logger) << "v_outStream = avformat_new_stream() failed";
                return false;
            }

            if (!m_vEncoder)
            {
                m_vEncoder = VideoFfmpegEncoder::createNew(v_outStream->index, AVRational{ 1, 25 });
            }

            if (!m_vEncoder || !m_vEncoder->getCtx())
            {
                LOG_ERROR(g_logger) << "!m_vEncoder || !m_vEncoder->getCtx() ? why?";
                return false;
            }

            ret = avcodec_parameters_from_context(v_outStream->codecpar, m_vEncoder->getCtx());
            if (ret < 0)
            {
                LOG_ERROR(g_logger) << "avcodec_parameters_from_context(v_outStream->codecpar, m_vEncoder->getCtx()) return ret: "
                    << ret;
                return false;
            }

            v_outStream->codecpar->codec_tag = 0;
            m_vIndex = v_outStream->index;
        }

        if (needAudio)
        {
            a_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!a_outStream)
            {
                LOG_ERROR(g_logger) << "a_outStream = avformat_new_stream() failed";
                return false;
            }

            AVCodecID audioCodecId = m_ctx->oformat->audio_codec;
            m_aEncoder = AudioFfmpegEncoder::createNew(audioCodecId, { 0, 0 }, a_outStream->index);
            if (!m_aEncoder || !m_aEncoder->getCtx())
            {
                return false;
            }

            ret = avcodec_parameters_from_context(a_outStream->codecpar, m_aEncoder->getCtx());
            if (ret < 0) return false;

            a_outStream->codecpar->codec_tag = 0;
            m_aIndex = a_outStream->index;
        }

        // 打开网络 IO
        if (!(m_ctx->oformat->flags & AVFMT_NOFILE))
        {
            ret = avio_open2(&m_ctx->pb, m_url.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
            if (ret < 0) return false;
        }

        ret = avformat_write_header(m_ctx, nullptr);
        if (ret < 0) return false;

        if (a_outStream && m_aEncoder)
        {
            auto audio_encoder_ptr = std::dynamic_pointer_cast<AudioFfmpegEncoder>(m_aEncoder);
            if (audio_encoder_ptr)
            {
                audio_encoder_ptr->set_aOut_time_base(a_outStream->time_base);
            }
        }

        if (v_outStream && m_vEncoder)
        {
            auto video_encoder_ptr = std::dynamic_pointer_cast<VideoFfmpegEncoder>(m_vEncoder);
            if (video_encoder_ptr)
            {
                video_encoder_ptr->set_vOut_time_base(v_outStream->time_base);
            }
        }

        return true;
    }

    bool RtmpMuxer::WritePacket(const PacketWrapperPtr& packet) 
    {
        LOG_WARN(g_logger) << "RtmpMuxer::WritePacket(const PacketWrapperPtr& packet) is not define, please to use to "
            << "RtmpMuxer::WritePacket(AVPacket* packet)";
        return false;
    }

    bool RtmpMuxer::WritePacket(AVPacket* packet) 
    {
        if (!m_ctx || !packet) 
        {
            return false;
        }
        int ret = av_interleaved_write_frame(m_ctx, packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) 
        {
            LOG_ERROR(g_logger) << "av_interleaved_write_frame(m_ctx, packet) return ret: " << ret;
            return false;
        }
        return true;
    }

    void RtmpMuxer::Close() 
    {
        if (m_close)
            return;
        m_close = true;

        if (m_ctx) 
        {
            av_write_trailer(m_ctx);
            if (!(m_ctx->oformat->flags & AVFMT_NOFILE)) 
            {
                avio_closep(&m_ctx->pb);
            }
            avformat_free_context(m_ctx);
        }
    }

    ///////////////////////// streamer //////////////////////////////

    RtmpStreamer::RtmpStreamer(const std::string& url)
        : m_url(url) 
    {
        // 创建 rtmp 专属封装器实例
        m_muxer = RtmpMuxer::createNew(url);
    }

    RtmpStreamer::~RtmpStreamer()
    {
        Disconnect();
    }

    IStreamer::ptr RtmpStreamer::createNew(const std::string& url) 
    {
        IStreamer::ptr ptr = std::make_shared<RtmpStreamer>(url);
        // 打开网络部件
        if (!ptr->Connect())
        {
            return nullptr;
        }

        return ptr;
    }

    bool RtmpStreamer::Connect() 
    {
        if (m_muxer)
        {
            // 打开 rtmp 网络连接，ffmpeg 底层会处理 rtmp 和 tcp 握手···
            bool res = m_muxer->Open();
            if (res)
            {
                // 开启消费线程，发送数据到数据流中(依赖 av_interleaved_write_frame)
                if (!send_MuxThreadProc_to_threads())
                {
                    return false;
                }
            }
        }

        RtmpMuxer::ptr mux_ptr = std::dynamic_pointer_cast<RtmpMuxer>(m_muxer);
        if (!mux_ptr || !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder())
        {
            LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<LocalMuxer>(m_muxer) "
                "failed or !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder() is nullptr";
            return false;
        }

        // 获取音视频编码器
        SetVideoEncoder(mux_ptr->get_video_encoder());
        SetAudioEncoder(mux_ptr->get_audio_encoder());

        return true;
    }

    bool RtmpStreamer::SendPacket(const PacketWrapperPtr& packet) 
    {
        return true;
    }

    void RtmpStreamer::Disconnect()
    {
        if (m_muxer)
        {
            m_muxer->Close();
        }
    }

    void RtmpStreamer::MuxThreadProc() 
    {
        auto vEncPtr = std::dynamic_pointer_cast<VideoFfmpegEncoder>(m_vEncoder);
        auto aEncPtr = std::dynamic_pointer_cast<AudioFfmpegEncoder>(m_aEncoder);

        auto dxgiCap = vEncPtr ? vEncPtr->getDxgiCap() : nullptr;
        auto audioCap = aEncPtr ? aEncPtr->get_audioCap_shared() : nullptr;

        auto dshow_audioCap = std::dynamic_pointer_cast<streamer::DshowAudioCapture>(audioCap);
        int v_frame_idx = 0;

        AVRational v_tb = vEncPtr->get_vOut_time_base();
        AVRational a_tb = aEncPtr->get_aOut_time_base();

        bool done = false;
    
        // Ensure to only encode when valid pointers exist to avoid crash if components missing
        while (true) 
        {
            if (!done && SDL::GetInstance()->get_state() == STATE::Term)
            {
                done = true;
            }
            int cmp = av_compare_ts(m_last_video_pts, v_tb, m_last_audio_pts, a_tb);

            if (done)
            {
                cmp = 1;
                if (dshow_audioCap && m_aEncoder && m_aEncoder->getCtx() && dshow_audioCap->get_audio_queue_size() < m_aEncoder->getCtx()->frame_size)
                {
                    break;
                }
            }

            if (cmp <= 0) 
            {
                if (dxgiCap) 
                {
                    FramePtr vFrame = dxgiCap->ReadFrame(v_frame_idx);
                    if (vFrame && dxgiCap->isPass(v_frame_idx)) 
                    {
                        m_vEncoder->Encode(vFrame, [&](AVPacket* pkt) -> int {
                            m_last_video_pts = pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
                            return m_muxer->WritePacket(pkt) ? 0 : -1;
                            });
                        ++v_frame_idx;
                    }
                }
            }
            else
            {
                if (audioCap) 
                {
                    FramePtr aFrame = audioCap->ReadFrame();
                    if (aFrame) 
                    {
                        m_aEncoder->Encode(aFrame, [&](AVPacket* pkt) -> int {
                            m_last_audio_pts = pkt->dts; // 修改为用 dts 或 pts 记录都可以，但是一定要记录
                            return m_muxer->WritePacket(pkt) ? 0 : -1;
                            });
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (m_vEncoder) {
            m_vEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });
        }
        if (m_aEncoder) {
            m_aEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });
        }

        LOG_DEBUG(g_logger) << "RtmpStreamer::MuxThreadProc() is finished";
    }

    bool RtmpStreamer::send_MuxThreadProc_to_threads() 
    {
        int threads = SDL::GetInstance()->get_threads_counts();
        // 开启音视频消费者线程
        SDL::GetInstance()->push_thread_to_vector(std::bind(&RtmpStreamer::MuxThreadProc, this));
        if (threads == SDL::GetInstance()->get_threads_counts())
        {
            return false;
        }

        return true;
    }

} // namespace streamer
