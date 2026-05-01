#include "include\adapters\codec\FfmpegEncoder.h"

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

class Person
{
public:
	typedef std::shared_ptr<Person> ptr;

	void foo(int i)
	{
		LOG_INFO(g_logger) << "hello, fack: " << i;
	}
};

void f1(std::function<void(int i)> func)
{
	for (int i = 0; i < 5; ++i)
	{
		func(i);
	}
}

int main(int argc, char* argv[])
{
	//LOG_INFO(g_logger) << "nihoa";
	//auto v_encoder_ptr = streamer::VideoFfmpegEncoder::createNew();

	Person::ptr p1 = std::make_shared<Person>();
	f1(std::bind(&Person::foo, p1.get(), std::placeholders::_1));

	return 0;
}