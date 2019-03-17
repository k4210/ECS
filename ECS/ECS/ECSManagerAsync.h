#pragma once

#include "ECSBase.h"
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
	private:
		constexpr static const uint16_t kInvalidValue = UINT16_MAX;
		uint16_t index = kInvalidValue;
		friend class ECSManagerAsync;
		friend struct ExecutionNodeIdSet;
	public:
		constexpr ExecutionNodeId() = default;
		constexpr ExecutionNodeId(uint16_t in_idx)
			: index(in_idx)
		{
			assert(IsValid());
		}

		constexpr int GetIndex() const { return index; }

		constexpr bool IsValid() const
		{
			return (index != kInvalidValue) && (index < kMaxExecutionNode);
		}
	};

	struct ExecutionNodeIdSet
	{
		Bitset2::bitset2<kMaxExecutionNode> bits;

		constexpr void Add(const ExecutionNodeId& id)
		{
			if (id.IsValid())
			{
				bits.set(id.index, true);
			}
		}

		constexpr bool Test(const ExecutionNodeId& id) const
		{
			return id.IsValid() ? bits.test(id.index) : false;
		}

		ExecutionNodeIdSet() = default;

		constexpr ExecutionNodeIdSet(const ExecutionNodeId& id) 
		{ 
			Add(id); 
		}

		template<typename... TSets>constexpr ExecutionNodeIdSet(TSets... id)
		{
			(Add(id), ...);
		}
	};

	namespace AsyncDetails
	{
		struct Task;
		using InnerSyncFunc = std::add_pointer<void(ECSManager&, Task&)>::type;

		struct TaskFilter
		{
			Details::ComponentIdxSet read_only_components;
			Details::ComponentIdxSet mutable_components;
			Tag tag;

			constexpr bool Conflict(const TaskFilter& other) const
			{
				if (Tag::Match(tag, other.tag))
				{
					return AnyCommonBit(mutable_components,		other.mutable_components)
						|| AnyCommonBit(mutable_components,		other.read_only_components)
						|| AnyCommonBit(read_only_components,	other.mutable_components);
				}
				return false;
			}
		};

		struct Task
		{
			InnerSyncFunc func = nullptr;
			void* per_entity_function = nullptr;
			void* per_entity_function_second_pass = nullptr;
			TaskFilter filter;
			std::optional<TaskFilter> filter_second_pass;
			ExecutionNodeIdSet required_completed_tasks;
			ExecutionNodeId execution_id;
			ThreadGate* optional_notifier = nullptr;
		};

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallGeneric(ECSManager& ecs, Task& task)
		{
			using TFuncPtr = typename std::add_pointer_t<void(EntityId, TDecoratedComps...)>;
			assert(!!task.per_entity_function);
			TFuncPtr func = reinterpret_cast<TFuncPtr>(task.per_entity_function);
			ecs.Call<TFilter>(func, task.filter.tag);
		}
		
		template<typename TFilterA = typename Filter<>, typename TFilterB = typename Filter<>
			, typename THolder, typename TFuncPtr_FP, typename TFuncPtr_SP>
		void CallGeneric2(ECSManager& ecs, Task& task)
		{
			assert(!!task.per_entity_function);
			TFuncPtr_FP func_fp = reinterpret_cast<TFuncPtr_FP>(task.per_entity_function);

			assert(!!task.per_entity_function_second_pass);
			TFuncPtr_SP func_sp = reinterpret_cast<TFuncPtr_SP>(task.per_entity_function_second_pass);

			assert(task.filter_second_pass.has_value());
			ecs.CallOverlap<TFilterA, TFilterB, THolder>(func_fp, func_sp
				, task.filter.tag, task.filter_second_pass->tag);
		}
	}

	class ECSManagerAsync : public ECSManager
	{
	private:
		class WorkerThread
		{
			std::optional<AsyncDetails::Task> task;
		public:
			ECSManagerAsync& owner;
			std::thread thread;
			std::atomic_bool runs = false;
			int worker_idx = -1;

			WorkerThread(ECSManagerAsync& in_owner, int idx)
				: owner(in_owner), worker_idx(idx)
			{}

			const AsyncDetails::Task* GetTask_Unsafe() const
			{
				return task.has_value() ? &(*task) : nullptr;
			}

			bool IsRunning() const
			{
				const bool joinable = thread.joinable();
				assert(joinable == runs);
				return joinable;
			}

			static bool TryExecuteTask(std::optional<AsyncDetails::Task>& task, ECSManagerAsync& owner, int worker_idx)
			{
				(void)worker_idx;
				{
					std::lock_guard<std::mutex> guard(owner.mutex);
					task = owner.FindTaskToExecute_Unguarded();
				}

				if (task.has_value())
				{
					LOG("ECS worker %d found '%s'", worker_idx, Str(task->execution_id.GetIndex()));
					{
						ScopeDurationLog __sdl(task->execution_id.GetIndex());
						task->func(owner, *task);
					}
					LOG("ECS worker %d done '%s'", worker_idx, Str(task->execution_id.GetIndex()));
					auto optional_notifier = task->optional_notifier;
					const bool valid_execution_node = task->execution_id.IsValid();
					{
						std::lock_guard<std::mutex> guard(owner.mutex);
						owner.completed_tasks.Add(task->execution_id);
						task = {};
					}
					if (optional_notifier)
					{
						optional_notifier->Open();
					}
					if(valid_execution_node)
					{
						owner.new_task_cv.notify_all();
					}
					return true;
				}
				return false;
			}

			void Loop()
			{
				while (runs)
				{
					const bool bExecuted = TryExecuteTask(task, owner, worker_idx);
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

		std::deque<AsyncDetails::Task> pending_tasks;
		std::mutex mutex;
		WorkerThread wt[kMaxConcurrentWorkerThreads];

		std::condition_variable new_task_cv;
		std::mutex new_task_mutex;

		std::optional<AsyncDetails::Task> main_thread_task;

		ExecutionNodeIdSet completed_tasks;

		std::optional<AsyncDetails::Task> FindTaskToExecute_Unguarded()
		{
			if (pending_tasks.empty())
				return {};
			ScopeDurationLog __sdl(-1);
			
			auto tasks_conflict = [](const AsyncDetails::Task& a, const AsyncDetails::Task& b) -> bool
			{
				if (a.filter.Conflict(b.filter))
					return true;

				if(b.filter_second_pass.has_value() && a.filter.Conflict(*b.filter_second_pass))
					return true;

				if (a.filter_second_pass.has_value())
				{
					if (a.filter_second_pass->Conflict(b.filter))
						return true;

					if (b.filter_second_pass.has_value() && a.filter_second_pass->Conflict(*b.filter_second_pass))
						return true;
				}

				return false;
			};

			auto conflict_with_other_threads = [&](const AsyncDetails::Task& pending_task) -> bool
			{
				for (auto& t : wt)
				{
					const AsyncDetails::Task* task = t.GetTask_Unsafe();
					if (task && tasks_conflict(pending_task, *task))
						return true;
				}
				return main_thread_task.has_value()
					? tasks_conflict(pending_task, *main_thread_task)
					: false;
			};

			for (auto it = pending_tasks.begin(); it != pending_tasks.end(); it++)
			{
				if (!IsSubSetOf(it->required_completed_tasks.bits, completed_tasks.bits))
					continue;

				if (conflict_with_other_threads(*it))
					continue;

				assert(!completed_tasks.Test(it->execution_id));
				std::optional<AsyncDetails::Task> result(std::move(*it));
				const auto remaining_size = pending_tasks.size();
				pending_tasks.erase(it);
				assert((remaining_size - 1) == pending_tasks.size());
				return result;
			}
			return {};
		}

		template<std::size_t... indexes>
		ECSManagerAsync(std::index_sequence<indexes...>)
			: ECSManager(), wt{ {*this, indexes}... } 
		{}

	public:

		ECSManagerAsync()
			: ECSManagerAsync(std::make_index_sequence<kMaxConcurrentWorkerThreads>{})
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
			if (!pending_tasks.empty())
				return true;
			return false;
		}
		bool WorkFromMainThread(bool bSingleJob)
		{
			assert(!main_thread_task.has_value());
			bool result = false;
			do
			{
				const bool bExecuted = WorkerThread::TryExecuteTask(main_thread_task, *this, -1);
				if(bExecuted) 
					result = true;
				else 
					break;
			}
			while(!bSingleJob);
			return result;
		}
		void ResetCompletedTasks()
		{
			std::lock_guard<std::mutex> guard(mutex);
			assert(pending_tasks.empty());
			completed_tasks.bits.reset();
		}

		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void CallAsync(void(*func)(EntityId, TDecoratedComps...)
			, Tag tag
			, ExecutionNodeId node_id
			, ExecutionNodeIdSet requiried_completed_tasks = {}
			, ThreadGate* optional_notifier = nullptr)
		{
			assert(node_id.IsValid());
			constexpr Details::ComponentIdxSet read_only_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>::Build<TDecoratedComps...>();
			constexpr Details::ComponentIdxSet mutable_components = Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>::Build<TDecoratedComps...>();
			static_assert((read_only_components & mutable_components).none(), "");

			AsyncDetails::InnerSyncFunc inner_func = &AsyncDetails::CallGeneric<TFilter, TDecoratedComps...>;
			void* per_entity_func = func;
			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(AsyncDetails::Task{ inner_func
					, per_entity_func
					, nullptr
					, AsyncDetails::TaskFilter{read_only_components, mutable_components, tag}
					, {}
					, requiried_completed_tasks
					, node_id
					, optional_notifier });
			}
			new_task_cv.notify_one();
		}

		template<typename TFilterA = typename Filter<>, typename TFilterB = typename Filter<>, typename THolder, typename... TDComps1, typename... TDComps2>
		void CallAsyncOverlap(THolder(*first_pass)(EntityId, TDComps1...)
			, void(*second_pass)(THolder&, EntityId, TDComps2...)
			, Tag tag_a
			, Tag tag_b
			, ExecutionNodeId node_id
			, ExecutionNodeIdSet requiried_completed_tasks = {}
			, ThreadGate* optional_notifier = nullptr)
		{
			assert(node_id.IsValid());
			using FB_Const	= Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyConst>;
			using FB_Mut	= Details::FilterBuilder<false, Details::EComponentFilerOptions::OnlyMutable>;

			using TFuncPtr_FP = typename std::add_pointer_t<THolder(EntityId, TDComps1...)>;
			using TFuncPtr_SP = typename std::add_pointer_t<void(THolder&, EntityId, TDComps2...)>;
			AsyncDetails::InnerSyncFunc inner_func = &AsyncDetails::CallGeneric2<TFilterA, TFilterB, THolder, TFuncPtr_FP, TFuncPtr_SP>;
			{
				std::lock_guard<std::mutex> guard(mutex);
				pending_tasks.push_back(AsyncDetails::Task{ inner_func
					, first_pass
					, second_pass
					, AsyncDetails::TaskFilter{FB_Const::Build<TDComps1...>(), FB_Mut::Build<TDComps1...>(), tag_a}
					, AsyncDetails::TaskFilter{FB_Const::Build<TDComps2...>(), FB_Mut::Build<TDComps2...>(), tag_b}
					, requiried_completed_tasks
					, node_id
					, optional_notifier });
			}
			new_task_cv.notify_one();
		}
	};
}