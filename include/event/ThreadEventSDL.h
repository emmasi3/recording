#pragma once
#include "infra/Logger.h"
#include "factory/singleton.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>

namespace streamer
{
	// 当前状态
	enum class STATE
	{
		Start,
		Term,
	};

	/*
	* @brief 使用窗口监听键盘事件，管理程序的整个生命周期
	*/
	class SDL_event_Thread
	{
	public:
		typedef std::shared_ptr<SDL_event_Thread> ptr;

		SDL_event_Thread();

		~SDL_event_Thread();

		/*
		* @brief 开启事件循环，阻塞当前线程
		* @note 调用该方法后，由于当前线程阻塞，所以后续的线程任务放入线程队列时，需要在其他线程中放入，
		* @note 或者说，在抽象工厂将所有的组件、线程函数都启动后，再调用此方法开始全局事件监控(主要针对SDL的鼠标、键盘事件)
		* @note 当然，这里的目的：为了方便随时停止录制或者其余操作；所以可以用 QT 来做
		*/
		void Start();
		
		/*
		* @brief 停止事件循环，并终止线程（ALL）
		*/
		void Stop();

		STATE get_state() const { return m_state; }

		void set_state(const STATE val) { m_state = val; }
		
		/*
		* @brief 创建新线程，并放入数组中
		* @brief 使用该方法时，调用时机：
		*		(1)若和 Start() 处于同一线程，
		*		必须放在 Start() 之前进行
		*		(2)若处于不同线程，自己看着办。
		* @brief 建议在 Start() 之前添加所有工作的线程，手动处理有可能报错
		* @param func 线程函数
		*/
		// void push_thread_to_vector(std::function<void()> func);

		/*
		* @brief 接受线程函数，并放入线程函数数组中
		* @brief 不直接开启线程，统一在 start() 中开启
		* @note 未来可以用 unordered_map 来用键值对记录线程函数的更多信息 or ID
		* @param func 线程函数
		*/
		void push_thread_to_vector(std::function<void()> func);

		/*
		* @brief 接收线程函数，并立即启动线程，再放入线程队列
		* @brief 用于在已注册(m_thread_funcs)线程中开始其他线程
		* @param func 线程函数
		* @note 要避免同时访问 m_threads 线程队列
		*/
		void push_threadfunc_to_threads(std::function<void()> func);

		/*
		* @brief 获取线程函数数组大小，注册列表大小
		*/
		int get_threadfuncs_counts() const;

		/*
		* @brief 获取线程数组大小，线程数量
		*/
		int get_threads_counts() const;
		
	private:
		// 原子变量
		std::atomic<STATE> m_state;
		// 存放线程函数
		std::vector<std::function<void()>> m_thread_funcs;
		// 线程数组
		std::vector<std::thread> m_threads;
		// 互斥量
		mutable std::mutex m_mtx;
		
	};

	// 单例模式: SDL::GetInstance()->···
	typedef Singleton<SDL_event_Thread> SDL;

}