#pragma once

#include "core/Interfaces.h"

#include <atomic>
#include <thread>

namespace streamer {

/// <summary>
/// 模块作用：提供管线节点公共生命周期模板。
/// 用途：统一 Init/Start/Stop/Release 流程并封装工作线程管理。
/// </summary>
class PipelineNodeBase : public IPipelineNode {
public:
    /**
     * @brief 执行节点初始化并保存上下文
     *
     * @param ctx 应用上下文
     * @return 初始化是否成功
     */
    bool Init(AppContext& ctx) override;

    /**
     * @brief 启动节点工作线程
     *
     * @return 启动是否成功
     */
    bool Start() override;

    /**
     * @brief 停止节点并等待线程退出
     */
    void Stop() override;

    /**
     * @brief 释放节点持有资源
     */
    void Release() override;

protected:
    /// <summary>子类初始化钩子。</summary>
    virtual bool OnInit() = 0;
    /// <summary>子类停止钩子。</summary>
    virtual void OnStop() = 0;
    /// <summary>子类释放钩子。</summary>
    virtual void OnRelease() = 0;
    /// <summary>子类处理循环入口。</summary>
    virtual void ProcessLoop() = 0;

    /// <summary>节点运行状态标记。</summary>
    std::atomic<bool> running_{false};
    /// <summary>运行时上下文指针（不拥有）。</summary>
    AppContext* ctx_{nullptr};

private:
    /// <summary>节点工作线程。</summary>
    std::thread worker_;
};

} // namespace streamer
