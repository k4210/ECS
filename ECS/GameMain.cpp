
#include "Systems.h"

SResources* SResources::inst = nullptr;


//TODO: Frame time
//Render Thread
//

int main()
{
	SResources::inst = new SResources{};
	{
		auto& inst = *SResources::inst;
		inst.window.create(sf::VideoMode(800, 600), "HnS");

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
		while (inst.window.isOpen())
		{
			sf::Event event;
			while (inst.window.pollEvent(event))
			{
				if (event.type == sf::Event::Closed)
					inst.window.close();
			}

			inst.window.clear();
			{
				ECS::DebugLockScope __dls(inst.ecs);
				auto graphic_update_complete = inst.graphic_system.Update();
				inst.movement_system.Update();
				
				while (std::future_status::ready != graphic_update_complete.wait_for(std::chrono::microseconds(0)))
				{
					inst.ecs.WorkFromMainThread(true);
				}
				
				inst.graphic_system.RenderSync();

				while (inst.ecs.AnyWorkerIsBusy())
				{
					inst.ecs.WorkFromMainThread(false);
					std::this_thread::yield();
				}
			}
			{
				LOG(ECS::ScopeDurationLog __sdl("Display time%s %lld us \n");)
				inst.window.display();
			}
		}
		inst.ecs.StopThreads();
		inst.ecs.Reset();
	}
	delete SResources::inst;
	SResources::inst = nullptr;

	return 0;
}
