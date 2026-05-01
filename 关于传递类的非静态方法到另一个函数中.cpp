/*
* 
* 1、一般直接通过 f1([this](){}); 这样子方法
* 
*	但是如果是：
*	test_VideoFfmpegEncoder.cpp 中的情况
*	直接传递
* 
* 	Person::ptr p1 = std::make_shared<Person>();
*	f1(std::bind(&Person::foo, p1.get(), std::placeholders::_1));
* 
*	就行了
* 
* 
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
		Person::ptr p1 = std::make_shared<Person>();
		f1(std::bind(&Person::foo, p1.get(), std::placeholders::_1));

		return 0;
	}
* 
* 
* 
*/