#pragma once
#include "libav_h.h"
#include <memory>
#include <cstdint>


namespace streamer {
/// <summary>
/// 封装 FFmpeg `AVFrame` 的简单 RAII 类型。
/// 说明：为避免在头文件中包含 FFmpeg 头，这里仅使用指针并在 .cpp 中实现析构逻辑。
/// </summary>
class FrameWrapper {
public:
    using ptr = std::shared_ptr<FrameWrapper>;

    FrameWrapper() = default;
    /// 构造时传入底层 AVFrame 指针，owned 指示是否由本对象负责释放。
    explicit FrameWrapper(AVFrame* frm, bool owned = true) : frm_(frm), owned_(owned) {}

    FrameWrapper(FrameWrapper&& other) noexcept : frm_(other.frm_), owned_(other.owned_) {
        other.frm_ = nullptr; other.owned_ = false;
    }
    FrameWrapper& operator=(FrameWrapper&& other) noexcept {
        if (this != &other) {
            // 释放当前
            this->Release();
            frm_ = other.frm_;
            owned_ = other.owned_;
            other.frm_ = nullptr; other.owned_ = false;
        }
        return *this;
    }

    FrameWrapper(const FrameWrapper&) = delete;
    FrameWrapper& operator=(const FrameWrapper&) = delete;

    ~FrameWrapper();

    /// 返回底层 AVFrame 指针（不转移所有权）。
    AVFrame* Get() const noexcept { return frm_; }

    int64_t PtsUs() const noexcept { return ptsUs_; }
    int64_t DtsUs() const noexcept { return dtsUs_; }
    void SetPtsDtsUs(int64_t pts, int64_t dts) noexcept { ptsUs_ = pts; dtsUs_ = dts; }

private:
    void Release() noexcept;

    AVFrame* frm_{nullptr};
    bool owned_{false};
    int64_t ptsUs_{0};
    int64_t dtsUs_{0};
};

using FrameWrapperPtr = std::shared_ptr<FrameWrapper>;

} // namespace streamer
