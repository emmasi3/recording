#include "core/MediaTypes.h"
#include "include\infra\Logger.h"
#include "include\core\Clock.h"

#include <chrono>
#include <thread>

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

int main()
{
	LOG_INFO(g_logger) << "hello, world";

	LOG_INFO(g_logger) << "the Frequency is: " << streamer::QPC::GetInstance()->getFrequency();

	// 睡眠 1 秒，看看精度如何
	std::this_thread::sleep_for(std::chrono::seconds(1));
	LOG_INFO(g_logger) << "sleep_for(1 s), QPC: " << streamer::QPC::GetInstance()->NowMs() / 1000;
	
	// 睡眠 100 ms，看看精度如何
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	LOG_INFO(g_logger) << "sleep_for(100 ms), QPC: " << streamer::QPC::GetInstance()->NowMs();

	// 睡眠 16 ms，看看精度如何
	std::this_thread::sleep_for(std::chrono::milliseconds(16));
	LOG_INFO(g_logger) << "sleep_for(16 ms), QPC: " << streamer::QPC::GetInstance()->NowMs();


	/*
	filename:test_Clock.cpp  line:18    sleep_for(1 s), QPC: 1
	filename:test_Clock.cpp  line:22    sleep_for(100 ms), QPC: 110
	filename:test_Clock.cpp  line:26    sleep_for(16 ms), QPC: 30
	* 在 O2 编译器优化下，该线程 sleep_for 表现如上，嗯，差强人意，精度要求较高时最好非阻塞处理，哪怕是让他一直循环也好
	* 
	* 在认为设置windows的精度之后，精度确实能够达到 100、16 ms，嗯嗯，还行
	*/

	return 0;
}