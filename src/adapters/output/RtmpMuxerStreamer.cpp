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

        // 打开输出上下文 -- Rtmp 网络连接
        if (!ptr || !ptr->Open())
        {
            LOG_ERROR(g_logger) << "LocalMuxer::createNew() -- Open() fail";
            return nullptr;
        }

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
                m_vEncoder = VideoFfmpegEncoder::createNew(v_outStream->index, AVRational{ 1, 60 });
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
        // m_muxer = RtmpMuxer::createNew(url);
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
        //if (m_muxer)
        //{
        //    // 打开 rtmp 网络连接，ffmpeg 底层会处理 rtmp 和 tcp 握手···
        //    bool res = m_muxer->Open();
        //    if (res)
        //    {
        //        // 开启消费线程，发送数据到数据流中(依赖 av_interleaved_write_frame)
        //        if (!send_MuxThreadProc_to_threads())
        //        {
        //            return false;
        //        }
        //    }
        //}

        //RtmpMuxer::ptr mux_ptr = std::dynamic_pointer_cast<RtmpMuxer>(m_muxer);
        //if (!mux_ptr || !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder())
        //{
        //    LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<LocalMuxer>(m_muxer) "
        //        "failed or !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder() is nullptr";
        //    return false;
        //}

        //// 获取音视频编码器
        //SetVideoEncoder(mux_ptr->get_video_encoder());
        //SetAudioEncoder(mux_ptr->get_audio_encoder());

        // 开启消费线程，发送数据到数据流中(依赖 av_interleaved_write_frame)，将 muxer 的创建逻辑放到该消费者线程开头
        if (!send_MuxThreadProc_to_threads())
        {
            return false;
        }

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
        // 在消费者线程中，才开始执行 avformat_open_input，现在这样调用非常不好，但是要改的太多了，未来再说
        do
        {
            // 创建封装器 -- 
            m_muxer = RtmpMuxer::createNew(m_url);

            RtmpMuxer::ptr mux_ptr = std::dynamic_pointer_cast<RtmpMuxer>(m_muxer);
            if (!mux_ptr)
            {
                LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<RtmpMuxer>(m_muxer) "
                    "failed or !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder() is nullptr";
                return;
            }

            // 获取音视频编码器
            SetVideoEncoder(mux_ptr->get_video_encoder());
            SetAudioEncoder(mux_ptr->get_audio_encoder());
        } while (0);

        auto vEncPtr = std::dynamic_pointer_cast<VideoFfmpegEncoder>(m_vEncoder);
        auto aEncPtr = std::dynamic_pointer_cast<AudioFfmpegEncoder>(m_aEncoder);

        auto dxgiCap = vEncPtr ? vEncPtr->getDxgiCap() : nullptr;
        auto audioCap = aEncPtr ? aEncPtr->get_audioCap_shared() : nullptr;

        auto dshow_audioCap = std::dynamic_pointer_cast<streamer::DshowAudioCapture>(audioCap);

        bool is_only_video = false;
        bool is_only_audio = false;

        AVRational v_tb = vEncPtr ? vEncPtr->get_vOut_time_base() : AVRational{ 0, 0 };
        AVRational a_tb = aEncPtr ? aEncPtr->get_aOut_time_base() : AVRational{ 0, 0 };

        do
        {
            if ((v_tb.den + v_tb.num) == 0)
                is_only_audio = true;
            if ((a_tb.den + a_tb.num) == 0)
                is_only_video = true;

        } while (0);

        bool done = false;

        // 首次来时，清空音频缓存队列，尝试一下，是否可以弥补差距，实在不行，就采用绝对时间基准，强制让video、audio(丢弃前面的帧)同步
        //if(dshow_audioCap) 
        //{
        //    dshow_audioCap->drain_audio_fifo_size();
        //}

        // Ensure to only encode when valid pointers exist to avoid crash if components missing
        while (true) 
        {
            if (!done && SDL::GetInstance()->get_state() == STATE::Term)
            {
                done = true;
            }
            int cmp = av_compare_ts(m_last_video_pts, v_tb, m_last_audio_pts, a_tb);
            // LOG_INFO(g_logger) << "m_last_video_pts: " << m_last_video_pts << " m_lase_audio_pts: " << m_last_audio_pts;

            if (done)
            {
                static bool video_queue_empty = false;
                static bool audio_queue_empty = false;

                if (dshow_audioCap && !audio_queue_empty)
                {
                    if (dshow_audioCap->get_audio_queue_size() < m_aEncoder->getCtx()->frame_size)
                    {
                        LOG_INFO(g_logger) << "The audio_queue_size < nbSamples(" << m_aEncoder->getCtx()->frame_size << ')';
                        audio_queue_empty = true;
                    }
                }

                if (dxgiCap && !video_queue_empty)
                {
                    DxgiScreenCapture::ptr dxgiCap_ptr = std::dynamic_pointer_cast<DxgiScreenCapture>(dxgiCap);
                    if (dxgiCap_ptr && dxgiCap_ptr->GetQueueSize() == 0)
                    {
                        LOG_INFO(g_logger) << "The video_queue_size == 0";
                        video_queue_empty = true;
                    }
                }

                if ((video_queue_empty || is_only_audio) && (audio_queue_empty || is_only_video))
                {
                    LOG_INFO(g_logger) << "MuxThreadProc()::while(true) is break";
                    break;
                }

                do
                {
                    if (video_queue_empty)
                        cmp = 1;
                    else if (audio_queue_empty)
                        cmp = 0;
                } while (0);
            }

            do
            {
                if (is_only_video)
                    cmp = -1;
                else if (is_only_audio)
                    cmp = 1;
            } while (0);

            if (cmp <= 0) 
            {
                if (dxgiCap) 
                {
                    FramePtr vFrame = dxgiCap->ReadFrame();
                    if (!vFrame)
                    {
                        continue;
                    }

                    m_vEncoder->Encode(vFrame, [&](AVPacket* pkt) -> int {
                        m_last_video_pts = pkt->pts;
                        return m_muxer->WritePacket(pkt) ? 0 : -1;
                        });
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
                            m_last_audio_pts = pkt->pts;
                            return m_muxer->WritePacket(pkt) ? 0 : -1;
                            });
                    }
                }
            }

            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        int ret = 0;
        if(m_vEncoder)
        {
            ret = m_vEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });
            if (ret < 0)
            {
                LOG_WARN(g_logger) << "m_vEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });";
            }
        }

        if(m_aEncoder)
        {
            m_aEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });
            if (ret < 0)
            {
                LOG_WARN(g_logger) << "m_aEncoder->Flush([&](AVPacket* pkt) { return m_muxer->WritePacket(pkt) ? 0 : -1; });";
            }
        }

        LOG_INFO(g_logger) << "RtmpStreamer::MuxThreadProc() is finished";
    }

    bool RtmpStreamer::send_MuxThreadProc_to_threads(bool Immediately)
    {
        int threads = -1;

        if (!Immediately)
        {
            threads = SDL::GetInstance()->get_threadfuncs_counts();
            // 注册线程函数，不是立即开启
            SDL::GetInstance()->push_thread_to_vector(std::bind(&RtmpStreamer::MuxThreadProc, this));
            if (threads == SDL::GetInstance()->get_threadfuncs_counts())
            {
                LOG_ERROR(g_logger) << "LocalFileStreamer::send_MuxThreadProc_to_threads() failed";
                return false;
            }
        }
        else
        {
            threads = SDL::GetInstance()->get_threads_counts();
            // 注册线程函数，不是立即开启
            SDL::GetInstance()->push_threadfunc_to_threads(std::bind(&RtmpStreamer::MuxThreadProc, this));
            if (threads == SDL::GetInstance()->get_threads_counts())
            {
                LOG_ERROR(g_logger) << "LocalFileStreamer::send_MuxThreadProc_to_threads() failed";
                return false;
            }
        }

        return true;
    }

} // namespace streamer
