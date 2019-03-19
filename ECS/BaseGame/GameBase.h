#pragma once

#include <SFML/Graphics.hpp>
#include "ECS/ECSBase.h"
#include "ECS/ECSManagerAsync.h"
#include "ECS/ECSEvent.h"
#include "ECS/ECSStat.h"
#include "QuadTree.h"

template<typename T> bool IsValid(const T& v)
{
	return v.IsValidForm();
}

enum class EStatId : int
{
	Graphic_WaitForUpdate,
	Graphic_RenderSync,
	Graphic_WaitForRenderSync,
	Display,
	GameFrame,
	QuadTreeIteratorConstrucion,
	_Count
};

struct BaseGameInstance
{
	QuadTree<ECS::EntityId> quad_tree;
	ECS::ECSManagerAsync ecs;
	ECS::EventManager event_manager;

	ECS::ThreadGate wait_for_graphic_update;
	ECS::ThreadGate wait_for_render_sync;

	sf::RenderWindow window;

	float frame_time_seconds = 0.0f;
	uint64_t frames = 0;

	std::atomic_bool close_request = false;

	static BaseGameInstance* inst;
	static BaseGameInstance* CreateGameInstance();
	
	virtual void InitializeGame() {}
	virtual void DispatchTasks() {} // should open wait_for_graphic_update
	virtual void Render() {}
};
