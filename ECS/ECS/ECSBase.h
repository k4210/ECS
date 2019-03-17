#pragma once

#include<assert.h>
#include<type_traits>
#include<functional>
#include<chrono>
#include "bitset2\bitset2.hpp"

#define IMPLEMENT_COMPONENT(COMP) COMP::Container ECS::Component<COMP::kComponentTypeIdx, COMP::Container>::__container; \
	template<> void ECS::Details::ComponentBase<COMP::kComponentTypeIdx>::Remove(EntityId id) { COMP::GetContainer().Remove(id); } \

#define IMPLEMENT_EMPTY_COMPONENT(COMP) template<> void ECS::Details::ComponentBase<COMP::kComponentTypeIdx>::Remove(EntityId) { }

namespace ECS
{
	// >>CONFIG
	static const constexpr uint32_t kMaxEntityNum = 1024;
	static const constexpr uint32_t kActuallyImplementedComponents = 12;
	static const constexpr uint32_t kMaxConcurrentWorkerThreads = 2;
	static const constexpr uint32_t kMaxExecutionNode = 64;
	static const constexpr uint32_t kMaxTagsNum = 8;
	// <<CONFIG

	static const constexpr uint32_t kMaxComponentTypeNum = kActuallyImplementedComponents;
	static_assert(kActuallyImplementedComponents <= kMaxComponentTypeNum, "too many component types");

	struct Tag
	{
		using TagId = uint8_t;
		constexpr const static uint8_t kNoTagValue = UINT8_MAX;
	private:
		TagId id = kNoTagValue;

	public:
		constexpr Tag() = default;
		template<typename T> Tag(T v) : id(static_cast<TagId>(v))
		{
			assert(static_cast<uint32_t>(v) < kMaxTagsNum);
			assert(id != kNoTagValue);
		}

		constexpr static bool Match(const Tag A, const Tag B)
		{
			return (A.id == B.id) || (A.id == kNoTagValue) || (B.id == kNoTagValue);
		}

		constexpr TagId Index() const { return id; }
		constexpr bool HasValidValue() const
		{
			assert((id < kMaxTagsNum) || (id != kNoTagValue));
			return id != kNoTagValue;
		}
	};

	struct EntityId
	{
		using TIndex = uint16_t;
	private:
		constexpr static const TIndex kInvalidValue = UINT16_MAX;
		TIndex index = kInvalidValue;

		template<typename T>
		constexpr EntityId(T _idx) 
			: index(static_cast<EntityId::TIndex>(_idx))
		{
			assert(_idx < UINT16_MAX);
			assert(IsValidForm());
		}

		friend class ECSManager;
	public:
		constexpr EntityId() = default;

		constexpr bool IsValidForm() const { return index >= 0 && index < kMaxEntityNum; }

		constexpr operator TIndex() const { return index; }

		constexpr bool operator < (const EntityId& other) const
		{
			return ((index == kInvalidValue) && (other.index != kInvalidValue))
				|| (index < other.index);
		}

		constexpr bool operator== (const EntityId& other) const
		{
			return index == other.index;
		}
	};

	struct EntityHandle
	{
		using TGeneration = int16_t;
		constexpr static const TGeneration kNoGeneration = UINT16_MAX;

	private:
		TGeneration generation = kNoGeneration;
		EntityId id;
		friend class ECSManager;

		constexpr EntityHandle(TGeneration in_generation, EntityId in_idx)
			: generation(in_generation), id(in_idx) {}
	public:
		constexpr EntityHandle() = default;

		constexpr bool IsValidForm() const { return id.IsValidForm() && (generation != kNoGeneration); }

		operator EntityId() const { return id; }
	};

	template<size_t N, class T>
	constexpr bool AnyCommonBit(Bitset2::bitset2<N, T> const &bs1, Bitset2::bitset2<N, T> const &bs2)
	{
		return Bitset2::zip_fold_or(bs1, bs2, [](T v1, T v2) { return 0 != (v1 & v2); });
	}

	template<size_t N, class T>
	constexpr bool IsSubSetOf(Bitset2::bitset2<N, T> const &sub_set, Bitset2::bitset2<N, T> const &super_set) //
	{
		// Any bit unset in super_set must not be set in sub_set
		return Bitset2::zip_fold_and(sub_set, super_set, [](T sub, T super) { return (sub & ~super) == 0; });
	}

	namespace Details
	{
		using TCacheIter = uint32_t;

		using ComponentIdxSet = Bitset2::bitset2<kMaxComponentTypeNum>;

		template<int T, bool TIsEmpty> struct AnyComponentBase
		{
			static const constexpr uint32_t kComponentTypeIdx = T; //use  boost::hana::type_c ?
			static_assert(kComponentTypeIdx < kMaxComponentTypeNum, "too many component types");
			static_assert(kComponentTypeIdx < kActuallyImplementedComponents, "not implemented component");

			static const constexpr bool kIsEmpty = TIsEmpty;

			constexpr static ComponentIdxSet GetComponentCache()
			{
				return ComponentIdxSet{ 1 } << kComponentTypeIdx;
			}
		};

		template<int T> struct ComponentBase : public Details::AnyComponentBase<T, false>
		{
		private:
			friend class ECSManager;
			static void Remove(EntityId id);
		};

		template<bool TUseCachedIter, bool TUseAsFilter> struct BaseComponentContainer
		{
			constexpr static const bool kUseCachedIter = TUseCachedIter;
			constexpr static const bool kUseAsFilter = TUseCachedIter;
		};

		template<typename THead, typename... TTail>
		struct Split
		{
			using Head = THead;
		};

		template< class T > struct RemoveDecorators
		{
			using type = typename std::remove_reference< typename std::remove_pointer< typename std::remove_cv<T>::type >::type >::type;
		};

		enum class EComponentFilerOptions
		{
			BothMutableAndConst,
			OnlyMutable,
			OnlyConst
		};

		template<bool TIgnorePointers, EComponentFilerOptions TFilerOptions>
		struct FilterBuilder
		{
			template<typename TComp>
			constexpr static ComponentIdxSet BuildSingle()
			{
				constexpr bool kIsPointer = std::is_pointer_v<TComp>;
				constexpr bool kIsConst = std::is_const_v<std::remove_reference_t<std::remove_pointer_t<TComp>>>;
				if constexpr((TIgnorePointers && kIsPointer)
					|| ((TFilerOptions == EComponentFilerOptions::OnlyMutable) && kIsConst)
					|| ((TFilerOptions == EComponentFilerOptions::OnlyConst) && !kIsConst))
				{
					return ComponentIdxSet{};
				}
				else
				{
					return RemoveDecorators<TComp>::type::GetComponentCache();
				}
			}

			template<typename TCompHead, typename... TCompsTail>
			constexpr static ComponentIdxSet BuildHelper()
			{
				return BuildSingle<TCompHead>() | Build<TCompsTail...>();
			}

			template<typename... TComps>
			constexpr static ComponentIdxSet Build()
			{
				return BuildHelper<TComps...>();
			}

			template<>
			constexpr static ComponentIdxSet Build<>()
			{
				return ComponentIdxSet{};
			}
		};

		template<typename... TComps> static constexpr int NumCachedIter()
		{
			return  ((TComps::Container::kUseCachedIter ? 1 : 0) + ... + 0);
		}

		template<class TComp, int TIndex> struct Unbox {};

		template<class TComp, int TIndex> struct Unbox<TComp&, TIndex>
		{
			template<typename TArr> static TComp& Get(EntityId id, TArr& arr, const ComponentIdxSet&)
			{
				if constexpr(RemoveDecorators<TComp>::type::Container::kUseCachedIter)
				{
					return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id, arr[TIndex]);
				}
				(void)arr;
				return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id);
			}

			template<typename TArr, typename TKnownComp> static TComp& Get(EntityId id, TArr& arr, const ComponentIdxSet& dummy, TKnownComp& known_comp)
			{
				if constexpr (std::is_same_v<RemoveDecorators<TComp>::type, TKnownComp>)
				{
					return known_comp;
				}
				return Get<TArr>(id, arr, dummy);
			}
		};

		template<class TDecoratedComp, int TIndex> struct Unbox<TDecoratedComp*, TIndex>
		{
			template<typename TArr> static TDecoratedComp* Get(EntityId id, TArr& arr, const ComponentIdxSet& component_cache)
			{
				using TComp = typename RemoveDecorators<TDecoratedComp>::type;
				if (component_cache.test(TComp::kComponentTypeIdx))
				{
					return &(Unbox<TDecoratedComp&, TIndex>:: template Get<TArr>(id, arr, component_cache));
				}
				return nullptr;
			}

			template<typename TArr, typename TKnownComp> static TDecoratedComp* Get(EntityId id, TArr& arr, const ComponentIdxSet& component_cache, TKnownComp&)
			{
				return Get<TArr>(id, arr, component_cache);
			}
		};

		template<typename THead, typename... TTail> struct IndexOfIterParameterInner
		{
			template<typename T>
			constexpr static int Get()
			{
				return std::is_same_v<THead, T>
					? NumCachedIter<typename RemoveDecorators<TTail>::type...>()
					: IndexOfIterParameterInner<TTail...>::template Get<T>();
			}
		};

		template<typename THead> struct IndexOfIterParameterInner<THead>
		{
			template<typename T>
			constexpr static int Get()
			{
				return 0;
			}
		};

		template<typename... TComps> struct IndexOfIterParameter
		{
			template<typename T>
			constexpr static int Get()
			{
				if constexpr(sizeof...(TComps) > 0)
				{
					return IndexOfIterParameterInner<TComps...>::template Get<T>();
				}
				else
				{
					return -1;
				}
			}
		};

		template<class TComp> struct UnboxSimple {};

		template<class TComp> struct UnboxSimple<TComp&>
		{
			static TComp& Get(EntityId id, const ComponentIdxSet&)
			{
				return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id);
			}
		};

		template<class TDComp> struct UnboxSimple<TDComp*>
		{
			static TDComp* Get(EntityId id, const ComponentIdxSet& entity_components)
			{
				using TComp = typename RemoveDecorators<TDComp>::type;
				return entity_components.test(TComp::kComponentTypeIdx)
					? &TComp::GetContainer().GetChecked(id)
					: nullptr;
			}
		};
	}

	template<int T> struct EmptyComponent : public Details::AnyComponentBase<T, true> {};

	template<int T, typename TContainer, int TInitialReserveHint = (kMaxEntityNum / 8)> struct Component : public Details::ComponentBase<T>
	{
		using Container = TContainer;
		static Container __container;
		static Container& GetContainer() { return __container; }
		constexpr static const int kInitialReserve = TInitialReserveHint;

		void Initialize() {}
		void Reset() {}
	};
}
