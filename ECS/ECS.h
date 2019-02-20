#pragma once

#include"bitset2\bitset2.hpp"
#include<assert.h>
#include<type_traits>
#include<algorithm>
#include<map>
#include<vector>
#include<tuple>
#include<array>

#define IMPLEMENT_COMPONENT(COMP) COMP::Container ECS::Component<COMP::kComponentTypeIdx, COMP::Container>::__container; \
	template<> void ECS::ComponentBase<COMP::kComponentTypeIdx>::Remove(EntityId id) { COMP::GetContainer().Remove(id); }

namespace ECS
{
	static const constexpr int kMaxComponentTypeNum = 256;
	static const constexpr int kMaxEntityNum = 1024;
	static const constexpr int kActuallyImplementedComponents = 4;

	class ECSManager;

	using TCacheIter = unsigned int;

	struct EntityId
	{
		using TIndex = int16_t;
		TIndex index = -1;

		constexpr bool IsValid() const { return index >= 0 && index < kMaxEntityNum; }

		constexpr EntityId() {}
		constexpr EntityId(TIndex _idx) : index(_idx)
		{
			assert(IsValid());
		}
		constexpr bool operator==(const EntityId& Other) const
		{
			return index == Other.index;
		}
	};

	template<int T> struct ComponentBase
	{
		static const constexpr int kComponentTypeIdx = T; //use  boost::hana::type_c ?
		static_assert(kComponentTypeIdx < kMaxComponentTypeNum, "too many component types");
		static_assert(kComponentTypeIdx < kActuallyImplementedComponents, "not implemented component");
	private:
		friend ECSManager;
		static void Remove(EntityId id);
	};

	template<int T, typename TContainer> struct Component : public ComponentBase<T>
	{
		using Container = TContainer;
		static Container __container;
		static Container& GetContainer() { return __container; }

		void Initialize() {}
		void Reset() {}
	};

	template<bool TUseCachedIter> struct BaseComponentContainer
	{
		constexpr static const bool kUseCachedIter = TUseCachedIter;
	};

	template<typename TComponent> struct DenseComponentContainer : public BaseComponentContainer<false>
	{
	private:
		TComponent components[kMaxEntityNum];

	public:
		TComponent & Add(EntityId id)
		{
			components[id.index].Initialize();
			return components[id.index];
		}

		void Remove(EntityId id)
		{
			components[id.index].Reset();
		}

		TComponent* Get(EntityId id)
		{
			return &components[id.index];
		}

		TComponent& GetChecked(EntityId id)
		{
			return components[id.index];
		}
	};

	template<typename TComponent, bool TUseBinarySearch> struct SortedComponentContainer : public BaseComponentContainer<true>
	{
		static const constexpr bool kUseBinarySearch = TUseBinarySearch;
	private:
		using TPair = std::pair<EntityId::TIndex, TComponent>;
		std::vector<TPair> components;

	public:
		constexpr static bool Less(const TPair& A, const TPair& B)
		{
			return A.first < B.first;
		}

		constexpr auto DesiredPositionSearch(EntityId::TIndex id)
		{
			return std::lower_bound(components.begin(), components.end(), TPair{ id, TComponent{} }, &Less);
		}

		constexpr auto DesiredPositionSearch(EntityId::TIndex id, TCacheIter previous_pos)
		{
			return std::lower_bound(components.begin() + previous_pos, components.end(), TPair{ id, TComponent{} }, &Less);
		}

		constexpr TComponent& Add(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			auto new_it = components.insert(it, { id.index, TComponent{} });
			new_it->second.Initialize();
			return new_it->second;
		}

		void Remove(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			assert(it->first == id.index);
			it->second.Reset();
			components.erase(it);
		}

		TComponent* Get(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			assert(it->first == id.index);
			return (it->first == id.index) ? &it->second : nullptr;
		}

		TComponent* Get(EntityId id, TCacheIter& cached_iter)
		{
			if constexpr(kUseBinarySearch)
			{
				auto it = DesiredPositionSearch(id.index, cached_iter);
				assert((it != components.end()) && (it->first == id.index));
				cached_iter = std::distance(components.begin(), it) + 1;
				return &it->second;
			}
			else
			{
				for (auto it = components.begin() + cached_iter; it != components.end(); it++)
				{
					if (it->first == id.index)
					{
						cached_iter = std::distance(components.begin(), it) + 1;
						return &it->second;
					}
					else if (it->first > id.index)
					{
						break;
					}
				}
				assert(false);
				return nullptr;
			}
		}

		TComponent& GetChecked(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			assert(it->first == id.index);
			return it->second;
		}

		auto& GetCollection() { return components; }
	};

	template<typename TComponent> struct SparseComponentContainer : public BaseComponentContainer<false>
	{
	private:
		std::map<EntityId::TIndex, TComponent> components;

	public:
		TComponent & Add(EntityId id)
		{
			auto iter_pair = components.emplace(id.index, TComponent{});
			iter_pair.first->second.Initialize();
			return iter_pair.first->second;
		}

		void Remove(EntityId id)
		{
			auto iter = components.find(id.index);
			assert(iter != components.end());
			iter->second.Reset();
			components.erase(iter);
		}

		TComponent* Get(EntityId id)
		{
			auto iter = components.find(id.index);
			return (iter != components.end()) ? &(*iter) : nullptr;
		}

		TComponent& GetChecked(EntityId id)
		{
			return components.at(id.index);
		}

		auto& GetCollection() { return components; }
	};

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
				c.set(TComp::kComponentTypeIdx, true);
				return c;
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
					return std::make_tuple<TComp*>(TComp::GetContainer().Get(id));
				}
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

		template<typename TFunc, typename... TComps> void Call(TFunc& Func)
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

		//TESTS
		//PROFILER
		// Const, Exlusivity, Batches, threads
	};
}
