#pragma once
#include "ECSBase.h"
#include "concurrentqueue\concurrentqueue.h""

namespace ECS
{
	struct Event
	{
		EntityHandle sender;
		EntityHandle receiver;
		int msg_id = -1;
		std::aligned_storage<4 * sizeof(int), alignof(int)> raw_data;
		template<typename T>
		T* GetData()
		{
			static_assert(sizeof(T) <= (4 * sizeof(int)), "");
			static_assert(alignof(T) <= sizeof(int), "");
			return reinterpret_cast<const T*>(&raw_data);
		}
	};
	static_assert(sizeof(Event) == (8 * sizeof(int)), "");

	class EventManager
	{
		moodycamel::ConcurrentQueue<Event> queue;
	public:
		EventManager() : queue(256, 0, kMaxConcurrentWorkerThreads + 1) {}

		void Push(Event&& e)
		{
			queue.enqueue(e);
		}

		bool Pop(Event& result)
		{
			return queue.try_dequeue(result);
		}
	};
}