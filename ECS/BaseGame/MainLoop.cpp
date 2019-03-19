
#include "GameBase.h"

using namespace ECS;

BaseGameInstance* BaseGameInstance::inst = nullptr;

void RenderLoop()
{
	auto& inst = *BaseGameInstance::inst;
	inst.window.setActive(true);

	while (!inst.close_request)
	{
		inst.window.clear();
		{
			ScopeDurationLog __sdl(EStatId::Graphic_WaitForUpdate, EPredefinedStatGroups::Framework);
			inst.wait_for_graphic_update.WaitEnterClose();
		}

		{
			ScopeDurationLog __sdl(EStatId::Graphic_RenderSync, EPredefinedStatGroups::Framework);
			inst.Render();
			inst.wait_for_render_sync.Open();
		}

		{
			ScopeDurationLog __sdl(EStatId::Display, EPredefinedStatGroups::Framework);
			inst.window.display();
		}
	}
}

void HandleSystemEvents()
{
	auto& inst = *BaseGameInstance::inst;
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
	ScopeDurationLog __sdl(EStatId::GameFrame, EPredefinedStatGroups::Framework);
	const auto frame_start = std::chrono::system_clock::now();
	auto& inst = *BaseGameInstance::inst;

	HandleSystemEvents();
	if(inst.close_request) 
		return;

	{
		ECS::DebugLockScope __dls(inst.ecs);
		inst.DispatchTasks();
		inst.ecs.WorkFromMainThread(false);

		{
			ScopeDurationLog __sdl(EStatId::Graphic_WaitForRenderSync, EPredefinedStatGroups::Framework);
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
		while (BaseGameInstance::inst->event_manager.Pop(storage))
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
	BaseGameInstance::inst = BaseGameInstance::CreateGameInstance();
	{
		auto& inst = *BaseGameInstance::inst;

		inst.InitializeGame();
		inst.ecs.StartThreads();
		{
			inst.window.create(sf::VideoMode(800, 600), "HnS");
			inst.window.setActive(false);
			std::thread render_thread(RenderLoop);

#if ECS_STAT_ENABLED
			MainLoopBody(); //Remove first stat pass
			ECS::Stat::Reset();
#endif
			while (!BaseGameInstance::inst->close_request)
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

	delete BaseGameInstance::inst;
	BaseGameInstance::inst = nullptr;

	getchar();
	return 0;
}

#if ECS_STAT_ENABLED
namespace 
{
	static Stat::Register static_stat_register(EStatId::_Count, EPredefinedStatGroups::Framework, [](uint32_t eid)
	{
		const EStatId id = static_cast<EStatId>(eid);
		switch (id)
		{
			case EStatId::Graphic_WaitForUpdate: return "Graphic_WaitForUpdate";
			case EStatId::Graphic_RenderSync: return "Graphic_RenderSync";
			case EStatId::Graphic_WaitForRenderSync: return "Graphic_WaitForRenderSync";
			case EStatId::Display: return "Display";
			case EStatId::GameFrame: return "GameFrame";
			case EStatId::QuadTreeIteratorConstrucion: return "QuadTreeIteratorConstrucion";
		}
		return "unknown";
	});
}
#endif //ECS_STAT_ENABLED