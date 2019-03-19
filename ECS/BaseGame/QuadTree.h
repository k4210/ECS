#pragma once
#include "malloc.h"
#include <array>

template<typename Element, uint32_t kMaxElementsPerLeaf = 16, uint32_t kResolutionX = 64, uint32_t kResolutionY = 64>
struct QuadTree
{
	struct Leaf
	{
		std::array <Element, kMaxElementsPerLeaf> data = { Element{} };
	};

	static_assert(alignof(Element) == alignof(Leaf), "");

	Leaf entities[kResolutionX][kResolutionX] = {};

	struct Region
	{
		uint8_t min_x = 0xFF;
		uint8_t min_y = 0xFF;
		uint8_t max_x = 0xFF;
		uint8_t max_y = 0xFF;

		uint32_t	SizeX()		const { return max_x - min_x; }
		uint32_t	SizeY()		const { return max_y - min_y; }
		uint32_t	Area()		const { return SizeX() * SizeY(); }
		bool		IsValid()	const {
			return (min_x < kResolutionX) && (max_x <= kResolutionX) && (max_x > min_x)
				&& (min_y < kResolutionY) && (max_y <= kResolutionY) && (max_y > min_y);
		}

		uint32_t Index(uint8_t x, uint8_t y) const
		{
			assert(x >= min_x);
			assert(x < max_x);
			assert(y >= min_y);
			assert(y < max_y);
			return (x - min_x) * SizeY() * (y - min_y);
		}
	};

	template<typename TFunc, typename... Args>
	void ForEveryLeafInRegion(const Region region, TFunc func, Args... args)
	{
		for (uint32_t x = region.min_x; x < region.max_x; x++)
		{
			for (uint32_t y = region.min_y; y < region.max_y; y++)
			{
				func(entities[x][y], std::forward<Args>(args)...);
			}
		}
	}

	template<typename TFunc, typename... Args>
	void ForEveryLeafInRegion(const Region region, TFunc func, Args... args) const
	{
		for (uint32_t x = region.min_x; x < region.max_x; x++)
		{
			for (uint32_t y = region.min_y; y < region.max_y; y++)
			{
				func(entities[x][y], std::forward<Args>(args)...);
			}
		}
	}

	void Reset()
	{
		ForEveryLeafInRegion(Region{ 0, 0, kResolutionX , kResolutionY }, [](Leaf& leaf) { leaf = Leaf{}; });
	}

	static_assert(std::is_trivially_copyable_v<Element>);

	void Add(const Element id, const Region region)
	{
		ForEveryLeafInRegion(region, [](Leaf& leaf, const Element id)
		{
			assert(!::IsValid(leaf.data[kMaxElementsPerLeaf - 1]));
			int64_t idx_insert = 0;
			for (; idx_insert < kMaxElementsPerLeaf; idx_insert++)
			{
				auto& it_id = leaf.data[idx_insert];
				if (!::IsValid(it_id))
				{
					it_id = id;
					return;
				}
				if (it_id == id)
					return;
				if (it_id > id)
					break;
			}
			if (idx_insert >= kMaxElementsPerLeaf)
			{
				assert(false);
				return;
			}
			int64_t idx_first_invalid = idx_insert + 1;
			for (; (idx_first_invalid < kMaxElementsPerLeaf) && ::IsValid(leaf.data[idx_first_invalid]); idx_first_invalid++) {}
			assert(idx_first_invalid < kMaxElementsPerLeaf);
			memmove(&leaf.data[idx_insert + 1], &leaf.data[idx_insert], sizeof(Element) * (idx_first_invalid - idx_insert));
			leaf.data[idx_insert] = id;
		}, id);
	}

	void Remove(Element id, Region region)
	{
		ForEveryLeafInRegion(region, [](Leaf& leaf, const Element id)
		{
			int64_t idx = 0;
			for (; idx < kMaxElementsPerLeaf; idx++)
			{
				auto& it_id = leaf.data[idx];
				if (!::IsValid(it_id) || id < it_id)
				{
					assert(false);
					return;
				}
				if (id == it_id)
					break;
			}
			if (idx >= kMaxElementsPerLeaf)
			{
				assert(false);
				return;
			}
			int64_t idx2 = idx + 1;
			for (; (idx2 < kMaxElementsPerLeaf) && ::IsValid(leaf.data[idx2]); idx2++) {}
			idx2--;
			assert(idx2 < kMaxElementsPerLeaf);
			if (idx2 > idx)
			{
				memmove_s(&leaf.data[idx], sizeof(Element) * (kMaxElementsPerLeaf - idx), &leaf.data[idx + 1], sizeof(Element) * (idx2 - idx));
			}
			leaf.data[idx2] = {};
		}, id);
	}

	struct Iter
	{
	private:
		std::vector<uint8_t>& memory;
		uint32_t count = 0;
		uint32_t it = 0;

		Element& Get(uint32_t idx)
		{
			assert(memory.size() >= idx * sizeof(Element));
			return reinterpret_cast<Element*>(memory.data())[idx];
		};

		const Element& Get(uint32_t idx) const
		{
			assert(memory.size() >= idx * sizeof(Element));
			return reinterpret_cast<Element*>(memory.data())[idx];
		};

	public:
		Iter(const Element lowed_bound, const Region region, const QuadTree& qt, std::vector<uint8_t>& in_memory)
			: memory(in_memory)
		{
			ECS::ScopeDurationLog __sdl(EStatId::QuadTreeIteratorConstrucion, EPredefinedStatGroups::Framework);

			const uint32_t max_elements_num = region.Area() * kMaxElementsPerLeaf;
			memory.resize(max_elements_num * sizeof(Element));

			const std::size_t leaf_iterators_mam_size = region.Area() * sizeof(uint32_t*);
			uint32_t* leaf_iterators = reinterpret_cast<uint32_t*>(_malloca(leaf_iterators_mam_size));
			assert(leaf_iterators);
			memset(leaf_iterators, 0, leaf_iterators_mam_size);

			Element previous_id{};
			uint32_t filled_it = 0;
			for (; filled_it < max_elements_num; filled_it++)
			{
				const Element* min_id = nullptr;
				uint32_t* min_id_iter = nullptr;
				for (uint8_t x = region.min_x; x < region.max_x; x++)
				{
					for (uint8_t y = region.min_y; y < region.max_y; y++)
					{
						const Leaf& leaf = qt.entities[x][y];

						uint32_t& local_iter = leaf_iterators[region.Index(x, y)];
						if (local_iter >= kMaxElementsPerLeaf) continue;

						const Element* local_id = &leaf.data[local_iter];
						if (!::IsValid(*local_id)) continue;

						if ((*local_id < lowed_bound) || (*local_id == lowed_bound) || (*local_id == previous_id))
						{
							local_iter++;
							if (local_iter >= kMaxElementsPerLeaf) continue;
							local_id = &leaf.data[local_iter];
							if (!::IsValid(*local_id)) continue;
						}
						assert(previous_id < *local_id);
						if ((nullptr == min_id) || (*local_id < *min_id))
						{
							min_id = local_id;
							min_id_iter = &local_iter;
						}
					}
				}
				if (!min_id) break;
				previous_id = *min_id;
				Get(filled_it) = *min_id;
				assert(min_id_iter);
				(*min_id_iter)++;
			}
			count = filled_it;

			_freea(leaf_iterators);
		}

		bool					IsValid()		const { return (it < count) && ::IsValid(Get(it)); }
		operator bool()	const { return IsValid(); }
		void					operator++() { if (IsValid()) it++; }
		void					operator++(int) { operator++(); }
		const Element&	operator*()		const { assert(IsValid()); return Get(it); }
	};
};