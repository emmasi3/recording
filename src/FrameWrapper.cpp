#include "core/FrameWrapper.h"


namespace streamer {

FrameWrapper::~FrameWrapper() {
    Release();
}

void FrameWrapper::Release() noexcept {
    if (frm_ && owned_) {
        av_frame_free(&frm_);
        frm_ = nullptr;
        owned_ = false;
    }
}

} // namespace streamer
