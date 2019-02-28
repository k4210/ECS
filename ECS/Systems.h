#pragma once
#include <SFML/Graphics.hpp>
#include "Components.h"
#include "ECSManagerAsync.h"

class GameMovement
{
public:
	std::future<void> Update();
};

class GraphicSystem
{
public:
	std::future<void> Update();
	void RenderSync();
};

struct SResources
{
	sf::RenderWindow window;
	ECS::ECSManagerAsync ecs;
	GraphicSystem graphic_system;
	GameMovement movement_system;

	float frame_time_seconds = 0.0f;

	std::promise<std::future<void>> main_thread_triggered_graphic_update; //is this legal ?
	std::promise<void> render_thread_done_sync;

	static SResources* inst;
};

struct EStreams
{
	constexpr static const ECS::ExecutionStreamId None{};
	constexpr static const ECS::ExecutionStreamId Graphic{0};
};

void HandleGameEvents()
{

}

std::future<void> GraphicSystem::Update()
{
	return SResources::inst->ecs.CallAsync(ToFunc([this](EntityId id
		, const Position& pos
		, const CircleSize& size
		, Sprite2D& sprite)
	{
		sprite.shape.setPosition(pos.pos - sf::Vector2f(size.radius, size.radius));
		if (sprite.shape.getRadius() != size.radius)
		{
			sprite.shape.setRadius(size.radius);
		}
	}), EStreams::Graphic
	LOG_PARAM("GraphicSystem::Update"));
}
void GraphicSystem::RenderSync()
{
	SResources::inst->ecs.Call(ToFunc([this](EntityId id
		, const Sprite2D& sprite)
	{
		SResources::inst->window.draw(sprite.shape);
	})
		//, EStreams::Graphic
		LOG_PARAM("GraphicSystem::Render"));
}

std::future<void> GameMovement::Update()
{
	return SResources::inst->ecs.CallAsync(ToFunc([this](EntityId id
		, Position& pos
		, Velocity& vel)
	{
		if ((pos.pos.x < 0 && vel.velocity.x < 0) || (pos.pos.x > 800 && vel.velocity.x > 0))
		{
			vel.velocity.x = -vel.velocity.x;
		}

		if ((pos.pos.y < 0 && vel.velocity.y < 0) || (pos.pos.y > 600 && vel.velocity.y > 0))
		{
			vel.velocity.y = -vel.velocity.y;
		}

		pos.pos += vel.velocity * 100.0f * SResources::inst->frame_time_seconds;

	}), EStreams::None
		LOG_PARAM("GameMovement::Update"));
}