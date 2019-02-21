
#include "ECSContainer.h"
#include "ECSManager.h"
#include<iostream>

using namespace ECS;

struct TestComponent0 : public Component<0, DenseComponentContainer<TestComponent0>>
{
	EntityId id;
	int value = 0;
};
IMPLEMENT_COMPONENT(TestComponent0);
struct TestComponent1 : public Component<1, SortedComponentContainer<TestComponent1, false>>
{
	EntityId id;
	int value = 0;
};
IMPLEMENT_COMPONENT(TestComponent1);
struct TestComponent2 : public Component<2, SortedComponentContainer<TestComponent2, true>>
{
	EntityId id;
	int value = 0;
};
IMPLEMENT_COMPONENT(TestComponent2);
struct TestComponent3 : public Component<3, SparseComponentContainer<TestComponent3>>
{
	EntityId id;
	int value = 0;
};
IMPLEMENT_COMPONENT(TestComponent3);
/*
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
*/
ECSManager ecs;

void Test_0()
{
	assert(!ecs.IsValidEntity(0));
	assert(0 == ecs.GetNumEntities());
	const EntityId id0 = ecs.AddEntity();
	assert(id0.IsValid());
	assert(0 == id0.index);//
	assert(ecs.IsValidEntity(id0));
	assert(1 == ecs.GetNumEntities());
	assert(!ecs.IsValidEntity(1));//
	const EntityId id1 = ecs.AddEntity();
	assert(id1.IsValid());
	assert(1 == id1.index);//
	assert(ecs.IsValidEntity(id1));
	assert(2 == ecs.GetNumEntities());
	assert(ecs.IsValidEntity(id1));
	ecs.RemoveEntity(id1);
	assert(ecs.IsValidEntity(id0));
	assert(1 == ecs.GetNumEntities());
	assert(!ecs.IsValidEntity(id1));//
	ecs.RemoveEntity(id0);
	assert(!ecs.IsValidEntity(id0));
	assert(0 == ecs.GetNumEntities());
}

void Test_1()
{
	const EntityId id = ecs.AddEntity();
	assert(!ecs.HasComponent<TestComponent0>(id));
	assert(!ecs.HasComponent<TestComponent1>(id));
	assert(!ecs.HasComponent<TestComponent2>(id));
	assert(!ecs.HasComponent<TestComponent3>(id));

	ecs.AddComponent<TestComponent0>(id).id = id;
	assert(ecs.HasComponent<TestComponent0>(id));
	assert(!ecs.HasComponent<TestComponent1>(id));
	assert(!ecs.HasComponent<TestComponent2>(id));
	assert(!ecs.HasComponent<TestComponent3>(id));

	ecs.AddComponent<TestComponent1>(id).id = id;
	ecs.AddComponent<TestComponent2>(id).id = id;
	ecs.AddComponent<TestComponent3>(id).id = id;
	assert(ecs.HasComponent<TestComponent0>(id));
	assert(ecs.HasComponent<TestComponent1>(id));
	assert(ecs.HasComponent<TestComponent2>(id));
	assert(ecs.HasComponent<TestComponent3>(id));

	const EntityId id1 = ecs.AddEntity();
	assert(!ecs.HasComponent<TestComponent0>(id1));
	assert(!ecs.HasComponent<TestComponent1>(id1));
	assert(!ecs.HasComponent<TestComponent2>(id1));
	assert(!ecs.HasComponent<TestComponent3>(id1));

	ecs.AddComponent<TestComponent0>(id1).id = id1;
	ecs.AddComponent<TestComponent1>(id1).id = id1;
	ecs.AddComponent<TestComponent2>(id1).id = id1;
	ecs.AddComponent<TestComponent3>(id1).id = id1;
	assert(ecs.HasComponent<TestComponent0>(id1));
	assert(ecs.HasComponent<TestComponent1>(id1));
	assert(ecs.HasComponent<TestComponent2>(id1));
	assert(ecs.HasComponent<TestComponent3>(id1));

	ecs.RemoveComponent<TestComponent0>(id1);
	assert(!ecs.HasComponent<TestComponent0>(id1));
	assert(ecs.HasComponent<TestComponent1>(id1));
	assert(ecs.HasComponent<TestComponent2>(id1));
	assert(ecs.HasComponent<TestComponent3>(id1));

	ecs.RemoveComponent<TestComponent1>(id1);
	ecs.RemoveComponent<TestComponent2>(id1);
	ecs.RemoveComponent<TestComponent3>(id1);
	assert(!ecs.HasComponent<TestComponent0>(id1));
	assert(!ecs.HasComponent<TestComponent1>(id1));
	assert(!ecs.HasComponent<TestComponent2>(id1));
	assert(!ecs.HasComponent<TestComponent3>(id1));

	assert(ecs.HasComponent<TestComponent0>(id));
	assert(ecs.HasComponent<TestComponent1>(id));
	assert(ecs.HasComponent<TestComponent2>(id));
	assert(ecs.HasComponent<TestComponent3>(id));

	assert(ecs.GetComponent<TestComponent0>(id).id == id);
	assert(ecs.GetComponent<TestComponent1>(id).id == id);
	assert(ecs.GetComponent<TestComponent2>(id).id == id);
	assert(ecs.GetComponent<TestComponent3>(id).id == id);
}

void Test_2()
{
	for (int i = 0; i < 16; i++)
	{
		const EntityId id = ecs.AddEntity();
		if (0 != (i & 1))
		{
			ecs.AddComponent<TestComponent0>(id).id = id;
		}
		if (0 != (i & 2))
		{
			ecs.AddComponent<TestComponent1>(id).id = id;
		}
		if (0 != (i & 4))
		{
			ecs.AddComponent<TestComponent2>(id).id = id;
		}
		if (0 != (i & 8))
		{
			ecs.AddComponent<TestComponent3>(id).id = id;
		}
	}
	{
		int counter = 0;
		ecs.Call(ToFunc([&](EntityId id, const TestComponent0& t0, const TestComponent1* t1)
		{
			assert(t0.id == id);
			assert(!t1 || (t1->id == id));
			counter++;
		}));
		assert(8 == counter);
	}

	{
		int counter = 0;
		auto test_lambda = ToFunc([&](EntityId id, TestComponent0& t0, TestComponent1& t1, TestComponent2& t2, TestComponent3& t3)
		{
			assert(t0.id == id);
			assert(t1.id == id);
			assert(t2.id == id);
			assert(t3.id == id);
			counter++;
		});
		ecs.Call(test_lambda);
		assert(1 == counter);
	}
}

int main()
{
	Test_0();
	ecs.Reset();
	Test_1();
	ecs.Reset();
	Test_2();
	ecs.Reset();
	std::cout << "Tests succedded!...\n";
	getchar();
/*	auto init_actor = [](auto actor)
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

	UpdatePos(ecs);
	AccelerateMove(ecs);
	UpdateAcceleration(ecs);

	ecs.RemoveEntity(actor1);

	ecs.Reset();
*/
}
