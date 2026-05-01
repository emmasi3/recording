#pragma once

namespace streamer
{

	template<class T>
	class Singleton
	{
	public:
		static T* GetInstance()
		{
			static T v;
			return &v;
		}

	};


}