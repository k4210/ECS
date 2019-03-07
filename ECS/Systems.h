#pragma once
#include <SFML/Graphics.hpp>
#include "Components.h"
#include "ECSManagerAsync.h"
#include "ECSEvent.h"

struct SResources
{
	sf::RenderWindow window;
	ECS::ECSManagerAsync ecs;

	float frame_time_seconds = 0.0f;
	uint64_t frames = 0;

	ThreadGate wait_for_graphic_update;
	ThreadGate wait_for_render_sync;

	EventManager event_manager;

	std::atomic_bool close_request = false;

	static SResources* inst;
};

struct EStreams
{
	constexpr static const ECS::ExecutionStreamId None{};
	constexpr static const ECS::ExecutionStreamId Graphic{0};
};

void GraphicSystem_Update(EntityId id
	, const Position& pos
	, const CircleSize& size
	, Sprite2D& sprite)
{
	sprite.shape.setPosition(pos.pos - sf::Vector2f(size.radius, size.radius));
	if (sprite.shape.getRadius() != size.radius)
	{
		sprite.shape.setRadius(size.radius);
	}
}

void GraphicSystem_RenderSync(EntityId id, const Sprite2D& sprite)
{
	SResources::inst->window.draw(sprite.shape);
}

class OutOfBoardEvent : public IEvent
{
	EntityHandle entity;
public:
	void Execute()
	{
		if (SResources::inst->ecs.IsValidEntity(entity))
		{
			SResources::inst->ecs.RemoveEntity(entity);
		}
	}
	OutOfBoardEvent(EntityHandle eh) : entity(eh) {}
};

void GameMovement_Update(EntityId id
	, Position& pos
	, Velocity& vel
	, const CircleSize& size)
{
	if ((	(pos.pos.x + size.radius) < 0	&& vel.velocity.x < 0)
		|| ((pos.pos.x - size.radius) > 800 && vel.velocity.x > 0)
		|| ((pos.pos.y + size.radius) < 0   && vel.velocity.y < 0)
		|| ((pos.pos.y - size.radius) > 600 && vel.velocity.y > 0))
	{
		const auto eh = SResources::inst->ecs.GetHandle(id);
		SResources::inst->event_manager.Push(EventStorage::Create<OutOfBoardEvent>(eh));
	}
	else
	{
		pos.pos += vel.velocity * 100.0f * SResources::inst->frame_time_seconds;
	}
}
