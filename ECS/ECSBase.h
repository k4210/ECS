#pragma once

#include<assert.h>
#include<type_traits>
#include<functional>
#include "bitset2\bitset2.hpp"

#define IMPLEMENT_COMPONENT(COMP) COMP::Container ECS::Component<COMP::kComponentTypeIdx, COMP::Container>::__container; \
	template<> void ECS::ComponentBase<COMP::kComponentTypeIdx>::Remove(EntityId id) { COMP::GetContainer().Remove(id); } \

#define IMPLEMENT_EMPTY_COMPONENT(COMP) template<> void ECS::ComponentBase<COMP::kComponentTypeIdx>::Remove(EntityId) { }

namespace ECS
{
	static const constexpr int kMaxComponentTypeNum = 256;
	static const constexpr int kMaxEntityNum = 1024;
	static const constexpr int kActuallyImplementedComponents = 5;
	static const constexpr int kMaxConcurrentWorkerThreads = 4; //change ECSManagerAsync constructor
	static const constexpr int kMaxExecutionStream = 64;
	static_assert(kActuallyImplementedComponents <= kMaxComponentTypeNum, "too many component types");

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

	using TCacheIter = unsigned int;

	template<int T, bool TIsEmpty> struct AnyComponentBase
	{
		static const constexpr int kComponentTypeIdx = T; //use  boost::hana::type_c ?
		static_assert(kComponentTypeIdx < kMaxComponentTypeNum, "too many component types");
		static_assert(kComponentTypeIdx < kActuallyImplementedComponents, "not implemented component");

		static const constexpr bool kIsEmpty = TIsEmpty;
	};

	template<int T> struct EmptyComponent : public AnyComponentBase<T, true> {};

	template<int T> struct ComponentBase : public AnyComponentBase<T, false>
	{
	private:
		friend class ECSManager;
		static void Remove(EntityId id);
	};

	template<int T, typename TContainer, int TInitialReserveHint = (kMaxEntityNum/8)> struct Component : public ComponentBase<T>
	{
		using Container = TContainer;
		static Container __container;
		static Container& GetContainer() { return __container; }
		constexpr static const int kInitialReserve = TInitialReserveHint;

		void Initialize() {}
		void Reset() {}
	};

	template<bool TUseCachedIter, bool TUseAsFilter> struct BaseComponentContainer
	{
		constexpr static const bool kUseCachedIter = TUseCachedIter;
		constexpr static const bool kUseAsFilter = TUseCachedIter;
	};

	using ComponentCache = Bitset2::bitset2<kMaxComponentTypeNum>;

	namespace Details
	{
		template<class T>
		struct AsFunction
			: public AsFunction<decltype(&T::operator())>
		{};

		template<class ReturnType, class... Args>
		struct AsFunction<ReturnType(Args...)> {
			using type = std::function<ReturnType(Args...)>;
		};

		template<class ReturnType, class... Args>
		struct AsFunction<ReturnType(*)(Args...)> {
			using type = std::function<ReturnType(Args...)>;
		};

		template<class Class, class ReturnType, class... Args>
		struct AsFunction<ReturnType(Class::*)(Args...) const> {
			using type = std::function<ReturnType(Args...)>;
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
			constexpr static ComponentCache BuildSingle()
			{
				constexpr bool kIsPointer = std::is_pointer_v<TComp>;
				constexpr bool kIsConst = std::is_const_v<std::remove_reference_t<std::remove_pointer_t<TComp>>>;
				if constexpr((TIgnorePointers && kIsPointer)
					|| ((TFilerOptions == EComponentFilerOptions::OnlyMutable) && kIsConst)
					|| ((TFilerOptions == EComponentFilerOptions::OnlyConst) && !kIsConst))
				{
					return ComponentCache{};
				}
				return ComponentCache{ 1 } << RemoveDecorators<TComp>::type::kComponentTypeIdx;
			}

			template<typename TCompHead, typename... TCompsTail>
			constexpr static ComponentCache BuildHelper()
			{
				return BuildSingle<TCompHead>() | Build<TCompsTail...>();
			}

			template<typename... TComps>
			constexpr static ComponentCache Build()
			{
				return BuildHelper<TComps...>();
			}

			template<>
			constexpr static ComponentCache Build<>()
			{
				return ComponentCache{};
			}
		};

		template<bool TIgnorePointers, EComponentFilerOptions TFilerOptions, typename... TComps>
		constexpr ComponentCache BuildFilter()
		{
			return FilterBuilder<TComps...>::Build<TIgnorePointers, TFilerOptions>();
		}

		template<bool TIgnorePointers, EComponentFilerOptions TFilerOptions>
		constexpr ComponentCache BuildFilter()
		{
			return ComponentCache{};
		}

		template<typename... TComps> static constexpr int NumCachedIter()
		{
			return  ((TComps::Container::kUseCachedIter ? 1 : 0) + ... + 0);
		}

		template<class TComp, int TIndex>
		struct Unbox {};

		template<class TComp, int TIndex>
		struct Unbox<TComp&, TIndex>
		{
			template<typename TArr> static TComp& Get(EntityId id, TArr& arr, const ComponentCache&)
			{
				if constexpr(RemoveDecorators<TComp>::type::Container::kUseCachedIter)
				{
					return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id, arr[TIndex]);
				}
				return RemoveDecorators<TComp>::type::GetContainer().GetChecked(id);
			}
		};

		template<class TDecoratedComp, int TIndex>
		struct Unbox<TDecoratedComp*, TIndex>
		{
			template<typename TArr> static TDecoratedComp* Get(EntityId id, TArr& arr, const ComponentCache& component_cache)
			{
				using TComp = typename RemoveDecorators<TDecoratedComp>::type;
				if (component_cache.test(TComp::kComponentTypeIdx))
				{
					return &(Unbox<TDecoratedComp&, TIndex>::Get<TArr>(id, arr, component_cache));
				}
				return nullptr;
			}
		};

		template<typename THead, typename... TTail>
		struct IndexOfIterParameterInner
		{
			template<typename T>
			constexpr static int Get()
			{
				return std::is_same_v<THead, T>
					? NumCachedIter<typename RemoveDecorators<TTail>::type...>()
					: IndexOfIterParameterInner<TTail...>::Get<T>();
			}
		};

		template<typename THead>
		struct IndexOfIterParameterInner<THead>
		{
			template<typename T>
			constexpr static int Get()
			{
				return 0;
			}
		};

		template<typename... TComps>
		struct IndexOfIterParameter
		{
			template<typename T>
			constexpr static int Get()
			{
				if constexpr(sizeof...(TComps) > 0)
				{
					return IndexOfIterParameterInner<TComps...>::Get<T>();
				}
				else
				{
					return -1;
				}
			}
		};

		template<size_t N, class T>
		bool any_common_bit(Bitset2::bitset2<N, T> const &bs1, Bitset2::bitset2<N, T> const &bs2)
		{
			using base_t = T;
			return Bitset2::zip_fold_or(bs1, bs2,
				[](base_t v1, base_t v2) noexcept
			{ return 0 != (v1 & v2); });
		}
	}

	template<class F>
	auto ToFunc(F&& f) -> typename Details::AsFunction<F>::type 
	{
		return { std::forward<F>(f) };
	}
}
