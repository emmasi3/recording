#pragma once

#include "core/Interfaces.h"
#include "Hwdevice_d3d11.h"
#include "infra/Logger.h"

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
    * @brief 读取一帧屏幕图像
    * @param i 帧索引，由 FfmpegEncoder 提供
    * @return 视频帧对象；无数据时可返回空
    */
    FramePtr ReadFrame(int i = 1) override;

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
};

} // namespace streamer
