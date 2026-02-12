#include "playerbot/playerbot.h"
#include "MoltenCoreDungeonTriggers.h"

#include "playerbot/strategy/AiObjectContext.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

using namespace ai;

namespace
{
    bool IsInRagnarosLair(Player* bot)
    {
        if (!bot || bot->GetMapId() != 409)
            return false;

        // Majordomo's post-teleport spawn is near Ragnaros platform.
        static constexpr float centerX = 848.933f;
        static constexpr float centerY = -812.875f;
        static constexpr float lairRadius = 130.0f;
        return bot->GetDistance2d(centerX, centerY) <= lairRadius;
    }
}

bool RagnarosSpreadRequiredTrigger::IsActive()
{
    if (!bot->IsInWorld() || bot->IsBeingTeleported() || !ai->IsStateActive(BotState::BOT_STATE_COMBAT))
        return false;

    return IsInRagnarosLair(bot);
}

bool RagnarosPreSpreadRequiredTrigger::IsActive()
{
    if (!bot->IsInWorld() || bot->IsBeingTeleported() || ai->IsStateActive(BotState::BOT_STATE_COMBAT))
        return false;

    return IsInRagnarosLair(bot);
}