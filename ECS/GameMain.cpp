
#include "Systems.h"

SResources* SResources::inst = nullptr;

void RenderLoop(sf::RenderWindow* window)
{
	auto& inst = *SResources::inst;
	window->setActive(true);
	while (window->isOpen())
	{
		window->clear();
		
		std::future<void> graphic_update_done;
		{
			LOG(ScopeDurationLog sdl("RenderLoop waited for %s %lld us \n", " triggering Update");)
			graphic_update_done = inst.main_thread_triggered_graphic_update.get_future().get();
		}
		{
			LOG(ScopeDurationLog sdl("RenderLoop waited for %s %lld us \n", " finishing Update");)
			graphic_update_done.wait();
		}
		inst.main_thread_triggered_graphic_update = {};
		//TODO: what about closing window ?

		LOG(ScopeDurationLog sdl("RenderLoop %s %lld us \n", "Sync and Display");)
		inst.graphic_system.RenderSync();
		inst.render_thread_done_sync.set_value();

		window->display();
	}
}

void InitializeGame()
{
	auto& inst = *SResources::inst;
	const float pi = acosf(-1);
	for (int i = 0; i < 20; i++)
	{
		const auto e = inst.ecs.AddEntity();
		inst.ecs.AddComponent<Position>(e).pos = sf::Vector2f(i * 800 / 20.0f, i * 600 / 20.0f);
		inst.ecs.AddComponent<CircleSize>(e).radius = 15;
		inst.ecs.AddComponent<Sprite2D>(e).shape.setFillColor(sf::Color::Green);
		const float angle = pi * 2.0f * (i + 1) / 22.0f;
		inst.ecs.AddComponent<Velocity>(e).velocity = sf::Vector2f(sinf(angle), cosf(angle));
	}

	inst.ecs.StartThreads();
}

void CleanGame()
{
	SResources::inst->ecs.StopThreads();
	SResources::inst->ecs.Reset();
	delete SResources::inst;
	SResources::inst = nullptr;
}

void MainLoopBody()
{
	const auto frame_start = std::chrono::system_clock::now();
	auto& inst = *SResources::inst;

	{
		sf::Event event;
		while (inst.window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
			{
				inst.window.close();
				//set inst.main_thread_triggered_graphic_update 
			}
		}
	}

	{
		ECS::DebugLockScope __dls(inst.ecs);
		inst.main_thread_triggered_graphic_update.set_value(inst.graphic_system.Update());
		inst.movement_system.Update();

		while (inst.ecs.AnyWorkerIsBusy())
		{
			inst.ecs.WorkFromMainThread(false);
			std::this_thread::yield();
		}

		{
			LOG(ScopeDurationLog sdl("MainLoop waited for %s %lld us \n", " Render Sync");)
			inst.render_thread_done_sync.get_future().wait();
		}
		inst.render_thread_done_sync = {};
	}

	HandleGameEvents();
	

	const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - frame_start);
	inst.frame_time_seconds = duration_us.count() / 1000000.0f;
	LOG(printf_s("FRAME TIME:  %lld us \n", duration_us.count());)
}

int main()
{
	SResources::inst = new SResources();

	auto& inst = *SResources::inst;
	inst.window.create(sf::VideoMode(800, 600), "HnS");
	inst.window.setActive(false);
	sf::Thread render_thread = sf::Thread(&RenderLoop, &inst.window);
	render_thread.launch();

	InitializeGame();

	while (SResources::inst->window.isOpen())
	{
		MainLoopBody();
	}

	CleanGame();
	return 0;
}
