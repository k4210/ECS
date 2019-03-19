#pragma once

#include "ECSBase.h"
#include "ECSStat.h"
#include "concurrentqueue\concurrentqueue.h"

namespace ECS
{
	__interface IEvent
	{
	public:
		void Execute();
	};

	struct EventStorage
	{
	private:
		alignas(std::alignment_of_v<int*>) std::array<uint8_t, 32> data = { 0 };
	public:

		template<class TEvent, typename... Args>
		static EventStorage Create(Args&&... args)
		{
			static_assert(std::is_base_of_v< IEvent, TEvent>, "is_base_of_v");
			static_assert(sizeof(TEvent) <= sizeof(EventStorage), "sizeof");
			static_assert(std::alignment_of_v<TEvent> <= std::alignment_of_v<EventStorage>, "alignment_of_v");
			//static_assert(std::is_trivially_copyable_v<TEvent>, "is_trivially_copyable_v");
			static_assert(std::is_trivially_destructible_v<TEvent>, "is_trivially_destructible_v");
			EventStorage es;
			assert(!es.IsValid());
			new (&es.data) TEvent(std::forward<Args>(args)...);
			assert(es.IsValid());
			return es;
		}

		bool IsValid() const
		{
			auto v_ptr = *reinterpret_cast<uint32_t* const*>(&data);
			return v_ptr != nullptr;
		}

		IEvent* Get()
		{
			return IsValid() ? reinterpret_cast<IEvent*>(&data) : nullptr;
		}
	};
	static_assert(sizeof(EventStorage) == 32, "");

	class EventManager
	{
		moodycamel::ConcurrentQueue<EventStorage> queue;
	public:
		EventManager() : queue(256, 0, kMaxConcurrentWorkerThreads + 1) {}

		void Push(EventStorage&& e)
		{
			ScopeDurationLog __sdl(Details::EStatId::PushEvent, EPredefinedStatGroups::InnerLibrary);
			queue.enqueue(e);
		}

		bool Pop(EventStorage& result)
		{
			ScopeDurationLog __sdl(Details::EStatId::PopEvent, EPredefinedStatGroups::InnerLibrary);
			return queue.try_dequeue(result);
		}
	};
}