#pragma once

#include<bitset>
#include<assert.h>
#include<type_traits>
#include<algorithm>
#include<map>
#include<vector>
#include<tuple>

static const constexpr int kMaxComponentTypeNum = 256;
static const constexpr int kMaxEntityNum = 1024;

struct EntityId
{
	static const constexpr int kInvalidValue = -1;
	int index = kInvalidValue;
	//TODO: generation?

	constexpr bool IsValid() const { return index >= 0 && index < kMaxEntityNum; }

	EntityId() {}
	EntityId(int _idx) : index(_idx)
	{
		assert(IsValid());
	}
};

template<int T> struct ComponentBase
{
	static const constexpr int kComponentTypeIdx = T; //use  boost::hana::type_c ?
	static_assert(kComponentTypeIdx < kMaxComponentTypeNum, "too many component types");

	static void Remove(EntityId id);

	void Initialize() {}
	void Reset() {}

	//Child must implement: 		
	// void Initialize()
	// void Reset()
	// using Container = ...
	// static Container& GetContainer();
};

template<typename TComponent> struct DenseComponentContainer
{
private:
	TComponent components[kMaxEntityNum];

public:
	TComponent& Add(EntityId id)
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

template<typename TComponent> struct SortedComponentContainer
{
private:
	using TPair = std::pair<int, TComponent>;
	std::vector<TPair> components;

public:
	static bool Less(const TPair& A, const TPair& B)
	{
		return A.first < B.first;
	}

	auto DesiredPosition(int id)
	{
		return std::lower_bound(components.begin(), components.end(), TPair{ id, TComponent{} }, &Less);
	}

	TComponent& Add(EntityId id)
	{
		auto it = DesiredPosition(id.index);
		auto new_it = components.insert(it, { id.index, TComponent{} });
		new_it->second.Initialize();
		return new_it->second;
	}

	void Remove(EntityId id)
	{
		auto it = DesiredPosition(id.index);
		assert(it->first == id.index);
		it->second.Reset();
		components.erase(it);
	}

	TComponent* Get(EntityId id)
	{
		auto it = DesiredPosition(id.index);
		return (it->first == id.index) ? &it->second : nullptr;
	}

	TComponent& GetChecked(EntityId id)
	{
		auto it = DesiredPosition(id.index);
		assert(it->first == id.index);
		return it->second;
	}

	auto& GetCollection() { return components; }
};

template<typename TComponent> struct SparseComponentContainer
{
private:
	std::map<int, TComponent> components;

public:
	TComponent& Add(EntityId id)
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

	auto& GetCollection() const { return components; }
};

struct ECSManager
{
	using ComponentCache = std::bitset<kMaxComponentTypeNum>;

private:
	struct Entity
	{
	private:
		ComponentCache components_cache;

	public:
		bool IsEmpty() const { return components_cache.none(); }
		bool PassFilter(const ComponentCache& pattern) const { return (pattern & (pattern ^ components_cache)).none(); }
		bool HasComponent(int ComponentId) const { return components_cache.test(ComponentId); }
		template<typename TComponent> bool HasComponent() const
		{
			return components_cache.test(TComponent::kComponentTypeIdx);
		}

		void Reset() { components_cache.reset(); }
		template<typename TComponent> void Set(bool value)
		{
			assert(components_cache[TComponent::kComponentTypeIdx] != value);
			components_cache[TComponent::kComponentTypeIdx] = value;
		}
	};

	struct EntityContainer
	{
	private:
		Entity entities_space[kMaxEntityNum];
		std::bitset<kMaxEntityNum> used_entities;

	public:
		Entity * Get(EntityId Id)
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
				entities_space[id.index].Reset();
				used_entities.set(id.index, false);
			}
		}

		EntityId GetNext(EntityId id, const ComponentCache& pattern) const
		{
			for (int it = id.index + 1; it < kMaxEntityNum; it++)
			{
				if (used_entities.test(it) && entities_space[it].PassFilter(pattern))
				{
					return EntityId(it);
				}
			}
			return EntityId();
		}
	};

private:
	EntityContainer entities;

public:
	EntityId AddEntity() { return entities.Add(); }
	void RemoveEntity(EntityId id) 
	{  
		Entity& entity = entities.GetChecked(id);

		if (entity.HasComponent(0)) { ComponentBase<0>::Remove(id); }
		if (entity.HasComponent(1)) { ComponentBase<1>::Remove(id); }
		if (entity.HasComponent(2)) { ComponentBase<2>::Remove(id); }

		entities.Remove(id);
	}

public:
	template<typename TComponent> TComponent& GetComponent(EntityId id) 
	{ 
		return TComponent::GetContainer().GetChecked(id); 
	}
	template<typename TComponent> TComponent* GetComponentPtr(EntityId id)
	{
		return TComponent::GetContainer().Get(id);
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

private:
	template<typename T, typename... Args> struct FilterBuilder
	{
		static ComponentCache Build()
		{
			return FilterBuilder<T>::Build() | FilterBuilder<Args...>::Build();
		}
	};

	template<typename T> struct FilterBuilder<T>
	{
		static ComponentCache Build()
		{
			ComponentCache c;
			c.set(T::kComponentTypeIdx, true);
			return c;
		}
	};

public:
	template<typename... Args>
	static ComponentCache BuildCacheFilter() { return FilterBuilder<Args...>::Build(); }

private:

	template<typename T, typename... Args> struct TupleBuilder
	{
		inline static auto Build(EntityId id)
		{
			return std::tuple_cat(TupleBuilder<T>::Build(id), TupleBuilder<Args...>::Build(id));
		}
	};

	template<typename T> struct TupleBuilder<T>
	{
		inline static auto Build(EntityId id)
		{
			return std::make_tuple<T*>(T::GetContainer().Get(id));
		}
	};

public:

	template<typename TFunc, typename... TComps>
	void Call(TFunc& Func)
	{
		const ComponentCache filter = BuildCacheFilter<TComps...>();
		for (EntityId id = entities.GetNext({}, filter); id.IsValid(); id = entities.GetNext(id, filter))
		{
			std::apply(Func, TupleBuilder<TComps...>::Build(id));
		}
	}

	template<typename THint, typename TFunc, typename... TComps>
	void CallHint(TFunc& Func)
	{
		const ComponentCache filter = BuildCacheFilter<THint, TComps...>();
		for (auto& it : THint::GetContainer().GetCollection())
		{
			const EntityId id(it.first);
			if (entities.GetChecked(id).PassFilter(filter))
			{
				std::apply(Func, std::tuple_cat(std::make_tuple(&it.second), TupleBuilder<TComps...>::Build(id)));
			}
		}
	}

	template<typename TComp, typename TFunc>
	void CallHintSingle(TFunc& Func)
	{
		for (auto& it : TComp::GetContainer().GetCollection())
		{
			Func(&it.second);
		}
	}

	// Iterators ?
	// Const, Exlusivity, Batches, threads

	//TESTS
	//PROFILER
};

