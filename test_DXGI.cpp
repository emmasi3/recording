#define _CRT_SECURE_NO_WARNINGS
#include "include\adapters\codec\FfmpegEncoder.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));


int main(int argc, char* argv[])
{
	LOG_INFO(g_logger) << "hello";

	streamer::IEncoder::ptr ptr = streamer::VideoFfmpegEncoder::createNew(0, {1, 60});
	streamer::VideoFfmpegEncoder::ptr ptr_video = std::dynamic_pointer_cast<streamer::VideoFfmpegEncoder>(ptr);
	if (!ptr_video)
	{
		LOG_ERROR(g_logger) << "std::dynamic_pointer_cast<streamer::VideoFfmpegEncoder>(ptr) failed";
		return -1;
	}

	streamer::IScreenCapture::ptr dxgi = ptr_video->getDxgiCap();

	// 打开文件
	FILE* f = nullptr;
	f = fopen("./data_out/video/DXGI.h264", "wb");
	if (!f)
	{
		LOG_ERROR(g_logger) << "./new/data_out/video/DXGI.h264 open() error";
		return -1;
	}

	for (int i = 0; i < 1000; ++i)
	{
		streamer::FramePtr frame = dxgi->ReadFrame(i);
		// 判断是否达到帧间隔
		if (!dxgi->isPass(i) || !frame)
		{
			--i;
			continue;
		}
		
		// 记录写入帧数量
		ptr->Encode(frame, [&f](AVPacket* pkt) ->int {
			if (!pkt)
			{
				LOG_ERROR(g_logger) << "pkt is empty";
				return -1;
			}

			//LOG_INFO(g_logger) << "pkt->pts: " << pkt->pts << ", pkt->dts: " << pkt->dts;
			fwrite(pkt->data, 1, pkt->size, f);

			return 0;
		});


	}

	// 刷新编码器，输出剩余帧
	ptr->Flush([&f](AVPacket* pkt) ->int {
		if (!pkt)
		{
			LOG_ERROR(g_logger) << "pkt is empty";
			return -1;
		}

		//LOG_INFO(g_logger) << "pkt->pts: " << pkt->pts << ", pkt->dts: " << pkt->dts;
		fwrite(pkt->data, 1, pkt->size, f);

		return 0;
	});

	// 关闭文件
	if (f)
		fclose(f);
	
	return 0;
}