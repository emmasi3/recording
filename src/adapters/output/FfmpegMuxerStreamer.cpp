#include "adapters/output/FfmpegMuxerStreamer.h"
#include "event/ThreadEventSDL.h"
#include "adapters/capture/DshowAudioCapture.h"

namespace streamer
{
    static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

    streamer::LocalMuxer::LocalMuxer(const std::string& filename)
        :m_close(false),
        m_filename(filename),
        m_vEncoder(nullptr),
        m_aEncoder(nullptr),
        m_aIndex(-1),
        m_vIndex(-1)
    {
    }

    streamer::LocalMuxer::~LocalMuxer()
    {
        if (!m_close)
        {
            Close();
        }
    }

    IMuxer::ptr LocalMuxer::createNew(const std::string& filename)
    {
        IMuxer::ptr ptr = std::make_shared<LocalMuxer>(filename);
        // 打开输出上下文
        if (!ptr || !ptr->Open())
        {
            LOG_ERROR(g_logger) << "LocalMuxer::createNew() -- Open() fail";
            return nullptr;
        }

        return ptr;
    }

    bool streamer::LocalMuxer::Open()
    {
        int ret = 0;
        // 1、创建目标文件上下文，文件路径
        AVStream* v_outStream = nullptr;
        AVStream* a_outStream = nullptr;

        // 防止重复打开
        if (m_ctx)
        {
            LOG_WARN(g_logger) << "LocalMuxer::Open called repeatedly.";
            return true;
        }

        ret = avformat_alloc_output_context2(&m_ctx, nullptr, nullptr, m_filename.c_str());
        if (ret < 0 || !m_ctx || !m_ctx->oformat)
        {
            LOG_ERROR(g_logger) << "aformat_alloc_output_context2 failed, file= " << m_filename;
            return false;
        }


        // 2) 根据后缀做“意图路由”：只音频 / 只视频 / 音视频
        auto to_lower = [](std::string s) {
            for (char& c : s)
            {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
            return s;
            };

        std::string ext;
        {
            const size_t dot = m_filename.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < m_filename.size())
                ext = to_lower(m_filename.substr(dot + 1));
        }

        const bool forceAudioOnly =
            (ext == "aac" || ext == "mp3" || ext == "wav" || ext == "flac" || ext == "opus" || ext == "m4a");
        const bool forceVideoOnly =
            (ext == "h264" || ext == "264" || ext == "h265" || ext == "265" || ext == "hevc");

        const bool ofmtHasVideo = (m_ctx->oformat->video_codec != AV_CODEC_ID_NONE);
        const bool ofmtHasAudio = (m_ctx->oformat->audio_codec != AV_CODEC_ID_NONE);

        /*
        * @brief 判断该媒体文件是否需要 音频流、视频流。
        * 判断依据 -- （1）ext 文件名后缀;（2）格式上下文的 oformat->video_codec && oformat->audio_codec 是否被初始化
        * 若被初始化，肯定是该媒体文件后缀格式可以创建有该媒体流，也就是 .mp4 可以开辟 video、audio、··· 媒体流
        * 另外一个就是用户希望的，也就是文件后缀名，两者共同决定，要先支持(可以创建该流)、才能真正创建
        */
        bool needVideo = ofmtHasVideo && !forceAudioOnly;
        bool needAudio = ofmtHasAudio && !forceVideoOnly;

        // 两边都不需要/不支持时，直接失败
        if (!needVideo && !needAudio)
        {
            LOG_ERROR(g_logger) << "Output format supports neither selected video nor audio. file=" << m_filename;
            return false;
        }

        // 3) 创建视频流（按需）
        if (needVideo)
        {
            v_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!v_outStream)
            {
                LOG_ERROR(g_logger) << "avformat_new_stream(video) failed.";
                return false;
            }

            if (!m_vEncoder)
            {
                m_vEncoder = VideoFfmpegEncoder::createNew(v_outStream->index, AVRational{ 1, 25 });
            }

            if (!m_vEncoder || !m_vEncoder->getCtx())
            {
                LOG_ERROR(g_logger) << "Video encoder is null or invalid.";
                return false;
            }

            ret = avcodec_parameters_from_context(v_outStream->codecpar, m_vEncoder->getCtx());
            if (ret < 0)
            {
                LOG_ERROR(g_logger) << "avcodec_parameters_from_context(video) failed.";
                return false;
            }

            v_outStream->codecpar->codec_tag = 0;
            m_vIndex = v_outStream->index;
        }
        else
        {
            m_vIndex = -1;
        }

        // 4) 创建音频流（按需）
        if (needAudio)
        {
            a_outStream = avformat_new_stream(m_ctx, nullptr);
            if (!a_outStream)
            {
                LOG_ERROR(g_logger) << "avformat_new_stream(audio) failed.";
                return false;
            }

            AVCodecID audioCodecId = m_ctx->oformat->audio_codec;
            m_aEncoder = AudioFfmpegEncoder::createNew(audioCodecId, { 0, 0 }, a_outStream->index);
            if (!m_aEncoder || !m_aEncoder->getCtx())
            {
                LOG_ERROR(g_logger) << "AudioFfmpegEncoder::createNew failed.";
                return false;
            }

            ret = avcodec_parameters_from_context(a_outStream->codecpar, m_aEncoder->getCtx());
            if (ret < 0)
            {
                LOG_ERROR(g_logger) << "avcodec_parameters_from_context(audio) failed.";
                return false;
            }

            a_outStream->codecpar->codec_tag = 0;
            m_aIndex = a_outStream->index;
        }
        else
        {
            m_aIndex = -1;
        }

        // 5) 打开 IO
        if (!(m_ctx->oformat->flags & AVFMT_NOFILE))
        {
            ret = avio_open2(&m_ctx->pb, m_filename.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
            if (ret < 0)
            {
                LOG_ERROR(g_logger) << "avio_open2 failed: " << m_filename;
                return false;
            }
        }

        // 6) 写文件头
        ret = avformat_write_header(m_ctx, nullptr);
        if (ret < 0)
        {
            LOG_ERROR(g_logger) << "avformat_write_header failed.";
            return false;
        }

        // 7) 头写完后，修正音视频编码器输出 time_base
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

        // 记录

        return true;
    }

    void LocalMuxer::Close()
    {
        // 写文件尾
        int ret = av_write_trailer(m_ctx);
        if (ret < 0)
        {
            LOG_ERROR(g_logger) << "av_write_trailer failed!";
        }

        // 关闭上下文并释放资源
        if (m_ctx)
        {
            if (m_ctx->pb)
                avio_close(m_ctx->pb);
            avformat_free_context(m_ctx);
        }

        m_close = true;
    }

    bool LocalMuxer::WritePacket(const PacketWrapperPtr& packet)
    {
        LOG_WARN(g_logger) << "bool LocalMuxer::WritePacket(const PacketWrapperPtr& packet) not define, "
            "please use WritePacket(AVPacket* packet)";
        return false;
    }

    bool LocalMuxer::WritePacket(AVPacket* packet)
    {
        int ret = 0;
        // 写入输出文件
        ret = av_interleaved_write_frame(m_ctx, packet);
        if (ret < 0 && ret != AVERROR(EAGAIN))
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));

            LOG_ERROR(g_logger) << "av_interleaved_write_frame failed: " << errbuf;
            return false;
        }
        else if(ret == AVERROR(EAGAIN))
        {
            LOG_INFO(g_logger) << "av_interleaved_write_frame() return ret: " << ret;
        }

        return true;
    }

    ////////////////////////////////////////// IStreamer ////////////////////////////////////////////////

    LocalFileStreamer::LocalFileStreamer(const std::string& filename)
        :m_filename(filename),
        m_muxer(nullptr),
        m_aEncoder(nullptr),
        m_vEncoder(nullptr)
    {
        if (m_filename.empty())
        {
            LOG_WARN(g_logger) << "LocalFilename is empty()";
        }

        // 创建封装器
        // m_muxer = LocalMuxer::createNew(m_filename);
    }

    LocalFileStreamer::~LocalFileStreamer()
    {
        if (SDL::GetInstance()->get_state() == STATE::Term)
        {
            Disconnect();
        }
    }

    IStreamer::ptr LocalFileStreamer::createNew(const std::string& filename)
    {
        IStreamer::ptr ptr = std::make_shared<LocalFileStreamer>(filename);
        
        // 连接目标IO流 / 初始化部件(获取部件智能指针)
        if (!ptr->Connect())
        {
            LOG_ERROR(g_logger) << "LocalFileStreamer::Connext() return false";
            return nullptr;
        }

        return ptr;
    }

    bool LocalFileStreamer::Connect()
    {
        //LocalMuxer::ptr mux_ptr = std::dynamic_pointer_cast<LocalMuxer>(m_muxer);
        //if (!mux_ptr)
        //{
        //    LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<LocalMuxer>(m_muxer) "
        //        "failed or !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder() is nullptr";
        //    return false;
        //}

        //// 获取音视频编码器
        //SetVideoEncoder(mux_ptr->get_video_encoder());
        //SetAudioEncoder(mux_ptr->get_audio_encoder());
        
        // 开始写入流(线程队列)
        if (!send_MuxThreadProc_to_threads())
        {
            return false;
        }

        return true;
    }

    bool LocalFileStreamer::SendPacket(const PacketWrapperPtr& packet)
    {
        LOG_WARN(g_logger) << "please use to LocalFileStreamer::MuxThreadProc(), there is no need to use this method";
        return true;
    }

    void LocalFileStreamer::Disconnect()
    {

    }

    void LocalFileStreamer::MuxThreadProc()
    {
        // 在消费者线程中，才开始执行 avformat_open_input，现在这样调用非常不好，但是要改的太多了，未来再说
        do
        {
            // 创建封装器 -- 
            m_muxer = LocalMuxer::createNew(m_filename);

            LocalMuxer::ptr mux_ptr = std::dynamic_pointer_cast<LocalMuxer>(m_muxer);
            if (!mux_ptr)
            {
                LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<LocalMuxer>(m_muxer) "
                    "failed or !mux_ptr->get_video_encoder() || !mux_ptr->get_audio_encoder() is nullptr";
                return;
            }

            // 获取音视频编码器
            SetVideoEncoder(mux_ptr->get_video_encoder());
            SetAudioEncoder(mux_ptr->get_audio_encoder());
        } while (0);

        // 标记是否单独 video、audio
        bool is_only_video = false;
        bool is_only_audio = false;

        // 这里利用 RTTI (dynamic_cast) 拿到音频和视频具体的 capture 接口
        auto vEncPtr = std::dynamic_pointer_cast<VideoFfmpegEncoder>(m_vEncoder);
        auto aEncPtr = std::dynamic_pointer_cast<AudioFfmpegEncoder>(m_aEncoder);

        auto dxgiCap = vEncPtr ? vEncPtr->getDxgiCap() : nullptr;
        auto audioCap = aEncPtr ? aEncPtr->get_audioCap_shared() : nullptr;
        // 获取音频采集器具体子类智能指针
        auto dshow_audioCap = std::dynamic_pointer_cast<streamer::DshowAudioCapture>(audioCap);
        if (!dshow_audioCap && audioCap)
        {
            LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<streamer::DshowAudioCapture>(audioCap) return nullptr";
        }
        // 局部视频帧时间戳 -- 用来给到 dxgiCap->ReadFrame(v_frame_idx) 内部判断是否到时间了
        int64_t v_frame_idx = 0;

        // 音视频时间基(TimeBase) -- 输出上下文
        AVRational v_tb = vEncPtr ? vEncPtr->get_vOut_time_base() : AVRational{ 0, 0 };
        AVRational a_tb = aEncPtr ? aEncPtr->get_aOut_time_base() : AVRational{ 0, 0 };

        // 检查是否为 only_video_or_audio ？
        do
        {
            if ((v_tb.den + v_tb.num) == 0)
                is_only_audio = true;
            if ((a_tb.den + a_tb.num) == 0)
                is_only_video = true;

        } while (0);

        // 标志 Term 命令是否触发，以此执行结束逻辑
        bool done = false;

        // 清空音频队列
        if (!is_only_video && dshow_audioCap)
        {
            dshow_audioCap->drain_audio_fifo_size();
        }

        while (true) 
        {
            // 检查是否 Term && 修改done状态
            if (!done && SDL::GetInstance()->get_state() == STATE::Term)
            {
                done = true;
            }
            // 利用 FFmpeg 函数 compare 时间戳决定写哪个流
            // av_compare_ts 返回 < 0 表示 a的pts早于b的pts
            int cmp = av_compare_ts(m_last_video_pts, v_tb, m_last_audio_pts, a_tb);

            // 执行done逻辑
            if (done)
            {
                // 由于视频采集逻辑的特殊性，这里简单处理为 -- 只处理音频队列(因为视频帧要硬解码，没有队列)
                cmp = 1;
                // 判断 Term 命令下发后，音频缓冲队列是否还有完整一帧数据可读，若无，直接 break
                if(dshow_audioCap)
                {
                    if (dshow_audioCap->get_audio_queue_size() < m_aEncoder->getCtx()->frame_size)
                    {
                        LOG_DEBUG(g_logger) << "The audio_queue_size < nbSamples(" << m_aEncoder->getCtx()->frame_size << ')';
                        break;
                    }
                }

                // 只有video
                if (!dshow_audioCap && is_only_video)
                {
                    break;
                }
            }

            // 修正 cmp -- 根据 is_only_video/audio
            do
            {
                if (is_only_video)
                    cmp = -1;
                else if (is_only_audio)
                    cmp = 1;
            } while (0);


            if (cmp <= 0) 
            {
                // ========= 视频时间更靠前，录制视频 ==============
                // 此处为非阻塞等待，但是一旦视频"慢"了，在主循环中就会直接执行 cmp <= 0 分支，保证下一帧写入的一定是视频帧
                FramePtr vFrame = dxgiCap->ReadFrame(v_frame_idx);
                if(vFrame)
                {
                    // 在有帧的情况下才做判断，否则会出现问题
                    if (dxgiCap->isPass(v_frame_idx))
                    {
                        m_vEncoder->Encode(vFrame, [&](AVPacket* pkt) -> int {
                            // 重调时间戳（如果编码器内没处理的话）
                            m_last_video_pts = pkt->pts;
                            return m_muxer->WritePacket(pkt) ? 0 : -1;
                            });
                        ++v_frame_idx;
                    }
                }
            }
            else
            {
                // ========= 音频时间更靠前，录制音频 ==============
                // 此处为阻塞等待
                FramePtr aFrame = audioCap->ReadFrame();
                if (aFrame) 
                {
                    m_aEncoder->Encode(aFrame, [&](AVPacket* pkt) -> int {
                        m_last_audio_pts = pkt->pts;
                        return m_muxer->WritePacket(pkt) ? 0 : -1;
                        });
                }
            }

            // 可选：无数据时小休眠防空转卡死
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // -- 退出，刷出编码器残留帧 Flush -- 优先视频，这里因为Flush设计思路，没有对最后的音视频帧做同步处理
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

        LOG_DEBUG(g_logger) << "LocalFileStream::MuxThreadProc() is finished";
    }

    bool LocalFileStreamer::send_MuxThreadProc_to_threads(bool Immediately)
    {
        int threads = -1;

        if (!Immediately)
        {
            threads = SDL::GetInstance()->get_threadfuncs_counts();
            // 注册线程函数，不是立即开启
            SDL::GetInstance()->push_thread_to_vector(std::bind(&LocalFileStreamer::MuxThreadProc, this));
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
            SDL::GetInstance()->push_threadfunc_to_threads(std::bind(&LocalFileStreamer::MuxThreadProc, this));
            if (threads == SDL::GetInstance()->get_threads_counts())
            {
                LOG_ERROR(g_logger) << "LocalFileStreamer::send_MuxThreadProc_to_threads() failed";
                return false;
            }
        }

        return true;
    }

}