#include "ECSStat.h"
#include "GameBase.h"

#if ECS_STAT_ENABLED
using namespace ECS;

Stat::TRecords Stat::records(static_cast<int>(EStatId::Count) + 1);
const Stat::FStatToStr Stat::stat_to_str([](StatId eid)
{
	const EStatId id = static_cast<EStatId>(eid);
	switch (id)
	{
		case EStatId::Graphic_Update: return "Graphic_Update";
		case EStatId::Graphic_WaitForUpdate: return "Graphic_WaitForUpdate";
		case EStatId::Graphic_RenderSync: return "Graphic_RenderSync";
		case EStatId::Graphic_WaitForRenderSync: return "Graphic_WaitForRenderSync";
		case EStatId::Display: return "Display";
		case EStatId::GameMovement_Update: return "GameMovement_Update";
		case EStatId::GameFrame: return "GameFrame";
	}
	return "unknown";
});

#endif //ECS_STAT_ENABLED