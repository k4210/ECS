#pragma once

#include "ECSManager.h"
#include <future>
#include <optional>
#include <deque>
#include <thread>
#include <mutex>
#include <chrono>

namespace ECS
{
	struct ExecutionStreamId
	{
		using Mask = Bitset2::bitset2<kMaxExecutionStream>;
	private:
		int index = -1;

	public:
		ExecutionStreamId() = default;
		ExecutionStreamId(int in_idx) 
			: index(in_idx) 
		{
			assert(IsValid());
		}

		bool IsValid() const
		{
			return (index >= 0) && (index < kMaxExecutionStream);
		}

		void MarkOnMask(Mask& mask) const
		{
			if (IsValid())
			{
				mask.set(index, true);
			}
		}

		bool Test(const Mask& mask) const
		{
			return IsValid() ? mask.test(index) : false;
		}
	};

	class ECSManagerAsync : public ECSManager
	{
	public:
		using InnerTask = std::packaged_task<void()>;
	protected:
		struct Task
		{
			InnerTask func;
			ComponentCache read_only_components;
			ComponentCache mutable_components;
			ExecutionStreamId preserve_order_in_execution_stream;
		};

		class WorkerThread
		{
			void ActiveSleep()
			{
				const std::chrono::microseconds sleep_time(50);
				const auto start = std::chrono::high_resolution_clock::now();
				const auto end = start + sleep_time;
				do
				{
					std::this_thread::yield();
				} while (std::chrono::high_resolution_clock::now() < end);
			}
		public:
			ECSManagerAsync& owner;
			std::thread thread;
			std::optional<Task> task;
			std::atomic_bool runs = false;

			WorkerThread(ECSManagerAsync& in_owner)
				: owner(in_owner)
				, thread()
			{}

			void Start()
			{
				assert(!IsRunning());
				runs = true;
				thread = std::thread(&WorkerThread::Loop, this);
			}

			void Stop()
			{
				assert(IsRunning());
				runs = false;
				thread.join();
			}

			bool IsRunning() const
			{
				const bool joinable = thread.joinable();
				assert(joinable == runs);
				return joinable;
			}

			void Loop()
			{
				while (runs)
				{
					{
						std::lock_guard<std::mutex> guard(owner.mutex);
						task = owner.FindTaskToExecute_Unguarded();
					}

					if (task.has_value())
					{
						task->func();
						{
							std::lock_guard<std::mutex> guard(owner.mutex);
							task = {};
						}
					}
					else
					{
						ActiveSleep();
					}
				}
			}
		};
		friend WorkerThread;

		std::deque<Task> pending_tasks;
		std::mutex mutex;
		WorkerThread wt[kMaxConcurrentWorkerThreads];

		std::optional<Task> FindTaskToExecute_Unguarded()
		{
			if (!pending_tasks.empty())
			{
				ExecutionStreamId::Mask unusable_streams;
				ComponentCache currently_read_only_components;
				ComponentCache currently_mutable_components;
				for (auto& t : wt)
				{
					if (t.task.has_value())
					{
						Task& task = *t.task;
						task.preserve_order_in_execution_stream.MarkOnMask(unusable_streams);
						currently_read_only_components |= task.read_only_components;
						currently_mutable_components |= task.mutable_components;
					}
				}
				for (auto it = pending_tasks.begin(); it != pending_tasks.end(); it++)
				{
					const bool ok_stream = !it->preserve_order_in_execution_stream.Test(unusable_streams);
					const bool ok_components = !Details::any_common_bit(it->mutable_components, currently_read_only_components)
						&& !Details::any_common_bit(it->read_only_components, currently_mutable_components)
						&& !Details::any_common_bit(it->mutable_components, currently_mutable_components);
					if (ok_stream && ok_components)
					{
						std::optional<Task> result(std::move(*it));
						pending_tasks.erase(it);
						return result;
					}
					if (ok_stream && it->preserve_order_in_execution_stream.IsValid())
					{
						it->preserve_order_in_execution_stream.MarkOnMask(unusable_streams);
					}
				}
			}
			return {};
		}
	public:
		ECSManagerAsync()
			: ECSManager()
			, wt{ { *this }, { *this }, { *this }, { *this } }
		{}

		void StartThreads() 
		{
			for (auto& t : wt)
			{
				t.Start();
			}
		}
		void StopThreads() 
		{
			for (auto& t : wt)
			{
				t.Stop();
			}
		}
		bool AnyWorkerIsBusy()
		{
			std::lock_guard<std::mutex> guard(mutex);
			for (auto& t : wt)
			{
				if (t.task.has_value())
				{
					return true;
				}
			}
			return false;
		}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		std::future<void> CallAsync(const std::function<void(EntityId, TDecoratedComps...)>& func, ExecutionStreamId preserve_order_in_execution_stream = {})
		{
			//We can ignore filter in the following masks:
			constexpr ComponentCache read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>::Build<TDecoratedComps...>();
			constexpr ComponentCache mutable_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDecoratedComps...>();
			static_assert((read_only_components & mutable_components).none(), "");

			InnerTask task([this, func]() { Call(func); });
			auto future = task.get_future();

			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(Task{ std::move(task), read_only_components, mutable_components, preserve_order_in_execution_stream });
			}

			return future;
		}
	};
}