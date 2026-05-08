#include "include/adapters/output/FfmpegMuxerStreamer.h"
#include "event/ThreadEventSDL.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

static bool init()
{
	const std::string filename = "./data_out/av/test_Muxer_Streamer.h264";
	static streamer::IStreamer::ptr str_ptr = streamer::LocalFileStreamer::createNew(filename);
	if (!str_ptr)
	{
		LOG_ERROR(g_logger) << "streamer::LocalFileStreamer::createNew() failed, LocalFile_Path= " << filename;
		return false;
	}

	return true;
}

int main()
{
	LOG_INFO(g_logger) << "hello, start";

	if (!init())
	{
		LOG_ERROR(g_logger) << "init() return false";
		return -1;
	}

	// 开启键鼠事件循环，模拟 QT 端操作
	streamer::SDL::GetInstance()->Start();

	return 0;
}