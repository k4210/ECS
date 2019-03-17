
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
			ScopeDurationLog __sdl(EStatId::Graphic_WaitForUpdate);
			inst.wait_for_graphic_update.WaitEnterClose();
		}

		{
			ScopeDurationLog __sdl(EStatId::Graphic_RenderSync);
			inst.ecs.Call(&GraphicSystem_RenderSync);
			inst.wait_for_render_sync.Open();
		}

		{
			ScopeDurationLog __sdl(EStatId::Display);
			inst.window.display();
		}
	}
}

void InitializeGame()
{
	auto& inst = *GResource::inst;
	const float pi = acosf(-1);
	for (int j = 0; j < 20; j++)
	{
		for (int i = 0; i < 20; i++)
		{
			const auto e = inst.ecs.AddEntity();
			inst.ecs.AddComponent<Position>(e).pos = sf::Vector2f(i * 800 / 20.0f, j * 600 / 20.0f);
			inst.ecs.AddComponent<CircleSize>(e).radius = 10;
			inst.ecs.AddComponent<Sprite2D>(e).shape.setFillColor(sf::Color::Green);
			const float angle = pi * 2.0f * (i + 1) / 22.0f;
			inst.ecs.AddComponent<Velocity>(e).velocity = sf::Vector2f(sinf(angle), cosf(angle));
			inst.ecs.AddComponent<Animation>(e);

			inst.quad_tree.Add(e, ToRegion(inst.ecs.GetComponent<Position>(e), inst.ecs.GetComponent<CircleSize>(e)));
		}
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
#if ECS_STAT_ENABLED
			ECS::Stat::LogAll(inst.frames);
#endif
			inst.close_request = true;
		}
	}
}

void MainLoopBody()
{
	ScopeDurationLog __sdl(EStatId::GameFrame);
	const auto frame_start = std::chrono::system_clock::now();
	auto& inst = *GResource::inst;

	HandleSystemEvents();
	if(inst.close_request) 
		return;

	{
		ECS::DebugLockScope __dls(inst.ecs);
		inst.ecs.CallAsync(&GraphicSystem_Update, ECS::Tag{}, EExecutionNode::Graphic_Update, ExecutionNodeIdSet{}, &inst.wait_for_graphic_update);
		inst.ecs.CallAsyncOverlap(&TestOverlap_FirstPass, &TestOverlap_SecondPass, ECS::Tag{}, ECS::Tag{}, EExecutionNode::TestOverlap);
		inst.ecs.CallAsync(&GameMovement_Update, ECS::Tag{}, EExecutionNode::Movement_Update, EExecutionNode::TestOverlap);

		inst.ecs.WorkFromMainThread(false);

		{
			ScopeDurationLog __sdl(EStatId::Graphic_WaitForRenderSync);
			inst.wait_for_render_sync.WaitEnterClose();
		}

		while (inst.ecs.AnyWorkerIsBusy())
		{
			std::this_thread::yield();
		}
		inst.ecs.ResetCompletedTasks();
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
	LOG("Frame %d time: %7.3f[ms]", inst.frames, duration_us.count() / 1000.0f);
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

#if ECS_STAT_ENABLED
			MainLoopBody(); //Remove first stat pass
			ECS::Stat::Reset();
#endif
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
	}

	delete GResource::inst;
	GResource::inst = nullptr;

	getchar();
	return 0;
}
