
#include "ECS.h"

using namespace ECS;

struct PositionComponent : public Component<0, DenseComponentContainer<PositionComponent>>
{
	float x = 0;
	float y = 0;
};
IMPLEMENT_COMPONENT(PositionComponent);

struct MovementComponent : public Component<1, SortedComponentContainer<MovementComponent, false>>
{
	float x = 0;
	float y = 0;
};
IMPLEMENT_COMPONENT(MovementComponent);

struct AccelerationComponent : public Component<2, SparseComponentContainer<AccelerationComponent>>
{
	float x = 0;
	float y = 0;
};
IMPLEMENT_COMPONENT(AccelerationComponent);

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
	auto init_actor = [](auto actor)
	{
		ecs.AddComponent<PositionComponent>(actor);
		auto& mov_comp = ecs.AddComponent<MovementComponent>(actor);
		mov_comp.x = 2;
		mov_comp.y = -1;
		auto& acc_comp = ecs.AddComponent<AccelerationComponent>(actor);
		acc_comp.x = 1;
		acc_comp.y = 0;
	};
	const auto actor1 = ecs.AddEntity();
	init_actor(actor1);

	const auto actor2 = ecs.AddEntity();
	init_actor(actor2);

	ms.UpdatePos(ecs);
	ms.AccelerateMove(ecs);
	ms.UpdateAcceleration(ecs);

	ecs.RemoveEntity(actor1);
}
