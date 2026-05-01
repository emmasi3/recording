#include "config/Config.h"

namespace streamer {

bool JsonConfigProvider::Load(const std::string& path) {
    std::scoped_lock lock(mtx_);
    path_ = path;
    return true;
}

AppConfig JsonConfigProvider::GetSnapshot() const {
    std::scoped_lock lock(mtx_);
    return config_;
}

void JsonConfigProvider::Subscribe(Callback cb) {
    std::scoped_lock lock(mtx_);
    callbacks_.push_back(std::move(cb));
}

void JsonConfigProvider::PollChanges() {
    std::vector<Callback> callbacks;
    AppConfig snapshot;

    {
        std::scoped_lock lock(mtx_);
        callbacks = callbacks_;
        snapshot = config_;
    }

    for (auto& cb : callbacks) {
        cb(snapshot);
    }
}

} // namespace streamer
