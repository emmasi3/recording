#define _CRT_SECURE_NO_WARNINGS
#include "include/adapters/codec/FfmpegEncoder.h"
#include "include/concurrency/ConcurrentQueue.h" // 引用你的并发队列
#include "include/core/FrameWrapper.h"

// STL and basic IO
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

using namespace streamer;

static ILogger::ptr g_logger = ILogger::ptr(new ConsoleLogger(LogLevel::Debug));

// 全局终止标志
std::atomic<bool> is_running(true);

// 将捕获线程与编码/写文件线程分开
void VideoCaptureThread(
    IScreenCapture::ptr dxgi, 
    std::shared_ptr<IConcurrentQueue<FramePtr>> video_queue)
{
    LOG_INFO(g_logger) << "VideoCaptureThread started.";

    int frame_idx = 0;
    // 采集循环
    while (is_running)
    {
        // 以阻塞/超时的形式读取下一帧
        FramePtr frame = dxgi->ReadFrame(frame_idx);

        if (frame)
        {
            // 检查这帧是否满足我们规定的 fps（你的 dxgiCap 内的帧率控制机制）
            if (!dxgi->isPass(frame_idx))
            {
                continue; // 无效或不到时间，抛弃或重试
            }

            // 直接将提取出来的硬件帧扔到并发缓冲区
            // 【重要】此处直接 Push。你可以配置一个BlockingQueue给它。
            video_queue->Push(frame);

            ++frame_idx;
        }
        else
        {
            // 获取失败或超时，可以短暂 yield 放出CPU，但 dxgi AcquireNextFrame 本身带阻塞
            std::this_thread::yield();
        }
    }

    // 最后封闭队列，通知消费者退出
    video_queue->Close();
    LOG_INFO(g_logger) << "VideoCaptureThread finished.";
}

int main(int argc, char* argv[])
{
    LOG_INFO(g_logger) << "hello, Start Testing DXGI + Queue";

    // 1. 初始化编码器，获取捕捉组件 DXGI (你的模式是强绑定的)
    IEncoder::ptr ptr = VideoFfmpegEncoder::createNew(0, { 1, 60 });
    VideoFfmpegEncoder::ptr ptr_video = std::dynamic_pointer_cast<VideoFfmpegEncoder>(ptr);
    if (!ptr_video)
    {
        LOG_ERROR(g_logger) << "VideoFfmpegEncoder init failed";
        return -1;
    }

    IScreenCapture::ptr dxgi = ptr_video->getDxgiCap();

    // 2. 创建用于接收 FramePtr 的阻塞并发队列
    // (假定你的 BlockingQueue<T> 能够直接被实例化)
    IConcurrentQueue<FramePtr>::ptr video_queue = std::make_shared<BlockingQueue<FramePtr>>();

    // 3. 打开写入的文件
    FILE* f = fopen("./data_out/video/DXGI_Queue.h264", "wb");
    if (!f)
    {
        LOG_ERROR(g_logger) << "file open failed";
        return -1;
    }

    // 4. 开启独立采集线程
    std::thread capture_thread(VideoCaptureThread, dxgi, video_queue);

    // 5. 在主线程中作为“消费者”：不断的从队列中 Pop 并且 Encode 写文件
    int encode_count = 0;
    while (is_running)
    {
        // 阻塞等待读取队列
        FramePtr frame = video_queue->WaitAndPop();

        // 如果队列关闭或者抛空返回 null（你的 WaitAndPop 需要能响应 Close 返回空，或者用 TryPop）
        if (!frame)
        {
            // 如果是正常关闭的话跳出
            if(!is_running)
                break;
            continue;
        }

        // 拿到数据后，在当前线程编码
        ptr_video->Encode(frame, [&f](AVPacket* pkt) -> int {
            if (!pkt) return -1;
            fwrite(pkt->data, 1, pkt->size, f);
            return 0;
        });

        // 为测试添加退出条件，录制 1000 帧后停止
        encode_count++;
        if (encode_count >= 1000)
        {
            is_running = false;
        }
    }

    // 6. 等待采集线程结束
    if (capture_thread.joinable())
    {
        capture_thread.join();
    }

    // 7. Flush 编码器内部剩余数据
    ptr_video->Flush([&f](AVPacket* pkt) -> int {
        if (pkt)
        {
            fwrite(pkt->data, 1, pkt->size, f);
            return 0;
        }
        return -1;
    });

    fclose(f);
    LOG_INFO(g_logger) << "Test DXGI with Queue finished! Total frames encoded: " << encode_count;

    return 0;
}
