
#include "Systems.h"

SResources* SResources::inst = nullptr;

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

		std::vector<std::future<void>> blocking_tasks;
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
				blocking_tasks.emplace_back(inst.movement_system.Update());
				blocking_tasks.emplace_back(inst.graphic_system.Update());

				for (auto& ft : blocking_tasks)
				{
					ft.wait();
				}
				blocking_tasks.resize(0);

				inst.graphic_system.RenderSync();
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
