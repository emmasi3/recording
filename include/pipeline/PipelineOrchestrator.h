#pragma once

#include "core/Context.h"
#include "core/Interfaces.h"

#include <memory>
#include <vector>

namespace streamer {

/// <summary>
/// 模块作用：统一调度管线节点生命周期。
/// 用途：管理节点初始化、启动、停止、释放顺序。
/// </summary>
class PipelineOrchestrator {
public:
    /// <summary>
    /// 构造编排器。
    /// </summary>
    /// <param name="ctx">应用上下文副本。</param>
    explicit PipelineOrchestrator(AppContext ctx);

    /// <summary>
    /// 注册一个管线节点。
    /// </summary>
    /// <param name="node">节点实例。</param>
    void AddNode(std::shared_ptr<IPipelineNode> node);

    /// <summary>
    /// 依次初始化所有节点。
    /// </summary>
    /// <returns>全部成功返回 true。</returns>
    bool Init();

    /// <summary>
    /// 依次启动所有节点。
    /// </summary>
    /// <returns>全部成功返回 true。</returns>
    bool Start();

    /// <summary>
    /// 逆序停止所有节点。
    /// </summary>
    void Stop();

    /// <summary>
    /// 逆序释放所有节点资源。
    /// </summary>
    void Release();

private:
    /// <summary>全局运行上下文。</summary>
    AppContext ctx_;
    /// <summary>已注册管线节点列表。</summary>
    std::vector<std::shared_ptr<IPipelineNode>> nodes_;
};

} // namespace streamer
