#include "concurrency/ConcurrentQueue.h"
#include "config/Config.h"
#include "core/Clock.h"
#include "core/Context.h"
#include "core/ErrorPolicy.h"
#include "factory/ComponentFactory.h"
#include "infra/FfmpegContextManager.h"
#include "infra/Logger.h"
#include "pipeline/Nodes.h"
#include "pipeline/PipelineOrchestrator.h"

#include <chrono>
#include <memory>
#include <thread>

using namespace streamer;



/// <summary>
/// 模块作用：应用启动入口。
/// 用途：完成依赖组装、节点注册、管线生命周期控制。
/// </summary>
int main() {
    // 初始化日志模块。
    auto logger = std::make_shared<ConsoleLogger>(LogLevel::Debug);

    // 初始化配置模块并加载配置文件。
    auto config = std::make_shared<JsonConfigProvider>();
    config->Load("config.json");

    // 初始化 FFmpeg 全局上下文管理器。
    auto ffmpeg = std::make_shared<FFmpegContextManager>();
    ffmpeg->Init();

    // 组装应用上下文，供所有节点共享依赖。
    AppContext ctx{
        .logger = logger,
        .configProvider = config,
        .timestampProvider = std::make_shared<SteadyTimestampProvider>(),
        .clockSync = std::make_shared<PassthroughClockSync>(),
        .errorPolicy = std::make_shared<DefaultErrorPolicy>(),
        .reconnectPolicy = std::make_shared<ExponentialReconnectPolicy>(),
        .ffmpeg = ffmpeg};

    // 创建节点间通信队列。
    auto rawFrameQueue = std::make_shared<BlockingQueue<FramePtr>>();
    auto packetQueue = std::make_shared<BlockingQueue<PacketWrapperPtr>>();

    // 通过工厂创建具体组件并组装节点。
    auto screenNode = std::make_shared<ScreenCaptureNode>(ComponentFactory::CreateScreenCapture("dxgi"), rawFrameQueue);
    auto audioNode = std::make_shared<AudioCaptureNode>(ComponentFactory::CreateAudioCapture("dshow"), rawFrameQueue);
    auto encodeNode = std::make_shared<EncodeNode>(ComponentFactory::CreateEncoder("ffmpeg"), rawFrameQueue, packetQueue);
    auto streamNode =
        std::make_shared<StreamNode>(ComponentFactory::CreateMuxer("rtmp"), ComponentFactory::CreateStreamer("rtmp"), packetQueue);

    // 创建编排器并按依赖顺序注册节点。
    PipelineOrchestrator orchestrator(std::move(ctx));
    orchestrator.AddNode(screenNode);
    orchestrator.AddNode(audioNode);
    orchestrator.AddNode(encodeNode);
    orchestrator.AddNode(streamNode);

    // 初始化管线。
    if (!orchestrator.Init()) {
        return -1;
    }

    // 启动管线。
    if (!orchestrator.Start()) {
        return -2;
    }

    // 示例运行 3 秒。
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 停止并释放管线资源。
    orchestrator.Stop();
    orchestrator.Release();

    // 关闭 FFmpeg 全局资源。
    ffmpeg->Shutdown();
    return 0;
}
