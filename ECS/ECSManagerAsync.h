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

		void Open()
		{
			{
				std::lock_guard<std::mutex> lk(mutex);
				state = EState::Open;
			}
			cv.notify_one();
		}
	};

	struct ExecutionNodeId
	{
		using Mask = Bitset2::bitset2<kMaxExecutionNode>;
	private:
		int index = -1;
		friend class ECSManagerAsync;
	public:
		constexpr ExecutionNodeId() = default;
		constexpr ExecutionNodeId(int in_idx)
			: index(in_idx)
		{
			assert(IsValid());
		}

		constexpr int GetIndex() const { return index; }

		constexpr bool IsValid() const
		{
			return (index >= 0) && (index < kMaxExecutionNode);
		}

		constexpr void MarkOnMask(Mask& mask) const
		{
			if (IsValid())
			{
				mask.set(index, true);
			}
		}

		constexpr Mask M() const
		{
			Mask m; 
			MarkOnMask(m);
			return m;
		}

		constexpr bool Test(const Mask& mask) const
		{
			return IsValid() ? mask.test(index) : false;
		}
	};

	namespace Details
	{
		struct Task;
		using InnerSyncFunc = std::add_pointer<void(ECSManager&, Task&)>::type;
		struct Task
		{
			InnerSyncFunc func = nullptr;
			void* per_entity_function = nullptr;
			void* per_entity_function_second_pass = nullptr;
			Details::ComponentCache read_only_components;
			Details::ComponentCache mutable_components;
			ExecutionNodeId::Mask required_completed_tasks;
			ExecutionNodeId execution_id;
			ThreadGate* optional_notifier = nullptr;
		};

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallGeneric(ECSManager& ecs, Task& task)
		{
			using TFuncPtr = typename std::add_pointer_t<void(EntityId, TDecoratedComps...)>;
			assert(!!task.per_entity_function);
			TFuncPtr func = reinterpret_cast<TFuncPtr>(task.per_entity_function);
			ecs.Call<TFilter>(func);
		}

		template<typename TFilterA = typename Filter<>, typename TFilterB = typename Filter<>, typename THolder, typename TFuncPtr_FP, typename TFuncPtr_SP>
		void CallGeneric2(ECSManager& ecs, Task& task)
		{
			assert(!!task.per_entity_function);
			TFuncPtr_FP func_fp = reinterpret_cast<TFuncPtr_FP>(task.per_entity_function);

			assert(!!task.per_entity_function_second_pass);
			TFuncPtr_SP func_sp = reinterpret_cast<TFuncPtr_SP>(task.per_entity_function_second_pass);

			ecs.CallOverlap<TFilterA, TFilterB, THolder>(func_fp, func_sp);
		}
	}

	class ECSManagerAsync : public ECSManager
	{
	protected:
		class WorkerThread
		{
			std::optional<Details::Task> task;
		public:
			ECSManagerAsync& owner;
			std::thread thread;
			std::atomic_bool runs = false;
			int worker_idx = -1;

			WorkerThread(ECSManagerAsync& in_owner, int idx)
				: owner(in_owner), thread(), worker_idx(idx)
			{}

			const Details::Task* GetTask_Unsafe() const
			{
				return task.has_value() ? &(*task) : nullptr;
			}

			bool IsRunning() const
			{
				const bool joinable = thread.joinable();
				assert(joinable == runs);
				return joinable;
			}

			static bool TryExecuteTask(std::optional<Details::Task>& task, ECSManagerAsync& owner LOG_PARAM(int worker_idx))
			{
				{
					std::lock_guard<std::mutex> guard(owner.mutex);
					task = owner.FindTaskToExecute_Unguarded();
				}

				if (task.has_value())
				{
					LOG(printf_s("ECS worker %d found '%s'\n", worker_idx, Str(task->execution_id.GetIndex()));)
					{
						STAT(ScopeDurationLog __sdl(task->execution_id.GetIndex());)
						task->func(owner, *task);
					}
					auto optional_notifier = task->optional_notifier;
					{
						std::lock_guard<std::mutex> guard(owner.mutex);
						task->execution_id.MarkOnMask(owner.completed_tasks);
						task = {};
					}
					if (optional_notifier)
					{
						optional_notifier->Open();
					}
					return true;
				}
				return false;
			}

			void Loop()
			{
				while (runs)
				{
					const bool bExecuted = TryExecuteTask(task, owner LOG_PARAM(worker_idx));
					if(!bExecuted)
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

		std::deque<Details::Task> pending_tasks;
		std::mutex mutex;
		WorkerThread wt[kMaxConcurrentWorkerThreads];

		std::condition_variable new_task_cv;
		std::mutex new_task_mutex;

		std::optional<Details::Task> main_thread_task;

		ExecutionNodeId::Mask completed_tasks;

		std::optional<Details::Task> FindTaskToExecute_Unguarded()
		{
			if (pending_tasks.empty())
				return {};
			STAT(ScopeDurationLog __sdl(-1);)
			
			Details::ComponentCache currently_read_only_components;
			Details::ComponentCache currently_mutable_components;
			auto tasks_dependencies = [&](const Details::Task& task)
			{
				currently_read_only_components |= task.read_only_components;
				currently_mutable_components |= task.mutable_components;
			};
			for (auto& t : wt)
			{
				const Details::Task* task = t.GetTask_Unsafe();
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
				const bool ok_required_tasks = IsSubSetOf(it->required_completed_tasks, completed_tasks);
				const bool ok_components = !AnyCommonBit(it->mutable_components, currently_read_only_components)
					&& !AnyCommonBit(it->read_only_components, currently_mutable_components)
					&& !AnyCommonBit(it->mutable_components, currently_mutable_components);
				if (ok_required_tasks && ok_components)
				{
					std::optional<Details::Task> result(std::move(*it));
					pending_tasks.erase(it);
					return result;
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
				const bool bExecuted = WorkerThread::TryExecuteTask(main_thread_task, *this LOG_PARAM(-1));
				if(!bExecuted)
					break;
			}
			while(!bSingleJob);
			return result;
		}
		void ResetCompletedTasks()
		{
			completed_tasks.reset();
		}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallAsync(void(*func)(EntityId, TDecoratedComps...)
			, ExecutionNodeId node_id
			, ExecutionNodeId::Mask requiried_completed_tasks = {}
			, ThreadGate* optional_notifier = nullptr)
		{
			assert(node_id.IsValid());
			//We can ignore filter in the following masks:
			constexpr Details::ComponentCache read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>::Build<TDecoratedComps...>();
			constexpr Details::ComponentCache mutable_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDecoratedComps...>();
			static_assert((read_only_components & mutable_components).none(), "");

			Details::InnerSyncFunc inner_func = &Details::CallGeneric<TFilter, TDecoratedComps...>;
			void* per_entity_func = func;
			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(Details::Task{ inner_func
					, per_entity_func
					, nullptr
					, read_only_components
					, mutable_components
					, requiried_completed_tasks
					, node_id
					, optional_notifier });
			}

			LOG(printf_s("ECS new async task: '%s'\n", Str(node_id.GetIndex()));)

			new_task_cv.notify_one();
		}

		template<typename TFilterA = typename Filter<>, typename TFilterB = typename Filter<>, typename THolder, typename... TDComps1, typename... TDComps2>
		void CallAsyncOverlap(THolder(*first_pass)(EntityId, TDComps1...)
			, void(*second_pass)(THolder&, EntityId, TDComps2...)
			, ExecutionNodeId node_id
			, ExecutionNodeId::Mask requiried_completed_tasks = {}
			, ThreadGate* optional_notifier = nullptr)
		{
			assert(node_id.IsValid());
			//We can ignore filter in the following masks:
			constexpr Details::ComponentCache read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst  >::Build<TDComps1..., TDComps2...>();
			constexpr Details::ComponentCache mutable_components   = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDComps1..., TDComps2...>();
			static_assert((read_only_components & mutable_components).none(), "");

			using TFuncPtr_FP = typename std::add_pointer_t<THolder(EntityId, TDComps1...)>;
			using TFuncPtr_SP = typename std::add_pointer_t<void(THolder&, EntityId, TDComps2...)>;
			Details::InnerSyncFunc inner_func = &Details::CallGeneric2<TFilterA, TFilterB, THolder, TFuncPtr_FP, TFuncPtr_SP>;
			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(Details::Task{ inner_func
					, first_pass
					, second_pass
					, read_only_components
					, mutable_components
					, requiried_completed_tasks
					, node_id
					, optional_notifier });
			}

			LOG(printf_s("ECS new async task: '%s'\n", Str(node_id.GetIndex()));)
			new_task_cv.notify_one();
		}

	};
}