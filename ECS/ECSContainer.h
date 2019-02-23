#pragma once

#include "ECSBase.h"

namespace ECS
{
	template<typename TComponent> struct DenseComponentContainer : public BaseComponentContainer<false, false>
	{
	private:
		TComponent components[kMaxEntityNum];

	public:
		TComponent & Add(EntityId id)
		{
			components[id.index].Initialize();
			return components[id.index];
		}

		void Remove(EntityId id) { components[id.index].Reset(); }

		TComponent* GetOptional(EntityId id) { return &components[id.index]; }
		TComponent& GetChecked(EntityId id) { return components[id.index]; }
	};

	template<typename TComponent, bool TUseBinarySearch> struct SortedComponentContainer : public BaseComponentContainer<true, true>
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

		TComponent* Get(EntityId id, TCacheIter& cached_iter)
		{
			if constexpr(kUseBinarySearch)
			{
				auto it = DesiredPositionSearch(id.index, cached_iter);
				const bool bOk = (it != components.end()) && (it->first == id.index);
				cached_iter = std::distance(components.begin(), it) + (bOk ? 1 : 0);
				return bOk ? &it->second : nullptr;
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
						cached_iter = std::distance(components.begin(), it);
						break;
					}
				}
				return nullptr;
			}
		}

		TComponent* GetOptional(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			return (it->first == id.index) ? &it->second : nullptr;
		}

		TComponent* GetOptional(EntityId id, TCacheIter& cached_iter)
		{
			return Get(id, cached_iter);
		}

		TComponent& GetChecked(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			assert(it->first == id.index);
			return it->second;
		}

		TComponent& GetChecked(EntityId id, TCacheIter& cached_iter)
		{
			auto ptr = Get(id, cached_iter);
			assert(ptr);
			return *ptr;
		}

		auto& GetCollection() { return components; }
	};

	template<typename TComponent> struct SparseComponentContainer : public BaseComponentContainer<false, true>
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

		TComponent* GetOptional(EntityId id)
		{
			auto iter = components.find(id.index);
			return (iter != components.end()) ? &(iter->second) : nullptr;
		}

		TComponent& GetChecked(EntityId id) { return components.at(id.index); }

		auto& GetCollection() { return components; }
	};

}