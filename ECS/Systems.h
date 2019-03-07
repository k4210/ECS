#pragma once
#include <SFML/Graphics.hpp>
#include "Components.h"
#include "GameBase.h"

void GraphicSystem_Update(ECS::EntityId id
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

void GraphicSystem_RenderSync(ECS::EntityId id, const Sprite2D& sprite)
{
	GResource::inst->window.draw(sprite.shape);
}

class OutOfBoardEvent : public ECS::IEvent
{
	ECS::EntityHandle entity;
public:
	void Execute()
	{
		GResource::inst->ecs.RemoveEntity(entity);
	}
	OutOfBoardEvent(ECS::EntityHandle eh) : entity(eh) {}
};

void GameMovement_Update(ECS::EntityId id
	, Position& pos
	, Velocity& vel
	, const CircleSize& size)
{
	if ((	(pos.pos.x + size.radius) < 0	&& vel.velocity.x < 0)
		|| ((pos.pos.x - size.radius) > 800 && vel.velocity.x > 0)
		|| ((pos.pos.y + size.radius) < 0   && vel.velocity.y < 0)
		|| ((pos.pos.y - size.radius) > 600 && vel.velocity.y > 0))
	{
		const auto eh = GResource::inst->ecs.GetHandle(id);
		GResource::inst->event_manager.Push(ECS::EventStorage::Create<OutOfBoardEvent>(eh));
	}
	else
	{
		pos.pos += vel.velocity * 100.0f * GResource::inst->frame_time_seconds;
	}
}
