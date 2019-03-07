#pragma once
#include "ECSContainer.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics.hpp>

//DEBUG
struct EntityDebugName : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<EntityDebugName>>
{
	std::string name;
};
static_assert(0 == EntityDebugName::kComponentTypeIdx, "wrong counter");

//TAGS
struct StaticActorTag : public ECS::EmptyComponent<__COUNTER__> {};
struct EnemyCharacterTag : public ECS::EmptyComponent<__COUNTER__>{};
struct MissileTag : public ECS::EmptyComponent<__COUNTER__> {};

//BASE
struct Position : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<Position>>
{
	sf::Vector2f pos;
};

struct CircleSize : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<CircleSize>>
{
	float radius = 0;
};

struct Rotation : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<Rotation>>
{
	sf::Vector2f direction;
};

struct Velocity : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<Velocity>>
{
	sf::Vector2f velocity;
};

//GRAPHIC
struct Sprite2D : public ECS::Component<__COUNTER__, ECS::DenseComponentContainer<Sprite2D>>
{
	sf::CircleShape shape;
	//sf::Sprite sprite;
};

struct Animation : public ECS::Component<__COUNTER__, ECS::SortedComponentContainer<Animation, false /* no binary search */>>
{
	int current_frame = 0;
	float time = 0.0f;
};

//GAMEPLAY
struct Damage : public ECS::Component<__COUNTER__, ECS::SortedComponentContainer<Damage, false /* no binary search */>>
{
	float damage = 0.0f;
};

struct LifeTime : public ECS::Component<__COUNTER__, ECS::SortedComponentContainer<LifeTime, false /* no binary search */>>
{
	float time = 0.0f;
};