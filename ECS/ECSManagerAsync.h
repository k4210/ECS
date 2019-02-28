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
		friend class ECSManagerAsync;

	public:
		constexpr ExecutionStreamId() = default;
		constexpr ExecutionStreamId(int in_idx)
			: index(in_idx) 
		{
			assert(IsValid());
		}

		constexpr bool IsValid() const
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
	protected:
		using InnerTask = std::packaged_task<void()>;
		struct Task
		{
			InnerTask func;
			ComponentCache read_only_components;
			ComponentCache mutable_components;
			ExecutionStreamId preserve_order_in_execution_stream;
			LOG(const char* task_name = nullptr;)
		};

		class WorkerThread
		{
			std::optional<Task> task;
		public:
			ECSManagerAsync& owner;
			std::thread thread;
			std::atomic_bool runs = false;
			LOG(const char* const worker_name = nullptr;)

			WorkerThread(ECSManagerAsync& in_owner LOG_PARAM(const char* in_worker_name))
				: owner(in_owner)
				, thread()
				LOG_PARAM(worker_name(in_worker_name))
			{}

			const Task* GetTask_Unsafe() const
			{
				return task.has_value() ? &(*task) : nullptr;
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
						LOG(printf_s("ECS worker '%s' found '%s' stream: %d \n"
							, worker_name, task->task_name, task->preserve_order_in_execution_stream.index);)
						task->func();
						{
							std::lock_guard<std::mutex> guard(owner.mutex);
							task = {};
						}
					}
					else
					{
						//LOG(printf_s("ECS worker '%s' found no task\n", worker_name);)
						std::unique_lock<std::mutex> guard(owner.new_task_mutex);
						if (runs)
						{
							owner.new_task_cv.wait(guard);
						}
					}
				}
			}
		};
		friend WorkerThread;

		std::deque<Task> pending_tasks;
		std::mutex mutex;
		WorkerThread wt[kMaxConcurrentWorkerThreads];

		std::condition_variable new_task_cv;
		std::mutex new_task_mutex;

		std::optional<Task> main_thread_task;

		std::optional<Task> FindTaskToExecute_Unguarded()
		{
			if (pending_tasks.empty())
				return {};
			//LOG(ScopeDurationLog sdl("ECS %s in %lld us \n", "found task");)
			ExecutionStreamId::Mask unusable_streams;
			ComponentCache currently_read_only_components;
			ComponentCache currently_mutable_components;
			auto tasks_dependencies = [&](const Task& task)
			{
				task.preserve_order_in_execution_stream.MarkOnMask(unusable_streams);
				currently_read_only_components |= task.read_only_components;
				currently_mutable_components |= task.mutable_components;
			};
			for (auto& t : wt)
			{
				const Task* task = t.GetTask_Unsafe();
				if (!task)
					continue;
				tasks_dependencies(*task);
			}
			if (main_thread_task.has_value())
			{
				tasks_dependencies(*main_thread_task);
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
			return {};
		}
	public:
		ECSManagerAsync()
			: ECSManager()
			, wt{ { *this LOG_PARAM("wt0") }, { *this LOG_PARAM("wt1") }, { *this LOG_PARAM("wt2") }, { *this LOG_PARAM("wt3") } }
		{}

		void StartThreads() 
		{
			for (auto& t : wt)
			{
				assert(!t.IsRunning());
				t.runs = true;
				t.thread = std::thread(&WorkerThread::Loop, &t);
			}
		}
		void StopThreads() 
		{
			for (auto& t : wt)
			{
				assert(t.IsRunning());
				t.runs = false;
			}
			{
				std::unique_lock<std::mutex> guard(new_task_mutex);
				new_task_cv.notify_all();
			}
			for (auto& t : wt)
			{
				t.thread.join();
			}
		}
		bool AnyWorkerIsBusy()
		{
			std::lock_guard<std::mutex> guard(mutex);
			for (auto& t : wt)
			{
				if (t.GetTask_Unsafe())
					return true;
			}
			return false;
		}
		bool WorkFromMainThread(bool bSingleJob)
		{
			assert(!main_thread_task.has_value());
			bool result = false;
			do
			{
				{
					std::lock_guard<std::mutex> guard(mutex);
					main_thread_task = FindTaskToExecute_Unguarded();
				}

				if (main_thread_task.has_value())
				{
					LOG(printf_s("ECS main thread found '%s' stream: %d \n"
						, main_thread_task->task_name, main_thread_task->preserve_order_in_execution_stream.index);)
					main_thread_task->func();
					{
						std::lock_guard<std::mutex> guard(mutex);
						main_thread_task = {};
					}
					result = true;
				}
				else
				{
					//LOG(printf_s("ECS main thread found no task\n");)
					break;
				}
			}
			while(!bSingleJob);
			return result;
		}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		std::future<void> CallAsync(const std::function<void(EntityId, TDecoratedComps...)>& func
			, ExecutionStreamId preserve_order_in_execution_stream = {}
			LOG_PARAM(const char* task_name = nullptr))
		{
			assert(debug_lock);
			//We can ignore filter in the following masks:
			constexpr ComponentCache read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>::Build<TDecoratedComps...>();
			constexpr ComponentCache mutable_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDecoratedComps...>();
			static_assert((read_only_components & mutable_components).none(), "");

			InnerTask task([this, func LOG_PARAM(task_name)]() { Call<TFilter>(func LOG_PARAM(task_name)); });
			auto future = task.get_future();

			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(Task{ std::move(task), read_only_components, mutable_components, preserve_order_in_execution_stream LOG_PARAM(task_name) });
			}
			LOG(printf_s("ECS new async task: '%s'\n", task_name);)

			new_task_cv.notify_one();

			return future;
		}


	};
}