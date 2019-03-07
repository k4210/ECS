#pragma once

#include "ECSManager.h"
#include <optional>
#include <deque>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include "ECSStat.h"

namespace ECS
{
	namespace Details
	{
		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallGeneric(ECSManager& ecs, void* func_ptr)
		{
			using TFuncPtr = typename std::add_pointer_t<void(EntityId, TDecoratedComps...)>;
			TFuncPtr func = reinterpret_cast<TFuncPtr>(func_ptr);
			ecs.Call<TFilter>(func);
		}
	}

	class ThreadGate
	{
		enum class EState { Close, Open };
		std::atomic<EState> state = EState::Close;
		std::mutex mutex;
		std::condition_variable cv;

	public:
		void WaitEnterClose()
		{
			std::unique_lock<std::mutex> lk(mutex);
			cv.wait(lk, [this]() { return EState::Open == state; });
			state = EState::Close;
		}

		void WaitEnterClose(std::atomic_bool& close_request)
		{
			std::unique_lock<std::mutex> lk(mutex);
			cv.wait(lk, [this, &close_request]() { return (EState::Open == state) || close_request; });
			state = EState::Close;
		}

		void Open()
		{
			{
				std::lock_guard<std::mutex> lk(mutex);
				state = EState::Open;
			}
			cv.notify_one();
		}

		void JustNotifyAll()
		{
			cv.notify_all();
		}
	};

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
		using InnerSyncFunc = std::add_pointer<void(ECSManager&, void*)>::type;
		struct Task
		{
			InnerSyncFunc func = nullptr;
			void* per_entity_function = nullptr;
			ComponentCache read_only_components;
			ComponentCache mutable_components;
			ExecutionStreamId preserve_order_in_execution_stream;
			ThreadGate* optional_notifier = nullptr;
			STAT(StatId stat_id = StatId::Count;)
		};

		class WorkerThread
		{
			std::optional<Task> task;
		public:
			ECSManagerAsync& owner;
			std::thread thread;
			std::atomic_bool runs = false;
			int worker_idx = -1;

			WorkerThread(ECSManagerAsync& in_owner, int idx)
				: owner(in_owner), thread(), worker_idx(idx)
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
						LOG(printf_s("ECS worker %d found '%s' stream: %d \n"
							, worker_idx, StatIdToStr(task->stat_id), task->preserve_order_in_execution_stream.index);)
						{
							STAT(ScopeDurationLog __sdl(task->stat_id);)
							task->func(owner, task->per_entity_function);
						}
						auto optional_notifier = task->optional_notifier;
						{
							std::lock_guard<std::mutex> guard(owner.mutex);
							task = {};
						}
						if (optional_notifier)
						{
							optional_notifier->Open();
						}
					}
					else
					{
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
			STAT(ScopeDurationLog __sdl(StatId::FindTaskToExecute);)
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
				const bool ok_components = !Details::AnyCommonBit(it->mutable_components, currently_read_only_components)
					&& !Details::AnyCommonBit(it->read_only_components, currently_mutable_components)
					&& !Details::AnyCommonBit(it->mutable_components, currently_mutable_components);
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
			, wt{ { *this, 0 }, { *this, 1 }, { *this, 2 }, { *this, 3 } }
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
						, StatIdToStr(main_thread_task->stat_id), main_thread_task->preserve_order_in_execution_stream.index);)
					{
						STAT(ScopeDurationLog __sdl(main_thread_task->stat_id);)
						main_thread_task->func(*this, main_thread_task->per_entity_function);
					}
					auto optional_notifier = main_thread_task->optional_notifier;
					{
						std::lock_guard<std::mutex> guard(mutex);
						main_thread_task = {};
					}
					if (optional_notifier)
					{
						optional_notifier->Open();
					}
					result = true;
				}
				else
				{
					break;
				}
			}
			while(!bSingleJob);
			return result;
		}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallAsync(void(*func)(EntityId, TDecoratedComps...)
			, ExecutionStreamId preserve_order_in_execution_stream
			, ThreadGate* optional_notifier
			STAT_PARAM(StatId stat_id))
		{
			assert(debug_lock);
			//We can ignore filter in the following masks:
			constexpr ComponentCache read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>::Build<TDecoratedComps...>();
			constexpr ComponentCache mutable_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDecoratedComps...>();
			static_assert((read_only_components & mutable_components).none(), "");

			InnerSyncFunc inner_func = &Details::CallGeneric<TFilter, TDecoratedComps...>;
			void* per_entity_func = func;
			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(Task{ inner_func
					, per_entity_func
					, read_only_components
					, mutable_components
					, preserve_order_in_execution_stream
					, optional_notifier 
					STAT_PARAM(stat_id) });
			}

			LOG(printf_s("ECS new async task: '%s'\n", StatIdToStr(stat_id));)

			new_task_cv.notify_one();
		}
	};
}