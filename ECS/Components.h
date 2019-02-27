#pragma once
#include "ECSContainer.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics.hpp>

using namespace ECS;

//DEBUG
struct EntityDebugName : public Component<__COUNTER__, DenseComponentContainer<EntityDebugName>>
{
	std::string name;
};
static_assert(0 == EntityDebugName::kComponentTypeIdx, "wrong counter");

//TAGS
struct StaticActorTag : public EmptyComponent<__COUNTER__> {}; 
struct EnemyCharacterTag : public EmptyComponent<__COUNTER__>{};
struct MissileTag : public EmptyComponent<__COUNTER__> {};

//BASE
struct Position : public Component<__COUNTER__, DenseComponentContainer<Position>>
{
	sf::Vector2f pos;
};

struct CircleSize : public Component<__COUNTER__, DenseComponentContainer<CircleSize>>
{
	float radius = 0;
};

struct Rotation : public Component<__COUNTER__, DenseComponentContainer<Rotation>>
{
	sf::Vector2f direction;
};

struct Velocity : public Component<__COUNTER__, DenseComponentContainer<Velocity>>
{
	sf::Vector2f velocity;
};

//GRAPHIC
struct Sprite2D : public Component<__COUNTER__, DenseComponentContainer<Sprite2D>>
{
	sf::CircleShape shape;
	//sf::Sprite sprite;
};

struct Animation : public Component<__COUNTER__, SortedComponentContainer<Animation, false /* no binary search */>>
{
	int current_frame = 0;
	float time = 0.0f;
};

//GAMEPLAY
struct Damage : public Component<__COUNTER__, SortedComponentContainer<Damage, false /* no binary search */>>
{
	float damage = 0.0f;
};

struct LifeTime : public Component<__COUNTER__, SortedComponentContainer<LifeTime, false /* no binary search */>>
{
	float time = 0.0f;
};