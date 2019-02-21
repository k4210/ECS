#pragma once

#include "ECSBase.h"
#include "bitset2\bitset2.hpp"

namespace ECS
{
	class ECSManager
	{
		using ComponentCache = Bitset2::bitset2<kMaxComponentTypeNum>;

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

		template<typename TCompHead, typename... TCompsTail> struct FilterBuilder
		{
			constexpr static ComponentCache Build()
			{
				return FilterBuilder<TCompHead>::Build() | FilterBuilder<TCompsTail...>::Build();
			}
		};

		template<typename TComp> struct FilterBuilder<TComp>
		{
			constexpr static ComponentCache Build()
			{
				ComponentCache c;
				c.set(RemoveDecorators<TComp>::type::kComponentTypeIdx, true);
				return c;
			}
		};

		template<typename TComp> struct FilterBuilder<TComp*>
		{
			constexpr static ComponentCache Build()
			{
				return ComponentCache{};
			}
		};

		template<typename... TComps> constexpr static ComponentCache BuildCacheFilter()
		{
			return FilterBuilder<TComps...>::Build();
		}

		template<typename... TComps> static constexpr int NumCachedIter()
		{
			return ((TComps::Container::kUseCachedIter ? 1 : 0) + ...);
		}

		template<typename TCompHead, typename... TCompsTail> struct ComponentsTupleBuilder
		{
			template<typename TArr, int IterIdx = NumCachedIter<TCompsTail...>()>
			static auto Build(EntityId id, TArr& cached_iters)
			{
				return std::tuple_cat(ComponentsTupleBuilder<TCompHead>::Build<TArr, IterIdx>(id, cached_iters)
					, ComponentsTupleBuilder<TCompsTail...>::Build<TArr>(id, cached_iters));
			}
		};

		template<typename TComp> struct ComponentsTupleBuilder<TComp>
		{
			template<typename TArr, int IterIdx = 0>
			static auto Build(EntityId id, TArr& cached_iters)
			{
				if constexpr(TComp::Container::kUseCachedIter)
				{
					return std::make_tuple<TComp*>(TComp::GetContainer().Get(id, cached_iters[IterIdx]));
				}
				else
				{
					return std::make_tuple<TComp*>(TComp::GetContainer().GetOptional(id));
				}
			}
		};

		template<class TComp>
		struct Unbox {};

		template<class TComp>
		struct Unbox<TComp&>
		{
			static TComp& Get(EntityId id)
			{
				return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id);
			}
		};

		template<class TComp>
		struct Unbox<const TComp&>
		{
			static const TComp& Get(EntityId id)
			{
				return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id);
			}
		};

		template<class TComp>
		struct Unbox<TComp*>
		{
			static TComp* Get(EntityId id)
			{
				return RemoveDecorators<TComp>::type::GetContainer().GetOptional(id);
			}
		};

		template<class TComp>
		struct Unbox<const TComp*>
		{
			static const TComp* Get(EntityId id)
			{
				return RemoveDecorators<TComp>::type::GetContainer().GetOptional(id);
			}
		};

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
			std::array<TCacheIter, NumCachedIter<typename RemoveDecorators<TDecoratedComps>::type...>()> cached_iters;
			cached_iters.fill(0);

			const ComponentCache filter = BuildCacheFilter<TDecoratedComps...>();
			int already_tested = 0;
			for (EntityId id = entities.GetNext({}, filter, already_tested); id.IsValid()
				; id = entities.GetNext(id, filter, already_tested))
			{
				Func(id, Unbox<TDecoratedComps>::Get(id)...);
			}
		}
		/*
		template<typename... TComps> void Call(std::function<void(TComps*...)> Func)
		{
		std::array<TCacheIter, NumCachedIter<TComps...>()> cached_iters;
		cached_iters.fill(0);

		const ComponentCache filter = BuildCacheFilter<TComps...>();
		int already_tested = 0;
		for (EntityId id = entities.GetNext({}, filter, already_tested); id.IsValid(); id = entities.GetNext(id, filter, already_tested))
		{
		std::apply(Func, ComponentsTupleBuilder<TComps...>::Build(id, cached_iters));
		}
		}

		template<typename... TComps> void Call(std::function<void(EntityId, TComps*...)> Func)
		{
		std::array<TCacheIter, NumCachedIter<TComps...>()> cached_iters;
		cached_iters.fill(0);

		const ComponentCache filter = BuildCacheFilter<TComps...>();
		int already_tested = 0;
		for (EntityId id = entities.GetNext({}, filter, already_tested); id.IsValid(); id = entities.GetNext(id, filter, already_tested))
		{
		std::apply(Func, std::tuple_cat(std::make_tuple(id), ComponentsTupleBuilder<TComps...>::Build(id, cached_iters)));
		}
		}

		template<typename THint, typename TFunc, typename... TComps> void CallHint(TFunc& Func)
		{
		std::array<TCacheIter, NumCachedIter<TComps...>()> cached_iters;
		cached_iters.fill(0);

		const ComponentCache filter = BuildCacheFilter<THint, TComps...>();
		for (auto& it : THint::GetContainer().GetCollection())
		{
		const EntityId id(it.first);
		if (entities.GetChecked(id).PassFilter(filter))
		{
		std::apply(Func, std::tuple_cat(std::make_tuple(&it.second), ComponentsTupleBuilder<TComps...>::Build(id, cached_iters)));
		}
		}
		}
		template<typename TComp, typename TFunc> void CallHintSingle(TFunc& Func)
		{
		for (auto& it : TComp::GetContainer().GetCollection())
		{
		Func(&it.second);
		}
		}
		*/
		// cached iter
		// hint

		//TESTS
		//PROFILER
		// Const, Exlusivity, Batches, threads

		//Optimization: Put similar entities next to eaxh other
	};
}