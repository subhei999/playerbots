#include "playerbot/playerbot.h"
#include "MoltenCoreDungeonActions.h"

#include "playerbot/strategy/AiObjectContext.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

#include <algorithm>
#include <cmath>

using namespace ai;

namespace
{
    bool IsInRagnarosLair(Player* bot)
    {
        if (!bot || bot->GetMapId() != 409)
            return false;

        static constexpr float centerX = 848.933f;
        static constexpr float centerY = -812.875f;
        static constexpr float lairRadius = 130.0f;
        return bot->GetDistance2d(centerX, centerY) <= lairRadius;
    }

    struct RagSpot
    {
        float x;
        float y;
        float z;
    };

    // Curated safe coordinates from GM .gps capture.
    static const RagSpot kRagnarosSpots[] =
    {
        {882.707581f, -823.524841f, -227.548584f},
        {886.102478f, -830.004028f, -227.920883f},
        {879.544678f, -817.340698f, -227.825851f},
        {875.923340f, -808.179688f, -226.705032f},
        {875.693726f, -800.346436f, -227.575653f},
        {870.938110f, -795.814087f, -228.054031f},
        {865.312866f, -790.219238f, -226.254166f},
        {860.577454f, -785.064026f, -226.191040f},
        {851.177856f, -790.485535f, -226.571243f},
        {844.409058f, -787.667175f, -227.892822f},
        {836.298218f, -788.105469f, -227.487610f},
        {827.440308f, -789.864807f, -226.723160f},
        {819.457153f, -791.796143f, -226.137817f},
        {813.438782f, -797.013062f, -225.848557f},
        {807.276184f, -802.997925f, -226.168106f},
        {800.283691f, -810.813538f, -226.269409f},
        {799.777283f, -820.983765f, -227.532562f},
        {800.093689f, -829.243103f, -229.211151f},
        {803.166748f, -838.385010f, -229.016846f},
        {809.273621f, -845.643799f, -229.057678f},
        {819.977051f, -838.736450f, -229.937759f},
        {829.253479f, -843.750427f, -229.752151f},
        {840.190613f, -848.324951f, -229.109039f},
        {850.115173f, -845.582092f, -228.800262f},
        {852.340698f, -837.985596f, -229.054810f},
        {851.928223f, -827.453369f, -229.057159f},
        {845.930237f, -818.538513f, -229.781525f},
        {835.664429f, -812.042236f, -229.110611f},
        {848.879028f, -809.365234f, -229.478058f},
        {855.575500f, -817.914368f, -229.060089f},
        {860.171204f, -828.010803f, -228.717422f},
        {861.890869f, -840.448120f, -228.172592f},
        {861.742126f, -851.295532f, -228.343582f},
        {854.977783f, -861.470154f, -228.601410f},
        {845.932129f, -865.255188f, -228.950668f},
        {833.721741f, -865.240051f, -229.208649f},
        {822.293640f, -862.713806f, -229.963745f},
        {814.167542f, -856.493530f, -227.890427f},
        {802.453613f, -852.476685f, -227.603119f},
        {794.875488f, -844.220215f, -228.038940f},
        {790.963013f, -829.626770f, -227.572083f},
        {789.678955f, -819.689941f, -226.459290f},
        {792.082520f, -806.881042f, -226.056473f},
        {798.772888f, -797.387695f, -225.985001f},
        {807.307190f, -789.964905f, -225.877899f},
        {816.849792f, -782.634705f, -225.857697f},
        {826.214722f, -775.440979f, -225.467300f},
        {836.897400f, -773.717407f, -225.562851f},
        {845.938599f, -774.989380f, -226.057816f},
        {857.572144f, -776.652954f, -226.324326f}
    };
}

bool SpreadAroundRagnarosAction::Execute(Event& event)
{
    if (!IsInRagnarosLair(bot))
        return false;

    static constexpr float centerX = 848.933f;
    static constexpr float centerY = -812.875f;
    static constexpr float lairRadius = 130.0f;
    std::vector<ObjectGuid> participants;
    std::list<Unit*> nearbyPlayers;
    MaNGOS::AnyUnitInObjectRangeCheck u_check(bot, lairRadius);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(nearbyPlayers, u_check);
    Cell::VisitAllObjects(bot, searcher, lairRadius);
    for (Unit* unit : nearbyPlayers)
    {
        if (!unit || unit->GetTypeId() != TYPEID_PLAYER || !unit->IsAlive())
            continue;

        const float dx = unit->GetPositionX() - centerX;
        const float dy = unit->GetPositionY() - centerY;
        if ((dx * dx + dy * dy) > (lairRadius * lairRadius))
            continue;

        participants.push_back(unit->GetObjectGuid());
    }

    participants.push_back(bot->GetObjectGuid());
    std::sort(participants.begin(), participants.end(),
        [](const ObjectGuid& a, const ObjectGuid& b) { return a.GetRawValue() < b.GetRawValue(); });
    participants.erase(std::unique(participants.begin(), participants.end()), participants.end());

    uint32 myIndex = 0;
    for (uint32 i = 0; i < participants.size(); ++i)
    {
        if (participants[i] == bot->GetObjectGuid())
        {
            myIndex = i;
            break;
        }
    }

    constexpr uint32 spotCount = sizeof(kRagnarosSpots) / sizeof(kRagnarosSpots[0]);
    if (!spotCount)
        return false;

    const RagSpot& spot = kRagnarosSpots[myIndex % spotCount];
    float targetX = spot.x;
    float targetY = spot.y;
    float targetZ = spot.z;

    if (bot->GetDistance(targetX, targetY, targetZ) < 2.5f)
        return false;

    if (!bot->IsWithinLOS(targetX, targetY, targetZ + bot->GetCollisionHeight()))
        return false;

    return MoveTo(bot->GetMapId(), targetX, targetY, targetZ, false, IsReaction(), false, true);
}