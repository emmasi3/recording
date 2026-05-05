#include "include/adapters/output/RtmpMuxerStreamer.h"
#include "event/ThreadEventSDL.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

static bool init()
{
	const std::string rtmp_url = "rtmp://127.0.0.1:1935/live/test001";

	static streamer::IStreamer::ptr streamer_ptr = streamer::RtmpStreamer::createNew(rtmp_url);
	if (!streamer_ptr)
	{
		return false;
	}

	return true;
}

int main()
{
	LOG_DEBUG(g_logger) << "hello, RTMP";

	if (!init())
	{
		LOG_ERROR(g_logger) << "init() return false";
		return -1;
	}

	streamer::SDL::GetInstance()->Start();

	return 0;
}