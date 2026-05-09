#pragma once
#include "core/Interfaces.h"
#include "Hwdevice_d3d11.h"
#include "infra/Logger.h"
#include "concurrency/ConcurrentQueue.h"

#include <vector>

namespace streamer {

/// <summary>
/// 模块作用：DXGI 桌面采集适配器骨架。
/// 用途：实现屏幕捕获接口并向管线输出原始视频帧。
/// </summary>
class DxgiScreenCapture final : public IScreenCapture 
{
public:
    typedef std::shared_ptr<DxgiScreenCapture> ptr;
    ~DxgiScreenCapture();

    DxgiScreenCapture(int fps = 25);

    static IScreenCapture::ptr createNew(int fps = 25);

    /**
    * @brief 打开 DXGI 采集资源，初始化 DXGI 组件，包括：
    *			g_device;
    *			g_devCtx;
    *			g_duplication;
    * @return 是否打开成功
    */
    bool Open() override;

    /**
    * @brief 读取一帧屏幕图像，应该放入生产者线程调用，不应直接调用
    * @param i 帧索引，由 FfmpegEncoder 提供
    * @return 视频帧对象；无数据时可返回空
    */
    FramePtr ReadFrame(int i) override;

    /*
    * @brief 从视频队列中读取一帧，阻塞读取
    * @return FramePtr 视频帧封装
    */
    FramePtr ReadFrame() override;

    /*
    * @brief 获取视频队列大小
    * @return m_video_queue size
    */
    int GetQueueSize() const;

    /**
    * @brief 关闭 DXGI 采集资源
    */
    void Close() override;

    HwDevice::ptr get_hw_device() const { return hw_device; }

    virtual bool isPass(int i, bool timeout = false) override;

    /*
    * @brief 初始化硬件帧池 hwFramePool -- 该方法在 FfmpegEncoder::createNew() 中被执行，涉及到 DXGI 的帧初始化，方法执行顺序不能乱，
    *   最好按照 FfmpegEncoder::createNew() 进行维护
    * @param ctx 由 FfmpegEncoder::createNew() 中提供
    * @param TEXTURE_BUFFER_SIZE 由 FfmpegEncoder::createNew() 中提供
    */
    int init_hwFramePool(AVCodecContext* ctx, int TEXTURE_BUFFER_SIZE);

    /*
    * @brief BGRA 纹理转换到 AVFrame 对应 NV12 纹理
    * @param srcBGRA 来自 AcquireNextFrame 的桌面纹理
    * @param hwFrame av_hwframe_get_buffer 分配的 D3D11 frame
    */
    bool ConvertDesktopBGRA_To_AVFrameNV12(
        ID3D11Texture2D* srcBGRA,
        AVFrame* hwFrame
    );

    /*
    * @brief 私有方法，将生产者线程送入线程队列(注册或是)
    * @param bool Immediately 是否立即开启线程，默认为 false，先注册，后续统一开启
    * @return 送入是否成功
    */
    bool send_VideoCaptureThread_to_SDL_threads(bool Immediately = false);

private:
    /*
    * @brief 创建(一次性) VideoProcessor 资源
    */
    struct BgraToNv12VP
    {
        ComPtr<ID3D11VideoDevice>           videoDev;
        ComPtr<ID3D11VideoContext>          videoCtx;
        ComPtr<ID3D11VideoProcessorEnumerator> vpEnum;
        ComPtr<ID3D11VideoProcessor>        vp;
        UINT                                width = 0;
        UINT                                height = 0;
    };

    // 转换纹理格式所需要的方法
    bool GetD3D11TextureFromAVFrame(AVFrame* frame,
        ID3D11Texture2D** outTex,
        UINT* outSubresource);

    // 转换纹理格式所需要的方法
    bool InitVP(BgraToNv12VP& s, UINT width, UINT height);

    /*
    * @brief video生产者线程，获取帧数据并送入阻塞队列
    */
    void VideoCaptureThread();


private:
    HwDevice::ptr hw_device;
    // D3D设备
    ComPtr<ID3D11Device> g_device;
    // D3D设备上下文
    ComPtr<ID3D11DeviceContext> g_devCtx;
    // 桌面复制
    ComPtr<IDXGIOutputDuplication> g_duplication;
    // 存放 AVFrame* 的数组，内部元素均由 av_hwframe_get_buffer() 提供
    std::vector<AVFrame*> hwFramePool;
    // 硬件帧池索引
    int m_curentIndex;
    // 视频帧队列(阻塞)
    IConcurrentQueue<FramePtr>::ptr m_video_queue;
};

} // namespace streamer
