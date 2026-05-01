#include "include\concurrency\ConcurrentQueue.h"
#include "include\infra\Logger.h"

#include <thread>
#include <chrono>
#include <memory>

static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

// 简单的生产者-消费者测试：
// 1) writer 线程写入若干正整数，写入完成后写入一个哨兵值 -1 表示结束，然后调用 Close()
// 2) reader 线程循环 WaitAndPop()，遇到哨兵值 -1 则退出
// 所有读写操作使用 LOG_INFO 打印，便于观察行为

int main()
{
	LOG_INFO(g_logger) << "concurrent queue test start";

	streamer::BlockingQueue<int> q;

	// writer thread
	std::thread writer([&q]() {
		for (int i = 1; i <= 10; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			q.Push(i);
			LOG_INFO(g_logger) << "writer: pushed " << i;
		}

		// push sentinel
		q.Push(-1);
		LOG_INFO(g_logger) << "writer: pushed sentinel -1 and closing queue";
		q.Close();
	});

	// reader thread
	std::thread reader([&q]() {
		while (true) {
			int v = q.WaitAndPop();
			// When queue is closed and empty WaitAndPop returns default-initialized int (0).
			// We use sentinel -1 to signal normal end; if 0 appears unexpectedly, continue or break
			if (v == -1) {
				LOG_INFO(g_logger) << "reader: received sentinel -1, exiting";
				break;
			}
			if (v == 0) {
				// could be default return when queue closed; no data
				LOG_INFO(g_logger) << "reader: received default 0 (queue may be closed and empty), exiting";
				break;
			}
			LOG_INFO(g_logger) << "reader: popped " << v;
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}
	});

	writer.join();
	reader.join();

	LOG_INFO(g_logger) << "concurrent queue test finished";

	// 使用建议：
	// - 在生产者-消费者模型中，创建一个 BlockingQueue<T> 作为共享缓冲区，生产者调用 Push()
	//   将数据放入队列，消费者调用 WaitAndPop() 阻塞等待并取出数据。
	// - 使用 Close() 来通知所有等待中的消费者队列即将关闭，消费者在收到哨兵或者
	//   WaitAndPop() 返回默认值时，应当退出循环并清理资源。
	// - 若需要多个生产者或多个消费者，只需在各自线程中调用 Push()/WaitAndPop()，该实现
	//   已使用互斥量和条件变量保证线程安全。
	// - 推荐在生产者结束后显式推送一个哨兵（特殊值）或使用 Close()，并让消费者识别哨兵
	//   以进行有序终止，避免误把默认值当做有效数据。

	return 0;
}
