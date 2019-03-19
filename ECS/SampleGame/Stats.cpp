#include "ECS/ECSStat.h"
#include "ECS/ECSManagerAsync.h"
#include "Game.h"

#if ECS_STAT_ENABLED
namespace
{
	using namespace ECS;
	static Stat::Register static_stat_register(3, EPredefinedStatGroups::ExecutionNode, [](uint32_t eid)
	{
		if (eid == EExecutionNode::Graphic_Update.GetIndex()) return "Graphic_Update";
		if (eid == EExecutionNode::Movement_Update.GetIndex()) return "Movement_Update";
		if (eid == EExecutionNode::TestOverlap.GetIndex()) return "TestOverlap";
		return "unknown";
	});
}
#endif //ECS_STAT_ENABLED