#pragma once

#include "ECSBase.h"

#include "concurrentqueue\concurrentqueue.h"

namespace ECS
{
	struct Event
	{
		std::aligned_storage<3 * sizeof(int), alignof(int)>::type raw_data;
		EntityHandle sender;
		EntityHandle receiver;
		int16_t receiver_component = -1;
		int16_t sender_component = -1;
		template<typename T> T* GetData()
		{
			static_assert(sizeof(T) <= (3 * sizeof(int)), "");
			static_assert(alignof(T) <= sizeof(int), "");
			return reinterpret_cast<const T*>(&raw_data);
		}
	};
	static_assert(sizeof(Event) == (6 * sizeof(int)), "");

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