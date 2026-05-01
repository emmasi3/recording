#pragma once
#include "libav_h.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <memory>

using Microsoft::WRL::ComPtr;

namespace streamer
{
	class HwDevice
	{
	public:
		typedef std::shared_ptr<HwDevice> ptr;

		virtual ~HwDevice() {}

		// 接口：初始化硬件设备
		virtual int initDevice() = 0;

		// 接口：关闭硬件设备
		virtual bool Close() = 0;
	};

	/*
	* @brief 硬件上下文简单封装
	*/
	class HwDevice_d3d11 final : public HwDevice
	{
	public:
		typedef std::shared_ptr<HwDevice_d3d11> ptr;

		~HwDevice_d3d11();
		HwDevice_d3d11();
		/*
		* @brief 接口：初始化 D3D11 设备，填充 ComPtr<ID3D11Device> g_device;
		* @return 0 success
		* @return <0 error
		*/
		virtual int initDevice() override;

		virtual bool Close() override;

		AVBufferRef* get_hw_device_ctx() const { return m_hw_device_ctx; }
		ComPtr<ID3D11Device> get_device() const { return g_device; }
		ComPtr<ID3D11DeviceContext> get_devCtx() const { return g_devCtx; }
		int get_TEXTURE_BUFFER_SIZE() const;
		/*
		* @brief 仿照 BXC RTSP 项目中一些类的初始化，这样子做
		*	原因：原本想在 HwDevice_d3d11() { initDevice(); } 
		*	就是想要在构造的时候实现 设备的初始化，但是这样子做有问题
		*	（1）initDevice() 本身就是基类提供的 “接口”
		*		如果在构造哈数中直接调用，多态被破坏
		*	（2）可能有些成员还没有被初始化【可能性小】
		* @brief 这样子做，只需要在外部调用 HwDevice::ptr ptr = HwDevice_d3d11::createNew(); 即可
		*		只需要在 createNew() 内部实例化对象，并调用 initDevice() 即可，嗯嗯，没问题！
		*/
		static HwDevice::ptr createNew();

	private:
		// 硬件上下文
		AVBufferRef* m_hw_device_ctx;
		// D3D设备
		ComPtr<ID3D11Device> g_device;
		// D3D设备上下文
		ComPtr<ID3D11DeviceContext> g_devCtx;
		// 硬件帧池大小 -- 和 ffmpeg 对应
		int TEXTURE_BUFFER_SIZE;
	};
}
