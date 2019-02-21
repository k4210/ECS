#pragma once

#include<assert.h>
#include<type_traits>
#include<algorithm>
#include<map>
#include<vector>
#include<tuple>
#include<array>
#include<functional>


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

	template< class T > struct RemoveDecorators
	{
		using type = typename std::remove_reference< typename std::remove_pointer< typename std::remove_cv<T>::type >::type >::type;
	};

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
	}

	template<class F>
	auto ToFunc(F&& f) -> typename Details::AsFunction<F>::type {
		return { std::forward<F>(f) };
	}
}
