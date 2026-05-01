#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace streamer {

/// <summary>
/// 模块作用：定义运行时配置结构与配置源接口。
/// 用途：支持文件配置读取与热更新通知扩展。
/// </summary>

/// <summary>
/// 应用配置快照。
/// </summary>
struct AppConfig {
    /// <summary>RTMP 推流地址。</summary>
    std::string rtmpUrl;
    /// <summary>视频目标码率（kbps）。</summary>
    int videoBitrateKbps{4000};
    /// <summary>音频目标码率（kbps）。</summary>
    int audioBitrateKbps{128};
    /// <summary>目标帧率。</summary>
    int fps{25};
    /// <summary>目标宽度。</summary>
    int width{1920};
    /// <summary>目标高度。</summary>
    int height{1080};
};

/// <summary>
/// 配置提供者接口。
/// </summary>
class IConfigProvider {
public:
    /// <summary>配置变更回调类型。</summary>
    using Callback = std::function<void(const AppConfig&)>;

    /**
     * @brief 虚析构
     */
    virtual ~IConfigProvider() = default;

    /**
     * @brief 从指定路径加载配置
     *
     * @param path 配置文件路径
     * @return 是否加载成功
     */
    virtual bool Load(const std::string& path) = 0;

    /**
     * @brief 获取当前配置快照
     *
     * @return 配置对象副本
     */
    virtual AppConfig GetSnapshot() const = 0;

    /**
     * @brief 订阅配置变化通知
     *
     * @param cb 变化回调
     */
    virtual void Subscribe(Callback cb) = 0;

    /**
     * @brief 主动轮询配置变化（用于热更新）
     */
    virtual void PollChanges() = 0;
};

/// <summary>
/// JSON 配置提供者骨架实现。
/// </summary>
class JsonConfigProvider final : public IConfigProvider {
public:
    /// <summary>加载 JSON 配置文件。</summary>
    bool Load(const std::string& path) override;

    /// <summary>读取配置快照。</summary>
    AppConfig GetSnapshot() const override;

    /// <summary>注册配置更新回调。</summary>
    void Subscribe(Callback cb) override;

    /// <summary>触发配置轮询与回调分发。</summary>
    void PollChanges() override;

private:
    /// <summary>保护内部配置状态的互斥量。</summary>
    mutable std::mutex mtx_;
    /// <summary>当前配置快照。</summary>
    AppConfig config_{};
    /// <summary>已注册回调列表。</summary>
    std::vector<Callback> callbacks_;
    /// <summary>配置文件路径缓存。</summary>
    std::string path_;
};

/// 配置提供者共享指针别名（也可以使用 `IConfigProvider::ptr`）。
using ConfigProviderPtr = std::shared_ptr<IConfigProvider>;

} // namespace streamer
