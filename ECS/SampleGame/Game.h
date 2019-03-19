#pragma once
#include "Systems.h"
#include "BaseGame\GameBase.h"

using namespace ECS;

struct EExecutionNode
{
	constexpr static const ExecutionNodeId Graphic_Update{ 0 };
	constexpr static const ExecutionNodeId Movement_Update{ 1 };
	constexpr static const ExecutionNodeId TestOverlap{ 2 };
};

struct GameInstance : public BaseGameInstance
{
	void InitializeGame() override
	{
		const float pi = acosf(-1);
		for (int j = 0; j < 20; j++)
		{
			for (int i = 0; i < 20; i++)
			{
				const auto e = ecs.AddEntity();
				ecs.AddComponent<Position>(e).pos = sf::Vector2f(i * 800 / 20.0f, j * 600 / 20.0f);
				ecs.AddComponent<CircleSize>(e).radius = 10;
				ecs.AddComponent<Sprite2D>(e).shape.setFillColor(sf::Color::Green);
				const float angle = pi * 2.0f * (i + 1) / 22.0f;
				ecs.AddComponent<Velocity>(e).velocity = sf::Vector2f(sinf(angle), cosf(angle));
				ecs.AddComponent<Animation>(e);

				quad_tree.Add(e, ToRegion(ecs.GetComponent<Position>(e), ecs.GetComponent<CircleSize>(e)));
			}
		}
	}

	void DispatchTasks() override
	{
		ecs.CallAsync(&GraphicSystem_Update, ECS::Tag{}, EExecutionNode::Graphic_Update, ExecutionNodeIdSet{}, &wait_for_graphic_update);
		ecs.CallAsyncOverlap(&TestOverlap_FirstPass, &TestOverlap_SecondPass, ECS::Tag{}, ECS::Tag{}, EExecutionNode::TestOverlap);
		ecs.CallAsync(&GameMovement_Update, ECS::Tag{}, EExecutionNode::Movement_Update, EExecutionNode::TestOverlap);
	}

	void Render() override 
	{
		ecs.CallBlocking(&GraphicSystem_RenderSync, ECS::Tag::Any());
	}
};

BaseGameInstance* BaseGameInstance::CreateGameInstance()
{
	return new GameInstance{};
}