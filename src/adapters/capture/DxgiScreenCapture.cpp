#include "adapters/capture/DxgiScreenCapture.h"
#include "core/Clock.h"

namespace streamer {
	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

	DxgiScreenCapture::DxgiScreenCapture(int fps)
		:IScreenCapture(fps),
		hw_device(nullptr),
		g_device(nullptr),
		g_devCtx(nullptr),
		m_video_queue(nullptr),
		m_curentIndex(0)
	{
		// 创建硬件组件
		hw_device = HwDevice_d3d11::createNew();
		if (hw_device)
		{
			g_device = std::dynamic_pointer_cast<HwDevice_d3d11>(hw_device)->get_device();
			g_devCtx = std::dynamic_pointer_cast<HwDevice_d3d11>(hw_device)->get_devCtx();
		}

		// 初始化组件 -- m_video_queue
		m_video_queue = std::make_shared<BlockingQueue<FramePtr>>();
	}

	IScreenCapture::ptr DxgiScreenCapture::createNew(int fps)
	{
		IScreenCapture::ptr ptr = std::make_shared<DxgiScreenCapture>(fps);
		// 初始化 DXGI 组件
		if (!ptr->Open())
		{
			LOG_ERROR(g_logger) << "DxgiScreenCapture::Open() return false!";
			return nullptr;
		}

		return ptr;
	}

	DxgiScreenCapture::~DxgiScreenCapture()
	{
		Close();
	}

	int DxgiScreenCapture::GetQueueSize() const
	{
		return m_video_queue->GetQueueSize();
	}

	void DxgiScreenCapture::Close()
	{
		// 释放硬件帧
		for (auto& i : hwFramePool)
		{
			if (i)
			{
				av_frame_free(&i);
			}
		}
		hwFramePool.clear();
		m_curentIndex = 0;
	}

	bool DxgiScreenCapture::isPass(int i, bool timeout)
	{
		// 获取实际两帧间隔 ms
		int64_t delta_time = QPC::GetInstance()->NowMs(false);
		// LOG_INFO(g_logger) << "delta_time: " << delta_time;
		// 判断是否是 第 1 帧，因为第一帧需要无脑送进去，并且因为主循环中，i != 0 的情况居多，
		// 为了防止分支预测错误的损耗叠加，将最可能的情况放到前面
		if (i != 0)
		{
			// 间隔 >= (1000 / m_fps) 时，可以送入
			// std::cout << sylar::delta_time << std::endl;
			if (delta_time >= (1000 / m_fps))
			{
				// 不是在超时逻辑中调用该函数，就更新时间帧
				if (!timeout)
				{
					QPC::GetInstance()->NowMs();
				}
				return true;
			}
			// 否则，不送入，继续循环
			return false;
		}
		else if (i == 0)
		{
			QPC::GetInstance()->NowMs();
			return true;
		}

	}


    bool DxgiScreenCapture::Open() 
	{
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		// 优先复用 FFmpeg 的 D3D11 设备，避免跨设备资源导致 E_INVALIDARG
		if (!g_device || !g_devCtx)
		{
			// 兜底：若外部没提供，才自行创建
			D3D_DRIVER_TYPE DriverTypes[] = { D3D_DRIVER_TYPE_HARDWARE,
				D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE, };
			UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

			D3D_FEATURE_LEVEL FeatureLevels[] = {
				D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_9_1 };
			UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

			D3D_FEATURE_LEVEL FeatureLevel;
			HRESULT hr = E_FAIL;
			for (UINT index = 0; index < NumDriverTypes; index++) {
				hr = D3D11CreateDevice(
					nullptr, DriverTypes[index], nullptr, 0,
					FeatureLevels, NumFeatureLevels, D3D11_SDK_VERSION,
					&g_device, &FeatureLevel, &g_devCtx);
				if (SUCCEEDED(hr)) break;
			}
			if (FAILED(hr)) return false;
		}

		HRESULT hr = S_OK;

		// 2、获取DXGITest设备
		IDXGIDevice* _pDXGITestDev = nullptr;
		hr = g_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&_pDXGITestDev));
		if (FAILED(hr))
		{
			return false;
		}

		// 3、获取DXGITest适配器
		IDXGIAdapter* _pDXGITestAdapter = nullptr;
		hr = _pDXGITestDev->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&_pDXGITestAdapter));
		if (FAILED(hr))
		{
			return false;
		}

		// 4、获取输出
		UINT i = 0;
		IDXGIOutput* _pDXGIOutput = nullptr;
		hr = _pDXGITestAdapter->EnumOutputs(i, &_pDXGIOutput);
		if (FAILED(hr))
		{
			return false;
		}

		// 获取输出描述结构
		DXGI_OUTPUT_DESC DesktopDesc;
		_pDXGIOutput->GetDesc(&DesktopDesc);

		// 5、请求接口给Output1
		IDXGIOutput1* _pDXGIOutput1 = nullptr;
		hr = _pDXGIOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&_pDXGIOutput1));
		if (FAILED(hr))
		{
			return false;
		}

		// 6、创建桌面副本
		hr = _pDXGIOutput1->DuplicateOutput(g_device.Get(), &g_duplication);
		if (FAILED(hr))
		{
			return false;
		}

		// 释放资源，其实可以用 ComPtr 来接管他们，但是考虑到这只是局部使用，额，就不用了
		if (_pDXGIOutput1) _pDXGIOutput1->Release();
		if (_pDXGIOutput) _pDXGIOutput->Release();
		if (_pDXGITestAdapter) _pDXGITestAdapter->Release();
		if (_pDXGITestDev) _pDXGITestDev->Release();

		// 7、开启视频生产线程，这里启动不行，硬件帧池还没初始化呢，改变位置：line 72 FfmpegEncoder.cpp
		//if (!send_VideoCaptureThread_to_SDL_threads(true))
		//{
		//	LOG_ERROR(g_logger) << "send_VideoCaptureThread_to_SDL_threads(true) failed";
		//	return false;
		//}

		return true;
	}

    FramePtr DxgiScreenCapture::ReadFrame(int i) 
	{
		// 构建需要返回的帧封装
		AVFrame* frame = nullptr;
		// 这里一般采用 fasle; 因为对于 DXGI 硬件帧池来说，提供给外部的 frame 实际指向的是池子(vector)中的数据，所以没必要这样
		// 最后统一释放即可
		// FramePtr ptr = std::make_shared<RawFrame>(FrameMeta{MediaType::Video}, std::make_shared<FrameWrapper>(frame, false));
		// 你还是 shit 吃得少了，把 frame 这个 nullptr 传进去？然后类中的 AVFrame* frm_ = frame; ？
		// 看出问题了吗？你将一个 nullptr 资源给到 frm_，这么写没有意义，你的本意，将 frame 给到封装器管理，管理指针，需要它的 二级指针
		// 所以你需要管理的是他的资源，也就是 AVFrame* 指向的东西，但是他现在还没有东西，so？该方法返回时再构造，嗯嗯

		HRESULT hr;
		DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
		ComPtr<IDXGIResource> desktopResource;
		hr = g_duplication->AcquireNextFrame(0,
			&frameInfo,
			&desktopResource);

		// 超时
		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			//std::cout << "超时: " << timeout_counts++ << std::endl;
			if (desktopResource)
			{
				desktopResource->Release();
				desktopResource = nullptr;
			}

			// 但是达到帧间隔，并且 hwFramePool 并不是空的（尚未初始化导致崩溃）
			if (isPass(i, true) && !hwFramePool.empty())
			{
				// 计算 sylar::currentIndex 的上一个索引
				size_t prevIndex = (m_curentIndex + hwFramePool.size() - 1) % hwFramePool.size();
				// 将上一帧给出
				frame = hwFramePool[prevIndex];
				if (frame) {
					// 接管
					FramePtr ptr = std::make_shared<RawFrame>(FrameMeta{ MediaType::Video }, std::make_shared<FrameWrapper>(frame, false));
					return ptr;
				}
			}

			return nullptr;
		}
		// 错误，返回-1
		if (FAILED(hr))
		{
			LOG_ERROR(g_logger) << "AcquireNextFrame() return FAILED(hr), hr: " << hr;
			return nullptr;
		}
		// 成功拿到新帧
		ComPtr<ID3D11Texture2D> dxgiTex;
		desktopResource.As(&dxgiTex);

		//if (hwFramePool.empty()) {
		//	LOG_ERROR(g_logger) << "hwFramePool is empty!";
		//	g_duplication->ReleaseFrame();
		//	return nullptr;
		//}

		// 转换纹理格式
		if (!ConvertDesktopBGRA_To_AVFrameNV12(dxgiTex.Get(), hwFramePool[m_curentIndex]))
		{
			LOG_ERROR(g_logger) << "BGRA->NV12 convert failed";
			g_duplication->ReleaseFrame();
			return nullptr;
		}

		// 5、返回 outFrame;
		frame = hwFramePool[m_curentIndex];
		// 6、更新索引（环形）
		m_curentIndex = (m_curentIndex + 1) % hwFramePool.size();
		// 调用完 AcquireNextFrame 必须调用 ReleaseFrame()
		g_duplication->ReleaseFrame();

		// 接管AVFrame资源
		FramePtr ptr = std::make_shared<RawFrame>(FrameMeta{ MediaType::Video }, std::make_shared<FrameWrapper>(frame, false));

		return ptr;
    }

	FramePtr DxgiScreenCapture::ReadFrame()
	{
		return m_video_queue->WaitAndPop();
	}

	int DxgiScreenCapture::init_hwFramePool(AVCodecContext* ctx, int TEXTURE_BUFFER_SIZE)
	{
		int ret = 0;
		hwFramePool.reserve(TEXTURE_BUFFER_SIZE);

		for (int i = 0; i < TEXTURE_BUFFER_SIZE; ++i)
		{
			AVFrame* hwframe = av_frame_alloc();
			ret = av_hwframe_get_buffer(ctx->hw_frames_ctx, hwframe, 0);
			if (ret < 0)
			{
				LOG_ERROR(g_logger) << "av_hwframe_get_buffer() failed!";
				return -1;
			}
			// 放入数组
			hwFramePool.push_back(hwframe);
		}

		return 0;
	}

	bool DxgiScreenCapture::GetD3D11TextureFromAVFrame(AVFrame* frame,
		ID3D11Texture2D** outTex,
		UINT* outSubresource)
	{
		if (!frame || !outTex || !outSubresource) return false;

		// AV_PIX_FMT_D3D11 下，data[0]通常是 ID3D11Texture2D*
		// data[1]通常存放子资源索引（通过 intptr_t 传递）
		auto* tex = reinterpret_cast<ID3D11Texture2D*>(frame->data[0]);
		if (!tex) return false;

		UINT sub = static_cast<UINT>(reinterpret_cast<uintptr_t>(frame->data[1]));

		*outTex = tex;
		(*outTex)->AddRef(); // 调用者释放
		*outSubresource = sub;
		return true;
	}

	bool DxgiScreenCapture::InitVP(BgraToNv12VP& s, UINT width, UINT height)
	{
		if (!g_device || !g_devCtx) return false;

		if (s.vp && s.width == width && s.height == height) {
			return true; // 已初始化且尺寸匹配
		}

		s = {}; // reset

		HRESULT hr = g_device->QueryInterface(__uuidof(ID3D11VideoDevice),
			reinterpret_cast<void**>(s.videoDev.GetAddressOf()));
		if (FAILED(hr) || !s.videoDev) {
			LOG_ERROR(g_logger) << "QueryInterface(ID3D11VideoDevice) failed";
			return false;
		}

		hr = g_devCtx->QueryInterface(__uuidof(ID3D11VideoContext),
			reinterpret_cast<void**>(s.videoCtx.GetAddressOf()));
		if (FAILED(hr) || !s.videoCtx) {
			LOG_ERROR(g_logger) << "QueryInterface(ID3D11VideoContext) failed";
			return false;
		}

		D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
		contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		contentDesc.InputFrameRate.Numerator = 60;
		contentDesc.InputFrameRate.Denominator = 1;
		contentDesc.OutputFrameRate.Numerator = 60;
		contentDesc.OutputFrameRate.Denominator = 1;
		contentDesc.InputWidth = width;
		contentDesc.InputHeight = height;
		contentDesc.OutputWidth = width;
		contentDesc.OutputHeight = height;
		contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		hr = s.videoDev->CreateVideoProcessorEnumerator(&contentDesc, &s.vpEnum);
		if (FAILED(hr)) {
			LOG_ERROR(g_logger) << "CreateVideoProcessorEnumerator failed";
			return false;
		}

		hr = s.videoDev->CreateVideoProcessor(s.vpEnum.Get(), 0, &s.vp);
		if (FAILED(hr)) {
			LOG_ERROR(g_logger) << "CreateVideoProcessor failed";
			return false;
		}

		s.width = width;
		s.height = height;
		return true;
	}


	bool DxgiScreenCapture::ConvertDesktopBGRA_To_AVFrameNV12(
		ID3D11Texture2D* srcBGRA,   // 来自 AcquireNextFrame 的桌面纹理
		AVFrame* hwFrame            // av_hwframe_get_buffer 分配的 D3D11 frame
	)
	{
		if (!srcBGRA || !hwFrame) {
			LOG_ERROR(g_logger) << "ConvertDesktopBGRA_To_AVFrameNV12: null input";
			return false;
		}

		// 源描述
		D3D11_TEXTURE2D_DESC srcDesc = {};
		srcBGRA->GetDesc(&srcDesc);

		// 目标纹理 + 目标子资源索引（来自 AVFrame->data[1]）
		ComPtr<ID3D11Texture2D> dstTex;
		UINT dstSubresource = 0;
		if (!GetD3D11TextureFromAVFrame(hwFrame, &dstTex, &dstSubresource)) {
			LOG_ERROR(g_logger) << "GetD3D11TextureFromAVFrame failed";
			return false;
		}

		D3D11_TEXTURE2D_DESC dstDesc = {};
		dstTex->GetDesc(&dstDesc);

		// 基本校验
		if (dstDesc.Format != DXGI_FORMAT_NV12) {
			LOG_ERROR(g_logger) << "Destination frame texture is not NV12";
			return false;
		}
		if (dstDesc.Width != srcDesc.Width || dstDesc.Height != srcDesc.Height) {
			LOG_ERROR(g_logger) << "Size mismatch src(" << srcDesc.Width << "x" << srcDesc.Height
				<< ") dst(" << dstDesc.Width << "x" << dstDesc.Height << ")";
			return false;
		}
		if (srcDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
			srcDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
			LOG_ERROR(g_logger) << "Unexpected source format: " << srcDesc.Format;
			return false;
		}

		static BgraToNv12VP s_vp;
		if (!InitVP(s_vp, srcDesc.Width, srcDesc.Height)) {
			LOG_ERROR(g_logger) << "InitVP failed";
			return false;
		}

		HRESULT hr = S_OK;

		// 输入视图（桌面 BGRA）
		D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inViewDesc = {};
		inViewDesc.FourCC = 0;
		inViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
		inViewDesc.Texture2D.MipSlice = 0;
		inViewDesc.Texture2D.ArraySlice = 0; // 桌面复制通常是单 slice

		ComPtr<ID3D11VideoProcessorInputView> inView;
		hr = s_vp.videoDev->CreateVideoProcessorInputView(
			srcBGRA, s_vp.vpEnum.Get(), &inViewDesc, &inView);
		if (FAILED(hr)) {
			LOG_ERROR(g_logger) << "CreateVideoProcessorInputView failed, hr=0x" << std::hex << hr << std::dec;
			return false;
		}

		// 输出视图（关键：使用 AVFrame 指定的 subresource/slice）
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outViewDesc = {};

		// 当前工程里 MipLevels 基本为 1，所以 subresource 可以直接视作 arraySlice
		UINT dstArraySlice = dstSubresource;
		if (dstArraySlice >= dstDesc.ArraySize) {
			LOG_ERROR(g_logger) << "Invalid dstSubresource/slice: " << dstSubresource
				<< ", ArraySize=" << dstDesc.ArraySize;
			return false;
		}

		if (dstDesc.ArraySize > 1) {
			outViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
			outViewDesc.Texture2DArray.MipSlice = 0;
			outViewDesc.Texture2DArray.FirstArraySlice = dstArraySlice;
			outViewDesc.Texture2DArray.ArraySize = 1;
		}
		else {
			outViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
			outViewDesc.Texture2D.MipSlice = 0;
		}

		ComPtr<ID3D11VideoProcessorOutputView> outView;
		hr = s_vp.videoDev->CreateVideoProcessorOutputView(
			dstTex.Get(), s_vp.vpEnum.Get(), &outViewDesc, &outView);
		if (FAILED(hr)) {
			LOG_ERROR(g_logger) << "CreateVideoProcessorOutputView failed, hr=0x" << std::hex << hr << std::dec;
			return false;
		}

		RECT srcRect = { 0, 0, (LONG)srcDesc.Width, (LONG)srcDesc.Height };
		RECT dstRect = { 0, 0, (LONG)dstDesc.Width, (LONG)dstDesc.Height };
		s_vp.videoCtx->VideoProcessorSetStreamSourceRect(s_vp.vp.Get(), 0, TRUE, &srcRect);
		s_vp.videoCtx->VideoProcessorSetStreamDestRect(s_vp.vp.Get(), 0, TRUE, &dstRect);

		// 颜色空间（可按需调整）
		D3D11_VIDEO_PROCESSOR_COLOR_SPACE inCS = {};
		inCS.Usage = 0;
		inCS.RGB_Range = 1; // full RGB
		inCS.YCbCr_Matrix = 1; // BT.709
		inCS.YCbCr_xvYCC = 0;
		inCS.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

		D3D11_VIDEO_PROCESSOR_COLOR_SPACE outCS = {};
		outCS.Usage = 0;
		outCS.RGB_Range = 0;
		outCS.YCbCr_Matrix = 1; // BT.709
		outCS.YCbCr_xvYCC = 0;
		outCS.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;

		s_vp.videoCtx->VideoProcessorSetStreamColorSpace(s_vp.vp.Get(), 0, &inCS);
		s_vp.videoCtx->VideoProcessorSetOutputColorSpace(s_vp.vp.Get(), &outCS);

		D3D11_VIDEO_PROCESSOR_STREAM stream = {};
		stream.Enable = TRUE;
		stream.OutputIndex = 0;
		stream.InputFrameOrField = 0;
		stream.PastFrames = 0;
		stream.FutureFrames = 0;
		stream.pInputSurface = inView.Get();

		hr = s_vp.videoCtx->VideoProcessorBlt(
			s_vp.vp.Get(),
			outView.Get(),
			0,
			1,
			&stream);

		if (FAILED(hr)) {
			LOG_ERROR(g_logger) << "VideoProcessorBlt failed, hr=0x" << std::hex << hr << std::dec;
			return false;
		}

		return true;
	}

	void DxgiScreenCapture::VideoCaptureThread()
	{
		LOG_INFO(g_logger) << "VideoCaptureThread started.";

		int frame_idx = 0;
		// 采集循环
		while (SDL::GetInstance()->get_state() != STATE::Term)
		{
			// 以阻塞/超时的形式读取下一帧
			FramePtr frame = ReadFrame(frame_idx);

			if (frame)
			{
				// 检查这帧是否满足我们规定的 fps (dxgiCap 内的帧率控制机制）
				if (!isPass(frame_idx))
				{
					continue; // 无效或不到时间，抛弃或重试
				}

				// 直接将提取出来的硬件帧扔到并发缓冲区
				// 【重要】此处直接 Push。
				m_video_queue->Push(frame);

				++frame_idx;
			}
			else
			{
				// 获取失败或超时，可以短暂 yield 放出CPU，但 dxgi AcquireNextFrame 本身带阻塞
				std::this_thread::yield();
			}
		}

		// 最后封闭队列，通知消费者退出
		m_video_queue->Close();
		LOG_INFO(g_logger) << "VideoCaptureThread finished.";
	}

	bool DxgiScreenCapture::send_VideoCaptureThread_to_SDL_threads(bool Immediately)
	{
		int threads_counts = -1;
		if (!Immediately)
		{
			threads_counts = SDL::GetInstance()->get_threadfuncs_counts();
			// 注册线程函数(并不立即开启) -- 开始采集音频并送入队列
			SDL::GetInstance()->push_thread_to_vector(std::bind(&DxgiScreenCapture::VideoCaptureThread, this));
			if (threads_counts == SDL::GetInstance()->get_threadfuncs_counts())
			{
				LOG_ERROR(g_logger) << "SDL::GetInstance()->push_thread_to_vector(std::bind(DshowAudioCapture::ReadFrameFrom_device, this)); failed";
				return false;
			}
		}
		else
		{
			// 立即开启该线程，并送入线程数组
			threads_counts = SDL::GetInstance()->get_threads_counts();
			SDL::GetInstance()->push_threadfunc_to_threads(std::bind(&DxgiScreenCapture::VideoCaptureThread, this));
			if (threads_counts == SDL::GetInstance()->get_threads_counts())
			{
				LOG_ERROR(g_logger) << "SDL::GetInstance()->push_threadfunc_to_threads(std::bind(DshowAudioCapture::ReadFrameFrom_device, this)); failed";
				return false;
			}
		}

		return true;
	}




} // namespace streamer
