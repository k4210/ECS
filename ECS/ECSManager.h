#pragma once

#include "ECSBase.h"

namespace ECS
{
	class ECSManager
	{
		struct Entity
		{
		private:
			ComponentCache components_cache;

		public:
			constexpr bool IsEmpty() const { return components_cache.none(); }
			constexpr bool PassFilter(const ComponentCache& filter) const
			{
				return Bitset2::zip_fold_and(filter, components_cache,
					[](ComponentCache::base_t v1, ComponentCache::base_t v2) noexcept
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
		};

		struct EntityContainer
		{
		private:
			Entity entities_space[kMaxEntityNum];
			Bitset2::bitset2<kMaxEntityNum> used_entities;
			int cached_number = 0;
		public:
			const Entity* Get(EntityId Id) const
			{
				return (Id.IsValid() && used_entities.test(Id.index)) ? &entities_space[Id.index] : nullptr;
			}

			Entity& GetChecked(EntityId id)
			{
				assert(id.IsValid() && used_entities.test(id.index));
				return entities_space[id.index];
			}

			EntityId Add()
			{
				int first_zero_idx = -1;
				for (std::size_t i = 0; i < used_entities.size(); ++i)
				{
					if (!used_entities.test(i))
					{
						first_zero_idx = i;
						break;
					}
				}
				assert(first_zero_idx >= 0);
				if (first_zero_idx >= 0)
				{
					used_entities[first_zero_idx] = 1;
					assert(entities_space[first_zero_idx].IsEmpty());
					cached_number++;
					return EntityId(first_zero_idx);
				}
				return EntityId();
			}

			void Remove(EntityId id)
			{
				const bool bProperId = id.IsValid() && used_entities.test(id.index);
				assert(bProperId);
				if (bProperId)
				{
					cached_number--;
					entities_space[id.index].Reset();
					used_entities.set(id.index, false);
				}
			}

			int GetNumEntities() const { return cached_number; }

			EntityId GetNext(EntityId id, const ComponentCache& pattern, int& already_tested) const
			{
				for (int it = id.index + 1; (it < kMaxEntityNum) && (already_tested < cached_number); it++)
				{
					already_tested++;
					if (used_entities.test(it) && entities_space[it].PassFilter(pattern))
					{
						return EntityId(it);
					}
				}
				return EntityId();
			}
		};

		EntityContainer entities;

		template<int I> static void RecursiveRemoveComponent(EntityId id, Entity& entity)
		{
			if constexpr(I > 0)
			{
				RecursiveRemoveComponent<I - 1>(id, entity);
			}
			if (entity.HasComponent(ComponentBase<I>::kComponentTypeIdx))
			{
				ComponentBase<I>::Remove(id);
			}
		}

		template<typename... TDecoratedComps> void CallNoHint(std::function<void(EntityId, TDecoratedComps...)> Func)
		{
			constexpr auto kArrSize = Details::NumCachedIter<typename Details::RemoveDecorators<TDecoratedComps>::type...>();
			std::array<TCacheIter, kArrSize> cached_iters; cached_iters.fill(0);

			const ComponentCache filter = Details::BuildCacheFilter<TDecoratedComps...>();
			int already_tested = 0;

			for (EntityId id = entities.GetNext({}, filter, already_tested); id.IsValid()
				; id = entities.GetNext(id, filter, already_tested))
			{
				using IndexOfParam = Details::IndexOfIterParameter<TDecoratedComps...>;
				Func(id, Details::Unbox<TDecoratedComps, IndexOfParam>::Get(id, cached_iters)...);
			}
		}

		template<typename TComp, typename... TDecoratedComps> void CallHint(std::function<void(EntityId, TComp, TDecoratedComps...)> Func)
		{
			constexpr auto kArrSize = Details::NumCachedIter<typename Details::RemoveDecorators<TDecoratedComps>::type...>();
			std::array<TCacheIter, kArrSize> cached_iters; cached_iters.fill(0);

			const ComponentCache filter = Details::BuildCacheFilter<TComp, TDecoratedComps...>();
			for (auto& it : Details::RemoveDecorators<TComp>::type::GetContainer().GetCollection())
			{
				const EntityId id(it.first);
				if (entities.GetChecked(id).PassFilter(filter))
				{
					using IndexOfParam = Details::IndexOfIterParameter<TDecoratedComps...>;
					Func(id, it.second, Details::Unbox<TDecoratedComps, IndexOfParam>::Get(id, cached_iters)...);
				}
			}
		}

	public:
		void Reset()
		{
			for (EntityId::TIndex i = 0; i < kMaxEntityNum; i++)
			{
				if (entities.Get(i))
				{
					RemoveEntity(i);
				}
			}
		}
		~ECSManager()
		{
			Reset();
		}

		EntityId AddEntity()
		{
			return entities.Add();
		}
		void RemoveEntity(EntityId id)
		{
			RecursiveRemoveComponent<kActuallyImplementedComponents - 1>(id, entities.GetChecked(id));
			entities.Remove(id);
		}
		int GetNumEntities() const
		{
			return entities.GetNumEntities();
		}
		bool IsValidEntity(EntityId id) const
		{
			return nullptr != entities.Get(id);
		}

		template<typename TComponent> bool HasComponent(EntityId id) const
		{
			const auto entity = entities.Get(id);
			return entity && entity->HasComponent<TComponent>();
		}
		template<typename TComponent> TComponent& GetComponent(EntityId id)
		{
			return TComponent::GetContainer().GetChecked(id);
		}
		template<typename TComponent> TComponent& AddComponent(EntityId id)
		{
			entities.GetChecked(id).Set<TComponent>(true);
			return TComponent::GetContainer().Add(id);
		}
		template<typename TComponent> void RemoveComponent(EntityId id)
		{
			entities.GetChecked(id).Set<TComponent>(false);
			TComponent::GetContainer().Remove(id);
		}

		template<typename... TDecoratedComps> void Call(std::function<void(EntityId, TDecoratedComps...)> Func)
		{
			using Head = typename Details::Split<TDecoratedComps...>::Head;
			using HeadContainer = typename Details::RemoveDecorators<Head>::type::Container;
			if constexpr(HeadContainer::kUseAsFilter && !std::is_pointer_v<Head>)
			{
				CallHint<TDecoratedComps...>(Func);
			}
			else
			{
				CallNoHint<TDecoratedComps...>(Func);
			}
		}

		// empty component
		// perfect forwarding

		// PROFILER
		// Const, Exlusivity, Batches, threads

		// Every bug found -> add a test

		//Optimization: Put similar entities next to eaxh other
	};
}