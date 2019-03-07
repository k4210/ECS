#include "ECSContainer.h"
#include "ECSManagerAsync.h"
#include "ECSEvent.h"

using namespace ECS;

#define RUN_TESTS 0

#if RUN_TESTS
struct TestComponent0 : public Component<0, DenseComponentContainer<TestComponent0>>
{
	EntityId id;
	int value = 0;
	bool bIsInitializesd = false;
	void Initialize() { bIsInitializesd = true; }
	void Reset() { assert(bIsInitializesd); bIsInitializesd = false;}
};
IMPLEMENT_COMPONENT(TestComponent0);

struct TestComponent1 : public Component<1, SortedComponentContainer<TestComponent1, false>>
{
	EntityId id;
	int value = 0;
	bool bIsInitializesd = false;
	void Initialize() { bIsInitializesd = true; }
	void Reset() { assert(bIsInitializesd); bIsInitializesd = false; }
};
IMPLEMENT_COMPONENT(TestComponent1);

struct TestComponent2 : public Component<2, SortedComponentContainer<TestComponent2, true>>
{
	EntityId id;
	int value = 0;
	bool bIsInitializesd = false;
	void Initialize() { bIsInitializesd = true; }
	void Reset() { assert(bIsInitializesd); bIsInitializesd = false; }
};
IMPLEMENT_COMPONENT(TestComponent2);

struct TestComponent3 : public Component<3, SparseComponentContainer<TestComponent3>>
{
	EntityId id;
	int value = 0;
	bool bIsInitializesd = false;
	void Initialize() { bIsInitializesd = true; }
	void Reset() { assert(bIsInitializesd); bIsInitializesd = false; }
};
IMPLEMENT_COMPONENT(TestComponent3);

struct EmptyComponent0 : EmptyComponent<4> {};
IMPLEMENT_EMPTY_COMPONENT(EmptyComponent0);

ECSManagerAsync ecs;

void Test_0()
{
	assert(0 == ecs.GetNumEntities());
	const EntityHandle id0 = ecs.AddEntity();
	assert(id0.IsValidForm());
	assert(0 == id0.id.index);//
	assert(ecs.IsValidEntity(id0));
	assert(1 == ecs.GetNumEntities());
	const EntityHandle id1 = ecs.AddEntity(100);
	assert(id1.IsValidForm());
	assert(100 == id1.id.index);//
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

	const EntityHandle id01 = ecs.AddEntity();
	assert(ecs.IsValidEntity(id01));
}

void Test_1()
{
	const EntityHandle handle = ecs.AddEntity();
	const EntityId id = handle.id;
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

	const EntityHandle handle1 = ecs.AddEntity();
	const EntityId id1 = handle1.id;
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
		const EntityHandle id = ecs.AddEntity(64);
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
			ecs.AddEmptyComponent<EmptyComponent0>(id);
		}
	}

	DebugLockScope dls(ecs);
	{
		int counter = 0;
		ecs.Call<Filter<EmptyComponent0>>(ToFunc([&](EntityId id, const TestComponent0* t0, const TestComponent1* t1)
		{
			assert(!t0 || (t0->id == id));
			assert(!t1 || (t1->id == id));
			counter++;
			assert(ecs.HasComponent<EmptyComponent0>(id));
		}));
		assert(8 == counter);
	}

	{
		int counter = 0;
		ecs.Call<Filter<TestComponent1>>(ToFunc([&](EntityId id, const TestComponent0& t0, const TestComponent1* t1)
		{
			assert(t0.id == id);
			assert(!t1 || (t1->id == id));
			counter++;
			assert(ecs.HasComponent<TestComponent1>(id));
		}));
		assert(4 == counter);
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

	{
		auto test_lambda = ToFunc([&](EntityId id, TestComponent1& t1, TestComponent2& t2, TestComponent3& t3)
		{
			assert(t1.id == id);
			assert(t2.id == id);
			assert(t3.id == id);
		});
		ecs.Call(test_lambda);
	}
	{
		int counter = 0;
		auto test_lambda = ToFunc([&](EntityId id, TestComponent1& t1)
		{
			assert(t1.id == id);
			counter++;
		});
		ecs.Call(test_lambda);
		assert(counter == 8);
	}
}

void Test_3()
{
	assert(!ecs.AnyWorkerIsBusy());
	ecs.StartThreads();
	assert(!ecs.AnyWorkerIsBusy());

	for (int i = 0; i < 16; i++)
	{
		const EntityId id = ecs.AddEntity(64);
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
			ecs.AddEmptyComponent<EmptyComponent0>(id);
		}
	}

	DebugLockScope dls(ecs);
	{
		int counter1 = 0;
		auto test_lambda1 = ToFunc([&](EntityId id, TestComponent0& t0, TestComponent1& t1)
		{
			assert(t0.id == id);
			assert(t1.id == id);
			std::this_thread::sleep_for(std::chrono::microseconds(1000));
			counter1++;
		});
		auto future1 = ecs.CallAsync(test_lambda1, 1);

		int counter2 = 0;
		auto test_lambda2 = ToFunc([&](EntityId id, TestComponent2& t2, TestComponent3& t3)
		{
			assert(t2.id == id);
			assert(t3.id == id);
			std::this_thread::sleep_for(std::chrono::microseconds(1000));
			counter2++;
		});
		auto future2 = ecs.CallAsync(test_lambda2, 2);


		future1.get();
		assert(4 == counter1);

		future2.get();
		assert(4 == counter2);
	}

	ecs.StopThreads();
	assert(!ecs.AnyWorkerIsBusy());
}

void TestMain()
{
	Test_0();
	ecs.Reset();
	Test_1();
	ecs.Reset();
	Test_2();
	ecs.Reset();
	Test_3();
	ecs.Reset();
	printf_s("Tests succedded...\n");
	getchar();
}

int main()
{
	TestMain();
}

#endif