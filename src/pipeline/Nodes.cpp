#include "pipeline/Nodes.h"

namespace streamer {

ScreenCaptureNode::ScreenCaptureNode(
    std::unique_ptr<IScreenCapture> cap,
    std::shared_ptr<IConcurrentQueue<FramePtr>> out)
    : cap_(std::move(cap)), out_(std::move(out)) {}

bool ScreenCaptureNode::OnInit() {
    return cap_ && cap_->Open();
}

void ScreenCaptureNode::OnStop() {
    if (cap_) {
        cap_->Close();
    }
}

void ScreenCaptureNode::OnRelease() {}

void ScreenCaptureNode::ProcessLoop() {
    while (running_.load(std::memory_order_acquire)) {
        auto frame = cap_->ReadFrame();
        if (frame) {
            out_->Push(std::move(frame));
        }
    }
}

AudioCaptureNode::AudioCaptureNode(
    std::unique_ptr<IAudioCapture> cap,
    std::shared_ptr<IConcurrentQueue<FramePtr>> out)
    : cap_(std::move(cap)), out_(std::move(out)) {}

bool AudioCaptureNode::OnInit() {
    return cap_ && cap_->Open();
}

void AudioCaptureNode::OnStop() {
    if (cap_) {
        cap_->Close();
    }
}

void AudioCaptureNode::OnRelease() {}

void AudioCaptureNode::ProcessLoop() {
    while (running_.load(std::memory_order_acquire)) {
        auto frame = cap_->ReadFrame();
        if (frame) {
            out_->Push(std::move(frame));
        }
    }
}

EncodeNode::EncodeNode(
    std::unique_ptr<IEncoder> encoder,
    std::shared_ptr<IConcurrentQueue<FramePtr>> in,
    std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> out)
    : encoder_(std::move(encoder)), in_(std::move(in)), out_(std::move(out)) {}

bool EncodeNode::OnInit() {
    return encoder_ && encoder_->Open();
}

void EncodeNode::OnStop() {
    if (encoder_) {
        encoder_->Close();
    }
    if (in_) {
        in_->Close();
    }
}

void EncodeNode::OnRelease() {}

void EncodeNode::ProcessLoop() {
    while (running_.load(std::memory_order_acquire)) {
        auto frame = in_->WaitAndPop();
        if (!frame) {
            continue;
        }

        auto packets = encoder_->Encode(frame);
        for (auto& packet : packets) {
            out_->Push(std::move(packet));
        }
    }
}

StreamNode::StreamNode(
    std::unique_ptr<IMuxer> muxer,
    std::unique_ptr<IStreamer> streamer,
    std::shared_ptr<IConcurrentQueue<PacketWrapperPtr>> in)
    : muxer_(std::move(muxer)), streamer_(std::move(streamer)), in_(std::move(in)) {}

bool StreamNode::OnInit() {
    return muxer_ && streamer_ && muxer_->Open() && streamer_->Connect();
}

void StreamNode::OnStop() {
    if (in_) {
        in_->Close();
    }
    if (streamer_) {
        streamer_->Disconnect();
    }
    if (muxer_) {
        muxer_->Close();
    }
}

void StreamNode::OnRelease() {}

void StreamNode::ProcessLoop() {
    while (running_.load(std::memory_order_acquire)) {
        auto packet = in_->WaitAndPop();
        if (!packet) {
            continue;
        }

        muxer_->WritePacket(packet);
        streamer_->SendPacket(packet);
    }
}

} // namespace streamer
