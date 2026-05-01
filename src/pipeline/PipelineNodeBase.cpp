#include "pipeline/PipelineNodeBase.h"

namespace streamer {

bool PipelineNodeBase::Init(AppContext& ctx) {
    ctx_ = &ctx;
    return OnInit();
}

bool PipelineNodeBase::Start() {
    running_.store(true, std::memory_order_release);
    worker_ = std::thread([this] { ProcessLoop(); });
    return true;
}

void PipelineNodeBase::Stop() {
    running_.store(false, std::memory_order_release);
    OnStop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void PipelineNodeBase::Release() {
    OnRelease();
    ctx_ = nullptr;
}

} // namespace streamer
