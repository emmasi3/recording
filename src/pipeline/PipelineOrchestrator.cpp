#include "pipeline/PipelineOrchestrator.h"

namespace streamer {

PipelineOrchestrator::PipelineOrchestrator(AppContext ctx)
    : ctx_(std::move(ctx)) {}

void PipelineOrchestrator::AddNode(std::shared_ptr<IPipelineNode> node) {
    nodes_.push_back(std::move(node));
}

bool PipelineOrchestrator::Init() {
    for (auto& node : nodes_) {
        if (!node->Init(ctx_)) {
            return false;
        }
    }
    return true;
}

bool PipelineOrchestrator::Start() {
    for (auto& node : nodes_) {
        if (!node->Start()) {
            return false;
        }
    }
    return true;
}

void PipelineOrchestrator::Stop() {
    for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
        (*it)->Stop();
    }
}

void PipelineOrchestrator::Release() {
    for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
        (*it)->Release();
    }
}

} // namespace streamer
