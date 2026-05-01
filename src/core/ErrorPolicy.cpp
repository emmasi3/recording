#include "core/ErrorPolicy.h"

namespace streamer {

ErrorAction DefaultErrorPolicy::OnError(const ErrorInfo& error) {
    switch (error.code) {
    case ErrorCode::NetworkFailed:
    case ErrorCode::DeviceOpenFailed:
        return ErrorAction::Retry;
    case ErrorCode::InvalidState:
        return ErrorAction::Stop;
    default:
        return ErrorAction::Stop;
    }
}

uint32_t ExponentialReconnectPolicy::NextBackoffMs() {
    const auto value = currentMs_;
    currentMs_ = (currentMs_ * 2 > maxMs_) ? maxMs_ : currentMs_ * 2;
    return value;
}

void ExponentialReconnectPolicy::Reset() {
    currentMs_ = 500;
}

} // namespace streamer
