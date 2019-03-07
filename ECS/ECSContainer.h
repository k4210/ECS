#pragma once

#include "ECSBase.h"
#include<map>
#include<vector>
#include<algorithm>
//#include<deque>

namespace ECS
{
	template<typename TComponent> struct DenseComponentContainer : public Details::BaseComponentContainer<false, false>
	{
	private:
		TComponent components[kMaxEntityNum];

	public:
		TComponent & Add(EntityId id)
		{
			components[id].Initialize();
			return components[id];
		}

		void Remove(EntityId id) { components[id].Reset(); }

		TComponent& GetChecked(EntityId id) { return components[id]; }
	};

	template<typename TComponent, bool TUseBinarySearch> struct SortedComponentContainer : public Details::BaseComponentContainer<true, true>
	{
		static const constexpr bool kUseBinarySearch = TUseBinarySearch;

	private:
		using TPair = std::pair<EntityId::TIndex, TComponent>;
		std::vector<TPair> components;
		//std::deque<TPair> components;

		constexpr static bool Less(const TPair& A, const TPair& B)
		{
			return A.first < B.first;
		}

		constexpr auto DesiredPositionSearch(EntityId::TIndex id, Details::TCacheIter previous_pos = 0)
		{
			return std::lower_bound(components.begin() + previous_pos, components.end(), TPair{ id, TComponent{} }, &Less);
		}

	public:
		SortedComponentContainer()
		{
			components.reserve(TComponent::kInitialReserve);
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
			auto it = DesiredPositionSearch(id);
			assert(it->first == id);
			it->second.Reset();
			components.erase(it);
		}

		TComponent& GetChecked(EntityId id)
		{
			auto it = DesiredPositionSearch(id.index);
			assert((it != components.end()) && (it->first == id.index));
			return it->second;
		}

		TComponent& GetChecked(EntityId id, Details::TCacheIter& cached_iter)
		{
			auto it = [&]() 
			{
				if constexpr(kUseBinarySearch)
				{
					return DesiredPositionSearch(id.index, cached_iter);
				}
				else
				{
					auto it = components.begin() + cached_iter;
					for (;it != components.end(); it++)
					{
						if (it->first == id.index)
						{
							break;
						}
						assert(it->first < id.index);
					}
					return it;
				}
			}();

			assert((it != components.end()) && (it->first == id.index));
			cached_iter = static_cast<Details::TCacheIter>(std::distance(components.begin(), it) + 1);
			return it->second;
		}

		auto& GetCollection() { return components; }
	};

	template<typename TComponent> struct SparseComponentContainer : public Details::BaseComponentContainer<false, true>
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

		TComponent& GetChecked(EntityId id) { return components.at(id.index); }

		auto& GetCollection() { return components; }
	};

}