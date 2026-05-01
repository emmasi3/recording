#include "infra/FfmpegContextManager.h"


namespace streamer {

void AVFormatContextDeleter::operator()(AVFormatContext* p) const noexcept {
    if (p->pb)
        avio_close(p->pb);
    avformat_free_context(p);
}

void AVCodecContextDeleter::operator()(AVCodecContext* p) const noexcept {
    avcodec_free_context(&p);
}

void AVFrameDeleter::operator()(AVFrame* p) const noexcept {
    (void)p;
}

bool FFmpegContextManager::Init() {
    return true;
}

void FFmpegContextManager::Shutdown() {
}

AVFormatContextPtr FFmpegContextManager::CreateFormatContext() {
    return AVFormatContextPtr(nullptr);
}

AVCodecContextPtr FFmpegContextManager::CreateCodecContext() {
    return AVCodecContextPtr(nullptr);
}

AVFramePtr FFmpegContextManager::CreateFrame() {
    return AVFramePtr(nullptr);
}

} // namespace streamer
