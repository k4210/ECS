
#include "Systems.h"

using namespace ECS;

GResource* GResource::inst = nullptr;

void RenderLoop()
{
	auto& inst = *GResource::inst;
	inst.window.setActive(true);

	while (!inst.close_request)
	{
		inst.window.clear();
		{
			STAT(ScopeDurationLog __sdl(EStatId::Graphic_WaitForUpdate);)
			inst.wait_for_graphic_update.WaitEnterClose();
		}

		{
			STAT(ScopeDurationLog __sdl(EStatId::Graphic_RenderSync);)
			inst.ecs.Call(&GraphicSystem_RenderSync);
			inst.wait_for_render_sync.Open();
		}

		{
			STAT(ScopeDurationLog __sdl(EStatId::Display);)
			inst.window.display();
		}
	}
}

void InitializeGame()
{
	auto& inst = *GResource::inst;
	const float pi = acosf(-1);
	for (int i = 0; i < 20; i++)
	{
		const auto e = inst.ecs.AddEntity();
		inst.ecs.AddComponent<Position>(e).pos = sf::Vector2f(i * 800 / 20.0f, i * 600 / 20.0f);
		inst.ecs.AddComponent<CircleSize>(e).radius = 15;
		inst.ecs.AddComponent<Sprite2D>(e).shape.setFillColor(sf::Color::Green);
		const float angle = pi * 2.0f * (i + 1) / 22.0f;
		inst.ecs.AddComponent<Velocity>(e).velocity = sf::Vector2f(sinf(angle), cosf(angle));
		inst.ecs.AddComponent<Animation>(e);
	}
}

void HandleSystemEvents()
{
	auto& inst = *GResource::inst;
	sf::Event event;
	while (inst.window.pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
		{
			inst.close_request = true;
		}
	}
}

void TestOverlap1(ECS::EntityId id_a, const Position& pos_a, const CircleSize& size_a, Velocity& vel_a
	, ECS::EntityId id_b, Velocity& vel_b)
{
	int k = 0; k++;
}

void TestOverlap2(ECS::EntityId id_a, const Position& pos_a, const CircleSize& size_a
	, ECS::EntityId id_b, Velocity& vel_b)
{
	int k = 0; k++;
}

struct QuadTree
{
	struct Iter
	{
		EntityId* id = nullptr;
		operator bool() const { return id; }
		const EntityId& operator*() const { return *id; }
		Iter& operator++() { return *this; }
		Iter operator++(int)
		{
			Iter tmp(*this); // copy
			operator++(); // pre-increment
			return tmp;   // return old value
		}
	};

	Iter GetIter(ECS::EntityId id, const Position& pos, const CircleSize& size) const
	{
		return Iter{};
	}
};


void MainLoopBody()
{
	STAT(ScopeDurationLog __sdl(EStatId::GameFrame);)
	const auto frame_start = std::chrono::system_clock::now();
	auto& inst = *GResource::inst;

	HandleSystemEvents();
	if(inst.close_request) 
		return;

	{
		ECS::DebugLockScope __dls(inst.ecs);
		inst.ecs.CallAsync(&GraphicSystem_Update, EStreams::Graphic, &inst.wait_for_graphic_update STAT_PARAM(EStatId::Graphic_Update));
		inst.ecs.CallAsync(&GameMovement_Update, EStreams::None, nullptr STAT_PARAM(EStatId::GameMovement_Update));

		QuadTree qt;
		{
			OverlapsContext<Velocity&> ctx(inst.ecs);
			ctx.Call(&TestOverlap1, qt);
		}
		{
			OverlapsContext<> ctx(inst.ecs);
			ctx.Call(&TestOverlap2, qt);
		}
		inst.ecs.Call(&Animation_Update);

		inst.ecs.WorkFromMainThread(false);

		{
			STAT(ScopeDurationLog __sdl(EStatId::Graphic_WaitForRenderSync);)
			inst.wait_for_render_sync.WaitEnterClose();
		}

		while (inst.ecs.AnyWorkerIsBusy())
		{
			std::this_thread::yield();
		}
	}

	{
		EventStorage storage;
		while (GResource::inst->event_manager.Pop(storage))
		{
			IEvent* e = storage.Get();
			assert(e);
			if (e)
			{
				e->Execute();
			}
		}
	}

	const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - frame_start);
	inst.frame_time_seconds = duration_us.count() / 1000000.0f;
	inst.frames++;
}

int main()
{
	GResource::inst = new GResource();
	{
		auto& inst = *GResource::inst;

		InitializeGame();
		inst.ecs.StartThreads();
		{
			inst.window.create(sf::VideoMode(800, 600), "HnS");
			inst.window.setActive(false);
			std::thread render_thread(RenderLoop);
			while (!GResource::inst->close_request)
			{
				MainLoopBody();
			}

			{
				ECS::DebugLockScope __dls(inst.ecs);
				inst.wait_for_graphic_update.Open();
				render_thread.join();
			}
			inst.window.close();
		}

		inst.ecs.StopThreads();
		inst.ecs.Reset();

		STAT(ECS::Stat::LogAll(inst.frames);)
	}

	delete GResource::inst;
	GResource::inst = nullptr;

	getchar();
	return 0;
}
