#define SDL_MAIN_HANDLED
#include "event/ThreadEventSDL.h"
#include <SDL.h>

namespace streamer
{
	static streamer::ILogger::ptr g_logger = streamer::ILogger::ptr(new streamer::ConsoleLogger(streamer::LogLevel::Debug));

	SDL_event_Thread::SDL_event_Thread()
		:m_state(STATE::Start)
	{

	}

	void SDL_event_Thread::Start()
	{
		SDL_Window* window = SDL_CreateWindow("Invisible Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_SHOWN);
		if (!window) {
			SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
			return;
		}

		SDL_Event event;
		do
		{
			SDL_WaitEvent(&event);

			switch (event.type)
			{
			case SDL_QUIT:
				LOG_WARN(g_logger) << "the key is : NULL,but forced to QUIT!";
				Stop();
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_q)
				{
					LOG_WARN(g_logger) << "the key is SDLK_q,so QUIT!";
					Stop();
				}
				break;
			default:
				LOG_INFO(g_logger) << "the key is Invilid";
				break;
			}

		} while (m_state != STATE::Term);

		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	SDL_event_Thread::~SDL_event_Thread()
	{

	}


	void SDL_event_Thread::Stop()
	{
		// 修改状态 -- 终止
		m_state = STATE::Term;

		for (auto& t : m_threads)
		{
			if (t.joinable())
			{
				t.join();
			}
		}

		LOG_INFO(g_logger) << "All of threads is term";
	}

	void SDL_event_Thread::push_thread_to_vector(std::function<void()> func)
	{
		m_threads.emplace_back(func);
	}
}