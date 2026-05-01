#include "factory/ComponentFactory.h"

#include "adapters/capture/DshowAudioCapture.h"
#include "adapters/capture/DxgiScreenCapture.h"
#include "adapters/codec/FfmpegEncoder.h"
#include "adapters/output/RtmpMuxerStreamer.h"
#include "adapters/output/FfmpegMuxerStreamer.h"

namespace streamer {

IScreenCapture::ptr ComponentFactory::CreateScreenCapture(const std::string& type) {
    if (type == "dxgi") {
        return DxgiScreenCapture::createNew();
    }
    return nullptr;
}

IAudioCapture::ptr ComponentFactory::CreateAudioCapture(const std::string& type) {
    if (type == "dshow") {
        return nullptr;
    }
    return nullptr;
}

IEncoder::ptr ComponentFactory::CreateEncoder(const std::string&) {
    return VideoFfmpegEncoder::createNew(0, AVRational{1, 25});
}

IMuxer::ptr ComponentFactory::CreateMuxer(const std::string& type) {
    if (type == "rtmp") {
        return std::make_shared<RtmpMuxer>();
    }
    else if(type == "localfile")
    {
        return LocalMuxer::createNew();
    }
    return nullptr;
}

IStreamer::ptr ComponentFactory::CreateStreamer(const std::string& type) {
    if (type == "rtmp") {
        return std::make_shared<RtmpStreamer>();
    }
    return nullptr;
}

} // namespace streamer
