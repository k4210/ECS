#pragma once

#include "ECSBase.h"
#include <array>
#include <atomic>

namespace ECS
{
	template<typename... TComps> struct Filter
	{
	private:
		constexpr static Details::ComponentCache Get()
		{
			return Details::FilterBuilder<false, Details::EComponentFilerOptions::BothMutableAndConst>::Build<TComps...>();
		}
		friend class ECSManager;
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
				return Bitset2::zip_fold_and(filter, components_cache,
					[](Details::ComponentCache::base_t v1, Details::ComponentCache::base_t v2) noexcept
				{ return (v1 & ~v2) == 0; }); // Any bit unset in v2 must not be set in v1
			}
			constexpr bool HasComponent(int ComponentId) const { return components_cache.test(ComponentId); }
			template<typename TComponent> constexpr bool HasComponent() const
			{
				return components_cache.test(TComponent::kComponentTypeIdx);
			}

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
				std::size_t first_zero_idx = (0 == min_position) 
					? free_entities.find_first()
					: free_entities.find_next(min_position - 1);
				assert(first_zero_idx != Bitset2::bitset2<kMaxEntityNum>::npos);
				if (first_zero_idx >= 0)
				{
					free_entities[first_zero_idx] = false;
					assert(entities_space[first_zero_idx].IsEmpty());
					cached_number++;
					return EntityHandle{ entities_space[first_zero_idx].NewGeneration(),
						static_cast<EntityId::TIndex>(first_zero_idx) };
				}
				return EntityHandle();
			}

			void RemoveChecked(EntityId id)
			{
				cached_number--;
				entities_space[id].Reset();
				free_entities.set(id, true);
			}

			int GetNumEntities() const { return cached_number; }

			EntityId GetNext(EntityId id, const Details::ComponentCache& pattern, int& already_tested) const
			{
				for (EntityId::TIndex it = id + 1; (it < kMaxEntityNum) && (already_tested < cached_number); it++)
				{
					if (!free_entities.test(it))
					{
						already_tested++;
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

		template<typename TFilter, typename... TDecoratedComps>
		void CallNoHint(void(*func)(EntityId, TDecoratedComps...))
		{
			constexpr auto kArrSize = Details::NumCachedIter<typename Details::RemoveDecorators<TDecoratedComps>::type...>();
			std::array<Details::TCacheIter, kArrSize> cached_iters; cached_iters.fill(0);

			constexpr Details::ComponentCache filter = TFilter::Get()
				| Details::FilterBuilder<true, Details::EComponentFilerOptions::BothMutableAndConst>::Build<TDecoratedComps...>();
			int already_tested = 0;

			for (EntityId id = entities.GetNext({}, filter, already_tested); id.IsValidForm()
				; id = entities.GetNext(id, filter, already_tested))
			{
				using IndexOfParam = Details::IndexOfIterParameter<TDecoratedComps...>;
				func(id, Details::Unbox<TDecoratedComps, IndexOfParam::template Get<TDecoratedComps>()>::Get(id, cached_iters, filter)...);
			}
		}

		template<typename TFilter, typename TComp, typename... TDecoratedComps>
		void CallHint(void(*func)(EntityId, TDecoratedComps...))
		{
			constexpr auto kArrSize = Details::NumCachedIter<typename Details::RemoveDecorators<TDecoratedComps>::type...>();
			std::array<Details::TCacheIter, kArrSize> cached_iters = { 0 };

			constexpr Details::ComponentCache filter = TFilter::Get()
				| Details::FilterBuilder<true, Details::EComponentFilerOptions::BothMutableAndConst>::Build<TDecoratedComps...>();
			for (auto& it : Details::RemoveDecorators<TComp>::type::GetContainer().GetCollection())
			{
				const EntityId id(it.first);
				if (entities.GetChecked(id).PassFilter(filter))
				{
					using IndexOfParam = Details::IndexOfIterParameter<TDecoratedComps...>;
					func(id, it.second, Details::Unbox<TDecoratedComps, IndexOfParam::template Get<TDecoratedComps>()>::Get(id, cached_iters, filter)...);
				}
			}
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
			using Head = typename Details::Split<TDecoratedComps...>::Head;
			using HeadContainer = typename Details::RemoveDecorators<Head>::type::Container;
			if constexpr (HeadContainer::kUseAsFilter && !std::is_pointer_v<Head>)
			{
				CallHint<TFilter, TDecoratedComps...>(func);
			}
			else
			{
				CallNoHint<TFilter, TDecoratedComps...>(func);
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