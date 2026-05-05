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

    RtmpMuxer::RtmpMuxer(const std::string& url) 
        : m_url(url), m_close(false), m_vIndex(0), m_aIndex(0), m_ctx(nullptr) 
    {
    }

    RtmpMuxer::~RtmpMuxer() noexcept 
    {
        Close();
    }

    IMuxer::ptr RtmpMuxer::createNew(const std::string& url) {
        return std::make_shared<RtmpMuxer>(url);
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

        if (!needVideo && !needAudio) return false;

        if (needVideo)
        {
            v_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!v_outStream) return false;

            if (!m_vEncoder)
            {
                m_vEncoder = VideoFfmpegEncoder::createNew(v_outStream->index, AVRational{ 1, 25 });
            }

            if (!m_vEncoder || !m_vEncoder->getCtx()) return false;

            ret = avcodec_parameters_from_context(v_outStream->codecpar, m_vEncoder->getCtx());
            if (ret < 0) return false;

            v_outStream->codecpar->codec_tag = 0;
            m_vIndex = v_outStream->index;
        }

        if (needAudio)
        {
            a_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!a_outStream) return false;

            AVCodecID audioCodecId = m_ctx->oformat->audio_codec;
            m_aEncoder = AudioFfmpegEncoder::createNew(audioCodecId, { 0, 0 }, a_outStream->index);
            if (!m_aEncoder || !m_aEncoder->getCtx()) return false;

            ret = avcodec_parameters_from_context(a_outStream->codecpar, m_aEncoder->getCtx());
            if (ret < 0) return false;

            a_outStream->codecpar->codec_tag = 0;
            m_aIndex = a_outStream->index;
        }

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
        if (!packet || !packet->Get()) return false;
        return WritePacket(packet->Get());
    }

    bool RtmpMuxer::WritePacket(AVPacket* packet) 
    {
        if (!m_ctx || !packet) return false;
        int ret = av_interleaved_write_frame(m_ctx, packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
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
            m_ctx = nullptr;
        }
    }

    ///////////////////////// streamer //////////////////////////////

    RtmpStreamer::RtmpStreamer(const std::string& url)
        : m_url(url) 
    {
    }

    RtmpStreamer::~RtmpStreamer()
    {
        Disconnect();
    }

    IStreamer::ptr RtmpStreamer::createNew(const std::string& url) 
    {
        return std::make_shared<RtmpStreamer>(url);
    }

    bool RtmpStreamer::Connect() {
        if (m_muxer)
        {
            bool res = m_muxer->Open();
            if (res) 
            {
                return send_MuxThreadProc_to_threads();
            }
        }
        return false;
    }

    bool RtmpStreamer::SendPacket(const PacketWrapperPtr& packet) {
        return true;
    }

    void RtmpStreamer::Disconnect() {
    if (m_muxer) {
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

        AVRational v_tb = vEncPtr ? vEncPtr->get_vOut_time_base() : AVRational{1, 1000};
        AVRational a_tb = aEncPtr ? aEncPtr->get_aOut_time_base() : AVRational{1, 1000};

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
                if (dxgiCap) {
                    FramePtr vFrame = dxgiCap->ReadFrame(v_frame_idx);
                    if (vFrame && dxgiCap->isPass(v_frame_idx)) 
                    {
                        m_vEncoder->Encode(vFrame, [&](AVPacket* pkt) -> int {
                            m_last_video_pts = pkt->pts;
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
                            m_last_audio_pts = pkt->pts;
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
