#pragma once

#include <SFML/Graphics.hpp>
#include "ECSManagerAsync.h"
#include "ECSEvent.h"

struct GResource
{
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

struct EStreams
{
	constexpr static const ECS::ExecutionStreamId None{};
	constexpr static const ECS::ExecutionStreamId Graphic{ 0 };
};

enum class EStatId : int
{
	Graphic_Update,
	Graphic_WaitForUpdate,
	Graphic_RenderSync,
	Graphic_WaitForRenderSync,
	Display,
	GameMovement_Update,
	GameFrame,
	Count
};