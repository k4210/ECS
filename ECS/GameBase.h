#pragma once

#include <SFML/Graphics.hpp>
#include "Components.h"
#include "ECSManagerAsync.h"
#include "ECSEvent.h"

struct QuadTree
{
	constexpr static const int kQuadPixelSize = 32;
	constexpr static const int kMaxElementsPerQuad = 6;


	struct Region
	{
		uint8_t min_x = -1;
		uint8_t min_y = -1;
		uint8_t max_x = -1;
		uint8_t max_y = -1;
	};

	static Region ToRegion(const Position& pos, const CircleSize& size)
	{
		return Region{
			static_cast<uint8_t>((pos.pos.x - size.radius) / kQuadPixelSize),
			static_cast<uint8_t>((pos.pos.y - size.radius) / kQuadPixelSize),
			static_cast<uint8_t>((pos.pos.x + size.radius) / kQuadPixelSize),
			static_cast<uint8_t>((pos.pos.y + size.radius) / kQuadPixelSize)};
	}

	struct Iter
	{
	private:
		ECS::EntityId* id = nullptr;
	public:
		operator bool() const { return id; }
		const ECS::EntityId& operator*() const { return *id; }
		void operator++() { }
		void operator++(int)
		{
			operator++();
		}
	};

	Iter GetIter(ECS::EntityId id, const Position& pos, const CircleSize& size) const
	{
		return Iter{};
	}
};

struct GResource
{
	QuadTree quad_tree;
	ECS::ECSManagerAsync ecs;
	ECS::EventManager event_manager;

	ECS::ThreadGate wait_for_graphic_update;
	ECS::ThreadGate wait_for_render_sync;

	sf::RenderWindow window;

	float frame_time_seconds = 0.0f;
	uint64_t frames = 0;

	std::atomic_bool close_request = false;

	static GResource* inst;
};

struct EExecutionNode
{
	constexpr static const ECS::ExecutionNodeId Graphic_Update{ 0 };
	constexpr static const ECS::ExecutionNodeId Movement_Update{ 1 };
	constexpr static const ECS::ExecutionNodeId TestOverlap{ 2 };
};

enum class EStatId : int
{
	Graphic_WaitForUpdate = EExecutionNode::TestOverlap.GetIndex(),
	Graphic_RenderSync,
	Graphic_WaitForRenderSync,
	Display,
	GameFrame,
	Count
};