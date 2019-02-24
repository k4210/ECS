#pragma once

#include "ECSManager.h"
#include <future>

namespace ECS
{
	class ECSManagerAsync : public ECSManager
	{
	public:
		void StartThreads() {}
		void StopThreads() {}
		void WaitForAllTasks() {}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		std::future<void> CallAsync(const std::function<void(EntityId, TDecoratedComps...)>& Func)
		{
			return {};
		}

	};
}