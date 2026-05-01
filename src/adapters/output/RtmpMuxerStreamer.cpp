#include "adapters/output/RtmpMuxerStreamer.h"

namespace streamer {

bool RtmpMuxer::Open() {
    return true;
}

bool RtmpMuxer::WritePacket(const PacketWrapperPtr&) {
    return true;
}

bool RtmpMuxer::WritePacket(AVPacket*) {
    return true;
}

void RtmpMuxer::Close() {
}

bool RtmpStreamer::Connect() {
    return true;
}

bool RtmpStreamer::SendPacket(const PacketWrapperPtr&) {
    return true;
}

void RtmpStreamer::Disconnect() {
}

} // namespace streamer
