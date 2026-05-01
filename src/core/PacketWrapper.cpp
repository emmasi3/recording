#include "core/PacketWrapper.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace streamer {

    
void AVPacketDeleter::operator()(AVPacket* pkt) const noexcept {
    av_packet_free(&pkt);
}

PacketWrapper::PacketWrapper(PacketPtr pkt) :
    m_pkt(std::move(pkt))
{

}

void PacketWrapper::SetPtsDtsUs(int64_t pts, int64_t dts)
{
    m_pkt->pts = pts;
    m_pkt->dts = dts;
    //
    m_pts = m_pkt->pts;
    m_dts = m_pkt->dts;
}

void PacketWrapper::Packet_rescale_ts(AVRational src, AVRational dst)
{
    av_packet_rescale_ts(m_pkt.get(), src, dst);
}

void PacketWrapper::Pakcet_unref()
{
    if (m_pkt.get())
    {
        av_packet_unref(m_pkt.get());
    }
}

} // namespace streamer
