#include "Hwdevice_d3d11.h"
#include "infra/Logger.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

namespace streamer
{
	HwDevice_d3d11::HwDevice_d3d11()
		:m_hw_device_ctx(nullptr),
		TEXTURE_BUFFER_SIZE(600)
	{

	}

	HwDevice_d3d11::~HwDevice_d3d11()
	{
		if (!Close())
		{
			LOG_ERROR(g_logger) << "HwDevice_d3d11::Close() failed";
		}
	}

	int HwDevice_d3d11::initDevice()
	{
		std::string err_buf;
		err_buf.resize(AV_ERROR_MAX_STRING_SIZE);

		AVBufferRef* hw_device_ctx = nullptr;
		int ret = av_hwdevice_ctx_create(
			&hw_device_ctx,
			AV_HWDEVICE_TYPE_D3D11VA,
			NULL,
			NULL,
			0);
		if (ret < 0)
		{
			LOG_ERROR(g_logger) << "av_hwdevice_ctx_create() return: " << ret << " -- "
				<< AVStrError::strerror(ret, err_buf.data(), err_buf.size());
			return ret;
		}

		// 关键：拿到 FFmpeg 内部的 D3D11 设备，后续 DXGI/VP 必须复用它
		AVHWDeviceContext* hwctx = reinterpret_cast<AVHWDeviceContext*>(hw_device_ctx->data);
		AVD3D11VADeviceContext* d3d11hw =
			reinterpret_cast<AVD3D11VADeviceContext*>(hwctx->hwctx);

		if (!d3d11hw || !d3d11hw->device)
		{
			av_buffer_unref(&hw_device_ctx);
			return -1;
		}

		g_device = d3d11hw->device; // ComPtr 会 AddRef

		if (d3d11hw->device_context)
		{
			g_devCtx = d3d11hw->device_context; // ComPtr 会 AddRef
		}
		else
		{
			ID3D11DeviceContext* ctx = nullptr;
			g_device->GetImmediateContext(&ctx);
			g_devCtx = ctx;
			if (ctx)
				ctx->Release();
		}

		this->m_hw_device_ctx = hw_device_ctx;

		return 0;
	}

	bool HwDevice_d3d11::Close()
	{
		if(m_hw_device_ctx)
			av_buffer_unref(&m_hw_device_ctx);
		return true;
	}

	int HwDevice_d3d11::get_TEXTURE_BUFFER_SIZE() const
	{
		return TEXTURE_BUFFER_SIZE;
	}

	HwDevice::ptr HwDevice_d3d11::createNew()
	{
		HwDevice::ptr ptr = std::make_shared<HwDevice_d3d11>();
		int ret = ptr->initDevice();
		if (ret < 0)
		{
			return nullptr;
		}

		return ptr;
	}

}
