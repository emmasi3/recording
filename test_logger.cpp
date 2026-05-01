#include "infra/Logger.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

int main(int argc, char* argv[])
{
	LOG_INFO(g_logger) << "hello";
	// 创建 日志器时设置其 日志级别

	return 0;
}