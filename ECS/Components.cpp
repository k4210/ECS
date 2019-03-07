#include "Components.h"

IMPLEMENT_COMPONENT(EntityDebugName);
IMPLEMENT_EMPTY_COMPONENT(StaticActorTag);
IMPLEMENT_EMPTY_COMPONENT(EnemyCharacterTag);
IMPLEMENT_EMPTY_COMPONENT(MissileTag);
IMPLEMENT_COMPONENT(Position);
IMPLEMENT_COMPONENT(CircleSize);
IMPLEMENT_COMPONENT(Rotation);
IMPLEMENT_COMPONENT(Velocity);
IMPLEMENT_COMPONENT(Sprite2D);
IMPLEMENT_COMPONENT(Animation);
IMPLEMENT_COMPONENT(Damage);
IMPLEMENT_COMPONENT(LifeTime);

#include "ECSStat.h"

#if ECS_STAT_ENABLED
const char* StatIdToStr(StatId id)
{
	switch (id)
	{
	case StatId::FindTaskToExecute: return "FindTaskToExecute";
	case StatId::Graphic_WaitForUpdate: return "Graphic_WaitForUpdate";
	case StatId::Graphic_RenderSync: return "Graphic_RenderSync";
	case StatId::Graphic_Update: return "Graphic_Update";
	case StatId::Graphic_WaitForRenderSync: return "Graphic_WaitForRenderSync";
	case StatId::Display: return "Display";
	case StatId::GameMovement_Update: return "GameMovement_Update";
	case StatId::GameFrame: return "GameFrame";
	}
	return "unknown";
}

Stat::TRecords Stat::records{};
#endif //ECS_STAT_ENABLED