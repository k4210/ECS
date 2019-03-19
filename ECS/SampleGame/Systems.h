#pragma once
#include <SFML/Graphics.hpp>
#include "Components.h"
#include "BaseGame/GameBase.h"

static QuadTree<ECS::EntityId>::Region ToRegion(const Position& pos, const CircleSize& size)
{
	const uint32_t kQuadPixelSize = 32;
	const float position_offset = 64;
	return QuadTree<ECS::EntityId>::Region{
		static_cast<uint8_t>((position_offset + pos.pos.x - size.radius) / kQuadPixelSize),
		static_cast<uint8_t>((position_offset + pos.pos.y - size.radius) / kQuadPixelSize),
		static_cast<uint8_t>(1 + ((position_offset + pos.pos.x + size.radius) / kQuadPixelSize)),
		static_cast<uint8_t>(1 + ((position_offset + pos.pos.y + size.radius) / kQuadPixelSize)) };
}

void GraphicSystem_Update(ECS::EntityId
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

void GraphicSystem_RenderSync(ECS::EntityId, const Sprite2D& sprite)
{
	BaseGameInstance::inst->window.draw(sprite.shape);
}

class OutOfBoardEvent : public ECS::IEvent
{
	ECS::EntityHandle entity;
public:
	void Execute()
	{
		auto& ecs = BaseGameInstance::inst->ecs;
		QuadTree<ECS::EntityId>::Region region = ToRegion(ecs.GetComponent<Position>(entity), ecs.GetComponent<CircleSize>(entity));
		BaseGameInstance::inst->quad_tree.Remove(entity, region);
		BaseGameInstance::inst->ecs.RemoveEntity(entity);
	}
	OutOfBoardEvent(ECS::EntityHandle eh) : entity(eh) {}
};

void GameMovement_Update(ECS::EntityId id
	, Position& pos
	, Velocity& vel
	, const CircleSize& size)
{
	if (	((pos.pos.x - size.radius) < 0	 && vel.velocity.x < 0)
		||	((pos.pos.x + size.radius) > 800 && vel.velocity.x > 0))
	{
		vel.velocity.x = -vel.velocity.x;
	}
	if(		((pos.pos.y - size.radius) < 0   && vel.velocity.y < 0)
		||	((pos.pos.y + size.radius) > 600 && vel.velocity.y > 0))
	{
		//const auto eh = GResource::inst->ecs.GetHandle(id);
		//GResource::inst->event_manager.Push(ECS::EventStorage::Create<OutOfBoardEvent>(eh));
		vel.velocity.y = -vel.velocity.y;
	}
	
	{
		auto& qt = BaseGameInstance::inst->quad_tree;
		qt.Remove(id, ToRegion(pos, size));
		const float scale_speed = 200.0f;
		pos.pos += vel.velocity * scale_speed * BaseGameInstance::inst->frame_time_seconds;
		qt.Add(id, ToRegion(pos, size));
	}
}

struct TestOverlap_Holder
{
	ECS::EntityId id;
	const Position& pos; 
	const CircleSize& size; 
	Velocity& vel;

	QuadTree<ECS::EntityId>::Region region;
	QuadTree<ECS::EntityId>::Iter GetIter(std::vector<uint8_t>& in_memory) const
	{
		assert(region.IsValid());
		return QuadTree<ECS::EntityId>::Iter(id, region, BaseGameInstance::inst->quad_tree, in_memory);
	}
};

TestOverlap_Holder TestOverlap_FirstPass(ECS::EntityId id, const Position& pos, const CircleSize& size, Velocity& vel)
{
	return TestOverlap_Holder{ id, pos, size, vel, ToRegion(pos, size) };
}

void TestOverlap_SecondPass(TestOverlap_Holder& first_pass, ECS::EntityId, const Position& pos, const CircleSize& size, Velocity& vel)
{
	const sf::Vector2f diff = (pos.pos - first_pass.pos.pos);
	const float dist_sq = diff.x * diff.x + diff.y * diff.y;
	const float radius_sum_sq = (first_pass.size.radius + size.radius) * (first_pass.size.radius + size.radius);
	
	const float update_time = 0.0001f;
	const sf::Vector2f next_diff = diff + (vel.velocity - first_pass.vel.velocity) * update_time;
	const float next_dist_sq = next_diff.x * next_diff.x + next_diff.y * next_diff.y;

	if ((dist_sq < radius_sum_sq) && (next_dist_sq < dist_sq))
	{
		std::swap(first_pass.vel.velocity, vel.velocity);
	}
}
