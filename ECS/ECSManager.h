#pragma once

#include "ECSBase.h"
#include <array>
#include <atomic>
#include "malloc.h"

namespace ECS
{
	template<typename... TComps> struct Filter
	{
		constexpr static Details::ComponentCache Get()
		{
			return Details::FilterBuilder<false, Details::EComponentFilerOptions::BothMutableAndConst>::Build<TComps...>();
		}
	};

	class ECSManager
	{
	protected:
		struct Entity
		{
		private:
			Details::ComponentCache components_cache;
			EntityHandle::TGeneration generation = -1;
		public:
			constexpr bool IsEmpty() const { return components_cache.none(); }
			constexpr bool PassFilter(const Details::ComponentCache& filter) const
			{
				return IsSubSetOf(filter, components_cache);
			}
			constexpr bool HasComponent(int ComponentId) const { return components_cache.test(ComponentId); }
			template<typename TComponent> constexpr bool HasComponent() const
			{
				return components_cache.test(TComponent::kComponentTypeIdx);
			}
			constexpr const Details::ComponentCache& GetCache() const { return components_cache; }

			constexpr void Reset() { components_cache.reset(); }
			template<typename TComponent> constexpr void Set(bool value)
			{
				assert(components_cache[TComponent::kComponentTypeIdx] != value);
				components_cache[TComponent::kComponentTypeIdx] = value;
			}

			EntityHandle::TGeneration GetGeneration() const { return generation; }
			EntityHandle::TGeneration NewGeneration() { generation++; return generation;}
		};

		struct EntityContainer
		{
		private:
			Entity entities_space[kMaxEntityNum];
			Bitset2::bitset2<kMaxEntityNum> free_entities;
			int cached_number = 0;
			int actual_max_entity_id = -1;
		public:
			EntityContainer()
			{
				free_entities.set();
			}

			const Entity* Get(EntityId id) const
			{
				return (id.IsValidForm() && !free_entities.test(id)) ? &entities_space[id] : nullptr;
			}

			bool IsHandleValid(EntityHandle handle) const
			{
				return handle.IsValidForm() 
					&& !free_entities.test(handle.id)
					&& (handle.generation == entities_space[handle.id].GetGeneration());
			}

			const Entity* Get(EntityHandle handle) const
			{
				return IsHandleValid(handle) ? &entities_space[handle.id] : nullptr;
			}

			Entity& GetChecked(EntityId id)
			{
				assert(id.IsValidForm() && !free_entities.test(id));
				return entities_space[id];
			}

			EntityHandle Add(unsigned int min_position)
			{
				const auto first_zero_idx_us = (0 == min_position)
					? free_entities.find_first()
					: free_entities.find_next(min_position - 1);
				assert(first_zero_idx_us != Bitset2::bitset2<kMaxEntityNum>::npos);
				const int first_zero_idx = static_cast<int>(first_zero_idx_us);
				if (first_zero_idx >= 0)
				{
					free_entities[first_zero_idx] = false;
					assert(entities_space[first_zero_idx].IsEmpty());
					cached_number++;
					if (first_zero_idx > actual_max_entity_id)
					{
						actual_max_entity_id = first_zero_idx;
					}
					return EntityHandle{ entities_space[first_zero_idx].NewGeneration(),
						static_cast<EntityId::TIndex>(first_zero_idx) };
				}
				return EntityHandle();
			}

			void RemoveChecked(EntityId id)
			{
				cached_number--;
				if (actual_max_entity_id == id)
				{
					int iter = id - 1;
					for (;(iter >= 0) && !free_entities.test(iter); iter--) {}
					actual_max_entity_id = iter;
				}
				entities_space[id].Reset();
				free_entities.set(id, true);
			}

			int GetNumEntities() const { return cached_number; }

			EntityId GetNext(EntityId id, const Details::ComponentCache& pattern) const
			{
				for (EntityId::TIndex it = id + 1; (it < actual_max_entity_id); it++)
				{
					if (!free_entities.test(it))
					{
						if (entities_space[it].PassFilter(pattern))
						{
							return EntityId(it);
						}
					}
				}
				return EntityId();
			}
		};

		EntityContainer entities;
#ifndef NDEBUG
		std::atomic_bool debug_lock = false;
		friend struct DebugLockScope;
#endif

		template<int I> static void RecursiveRemoveComponent(EntityId id, Entity& entity)
		{
			if constexpr(I > 0)
			{
				RecursiveRemoveComponent<I - 1>(id, entity);
			}
			if (entity.HasComponent(Details::ComponentBase<I>::kComponentTypeIdx))
			{
				Details::ComponentBase<I>::Remove(id);
			}
		}

		void RemoveEntityInner(EntityId id)
		{
			RecursiveRemoveComponent<kActuallyImplementedComponents - 1>(id, entities.GetChecked(id));
			entities.RemoveChecked(id);
		}
	public:

		void Reset()
		{
			assert(!debug_lock);
			for (EntityId::TIndex i = 0; i < kMaxEntityNum; i++)
			{
				if (entities.Get(i))
				{
					RemoveEntityInner(i);
				}
			}
		}
		~ECSManager()
		{
			Reset();
		}

		EntityHandle AddEntity(unsigned int MinPosition = 0)
		{
			assert(!debug_lock);
			return entities.Add(MinPosition);
		}
		bool RemoveEntity(EntityHandle entity_handle)
		{
			assert(!debug_lock);
			if (entities.IsHandleValid(entity_handle))
			{
				RemoveEntityInner(entity_handle.id);
				return true;
			}
			return false;
		}
		int GetNumEntities() const
		{
			return entities.GetNumEntities();
		}
		bool IsValidEntity(EntityHandle entity_handle) const
		{
			return nullptr != entities.Get(entity_handle);
		}
		EntityHandle GetHandle(EntityId id) const
		{
			const Entity* ptr = entities.Get(id);
			return ptr ? EntityHandle{ ptr->GetGeneration(), id } : EntityHandle{};
		}

		template<typename TComponent> bool HasComponent(EntityId id) const
		{
			const auto entity = entities.Get(id);
			return entity && entity->HasComponent<TComponent>();
		}
		template<typename TComponent> TComponent& GetComponent(EntityId id)
		{
			static_assert(!TComponent::kIsEmpty, "cannot get an empty component");
			return TComponent::GetContainer().GetChecked(id);
		}
		template<typename TComponent> TComponent& AddComponent(EntityId id)
		{
			assert(!debug_lock);
			static_assert(!TComponent::kIsEmpty, "cannot add an empty component");
			entities.GetChecked(id).Set<TComponent>(true);
			return TComponent::GetContainer().Add(id);
		}
		template<typename TComponent> void AddEmptyComponent(EntityId id)
		{
			assert(!debug_lock);
			static_assert(TComponent::kIsEmpty, "cannot add an empty component");
			entities.GetChecked(id).Set<TComponent>(true);
		}
		template<typename TComponent> void RemoveComponent(EntityId id)
		{
			assert(!debug_lock);
			entities.GetChecked(id).Set<TComponent>(false);
			if constexpr(!TComponent::kIsEmpty)
			{
				TComponent::GetContainer().Remove(id);
			}
		}
		
		template<typename TFilter = typename Filter<>, typename... TDecoratedComps>
		void Call(void(*func)(EntityId, TDecoratedComps...))
		{
			assert(debug_lock);
			using namespace Details;
			using Head = typename Split<TDecoratedComps...>::Head;
			using HeadComponent = typename RemoveDecorators<Head>::type;
			using HeadContainer = typename HeadComponent::Container;
			using IndexOfParam = IndexOfIterParameter<TDecoratedComps...>;
			constexpr auto kArrSize = NumCachedIter<typename RemoveDecorators<TDecoratedComps>::type...>();
			std::array<TCacheIter, kArrSize> cached_iters = { 0 };
			constexpr ComponentCache kFilter = TFilter::Get() | FilterBuilder<true, EComponentFilerOptions::BothMutableAndConst>::Build<TDecoratedComps...>();

			if constexpr (HeadContainer::kUseAsFilter && !std::is_pointer_v<Head>)
			{
				for (auto& it : HeadComponent::GetContainer().GetCollection())
				{
					const EntityId id(it.first);
					const auto& entity = entities.GetChecked(id);
					if (entity.PassFilter(kFilter))
					{
						HeadComponent& head_comp = it.second;
						func(id, Unbox<TDecoratedComps, IndexOfParam::template Get<TDecoratedComps>()>::Get(id, cached_iters, entity.GetCache(), head_comp)...);
					}
				}
			}
			else
			{
				for (EntityId id = entities.GetNext({}, kFilter); id.IsValidForm(); id = entities.GetNext(id, kFilter))
				{
					const auto& entity = entities.GetChecked(id);
					func(id, Unbox<TDecoratedComps, IndexOfParam::template Get<TDecoratedComps>()>::Get(id, cached_iters, entity.GetCache())...);
				}
			}
		}

		template<typename TFilterA = typename Filter<>, typename TFilterB = typename Filter<>, typename THolder, typename... TDComps1, typename... TDComps2>
		void CallOverlap(THolder(*first_pass)(EntityId, TDComps1...), void(*second_pass)(THolder&, EntityId, TDComps2...))
		{
			std::vector<uint8_t> memory(512, 0); 

			using namespace Details;
			auto handle_second_pass = [&](THolder& holder) -> void
			{
				constexpr ComponentCache kFilter = TFilterB::Get() | FilterBuilder<true, EComponentFilerOptions::BothMutableAndConst>::Build<TDComps2...>();
				for (auto iter = holder.GetIter(memory); iter; iter++)
				{
					const EntityId id = *iter;
					const Entity& entity = entities.GetChecked(id);
					if (entity.PassFilter(kFilter))
					{
						second_pass(holder, id, UnboxSimple<TDComps2>::Get(id, entity.GetCache())...);
					}
				}
			};

			assert(debug_lock);
			using Head = typename Split<TDComps1...>::Head;
			using HeadComponent = typename RemoveDecorators<Head>::type;
			using HeadContainer = typename HeadComponent::Container;
			using IndexOfParam = IndexOfIterParameter<TDComps1...>;
			constexpr auto kArrSize = NumCachedIter<typename RemoveDecorators<TDComps1>::type...>();
			std::array<TCacheIter, kArrSize> cached_iters = { 0 };
			constexpr ComponentCache kFilter = TFilterA::Get() | FilterBuilder<true, EComponentFilerOptions::BothMutableAndConst>::Build<TDComps1...>();

			if constexpr (HeadContainer::kUseAsFilter && !std::is_pointer_v<Head>)
			{
				for (auto& it : HeadComponent::GetContainer().GetCollection())
				{
					const EntityId id(it.first);
					const auto& entity = entities.GetChecked(id);
					if (entity.PassFilter(kFilter))
					{
						HeadComponent& head_comp = it.second;
						THolder holder = first_pass(id, Unbox<TDComps1, IndexOfParam::template Get<TDComps1>()>::Get(id, cached_iters, entity.GetCache(), head_comp)...);
						handle_second_pass(holder);
					}
				}
			}
			else
			{
				for (EntityId id = entities.GetNext({}, kFilter); id.IsValidForm(); id = entities.GetNext(id, kFilter))
				{
					const auto& entity = entities.GetChecked(id);
					THolder holder = first_pass(id, Unbox<TDComps1, IndexOfParam::template Get<TDComps1>()>::Get(id, cached_iters, entity.GetCache())...);
					handle_second_pass(holder);
				}
			}
		}
	};

	struct DebugLockScope
	{
#ifdef NDEBUG
		DebugLockScope(ECSManager&) {}
#else
		DebugLockScope(ECSManager& in_ecs)
			: ecs(in_ecs)
		{
			assert(!ecs.debug_lock);
			ecs.debug_lock = true;
		}
		~DebugLockScope()
		{
			assert(ecs.debug_lock);
			ecs.debug_lock = false;
		}
	private:
		ECSManager & ecs;
#endif
	};
}