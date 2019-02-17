
#include "ECS.h"

struct PositionComponent : public ComponentBase<0>
{
	using Container = DenseComponentContainer<PositionComponent>;
	static Container& GetContainer();

	float x = 0;
	float y = 0;
};
PositionComponent::Container PositionComponent_Container;
PositionComponent::Container& PositionComponent::GetContainer() { return PositionComponent_Container; }
template<> void ComponentBase<0>::Remove(EntityId id) { PositionComponent_Container.Remove(id); }

struct MovementComponent : public ComponentBase<1>
{
	using Container = DenseComponentContainer<MovementComponent>;
	static Container& GetContainer();

	float x = 0;
	float y = 0;
};
MovementComponent::Container MovementComponent_Container;
MovementComponent::Container& MovementComponent::GetContainer() { return MovementComponent_Container; }
template<> void ComponentBase<1>::Remove(EntityId id) { MovementComponent_Container.Remove(id); }

struct AccelerationComponent : public ComponentBase<2>
{
	using Container = SortedComponentContainer<AccelerationComponent>;
	static Container& GetContainer();

	float x = 0;
	float y = 0;
};
AccelerationComponent::Container AccelerationComponent_Container;
AccelerationComponent::Container& AccelerationComponent::GetContainer() { return AccelerationComponent_Container; }
template<> void ComponentBase<2>::Remove(EntityId id) { AccelerationComponent_Container.Remove(id); }

struct MovementSystem
{
public:
	void UpdatePos(ECSManager& ecs)
	{
		auto update_pos = [](PositionComponent* Pos, const MovementComponent* Move)
		{
			Pos->x += Move->x;
			Pos->y += Move->y;
		};
		ecs.Call<decltype(update_pos), PositionComponent, const MovementComponent>(update_pos);
	}
	void AccelerateMove(ECSManager& ecs)
	{
		auto update_move = [](const AccelerationComponent* Acceleration, MovementComponent* Move)
		{
			Move->x += Acceleration->x;
			Move->y += Acceleration->y;
		};
		ecs.CallHint<const AccelerationComponent, decltype(update_move), MovementComponent>(update_move);
	}
	void UpdateAcceleration(ECSManager& ecs)
	{
		auto update_acc = [](AccelerationComponent* Acceleration)
		{
			Acceleration->x += 1;
			Acceleration->y += 1;
		};
		ecs.CallHintSingle<AccelerationComponent>(update_acc);
	}
};

ECSManager ecs;
MovementSystem ms;

int main()
{
	const auto actor1 = ecs.AddEntity();
	{
		ecs.AddComponent<PositionComponent>(actor1);
		auto& mov_comp = ecs.AddComponent<MovementComponent>(actor1);
		mov_comp.x = 2;
		mov_comp.y = -1;
		auto& acc_comp = ecs.AddComponent<AccelerationComponent>(actor1);
		acc_comp.x = 1;
		acc_comp.y = 0;
	}
	ms.UpdatePos(ecs);
	ms.AccelerateMove(ecs);
	ms.UpdateAcceleration(ecs);

	ecs.RemoveEntity(actor1);
}
