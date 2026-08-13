#pragma once

#include "lv_types.h"

// Growth stages as real limb parts — same idea as robot limbs:
// a named GameData item with stats, slotted into the socket.

enum LvPartStage
{
    LV_PART_STUMP = 0,
    LV_PART_BUDDING,
    LV_PART_FORMING,
    LV_PART_KNITTING,
    LV_PART_COUNT
};

struct LvPartDef
{
    const char* name;          // I-key slot title, "LV " prefix
    const char* stringId;      // GameData stringID
    const char* desc;
    const char* vanillaVisual; // Economy limb used if our .mod item is missing
    int         slot;          // RobotLimbs::Limb / FCS LimbSlot
    float       hp;
    float       athletics;
    float       stealth;
    float       swimming;
    float       dexterity;
    float       strength;
    float       combat;
    float       thievery;
    float       weight;
};

const LvPartDef* LvPartFor(int limbId, int stage);
const char*      LvPartStageName(int stage);
int              LvPartStageFromProgress(float progress);
int              LvPartSlotForLimb(int limbId);

#if defined(LIMBVIGOR_IDE)
struct MedicalSystem;
struct Item;
#else
class MedicalSystem;
class Item;
#endif

int  LvIsGrowthPart(Item* item);
int  LvGrowthPartStage(Item* item); // 0..3 or -1
int  LvEquipGrowthPart(MedicalSystem* med, int limbId, int stage);
void LvClearGrowthPart(MedicalSystem* med, int limbId);
void LvSyncGrowthParts(MedicalSystem* med, const CharSnap* snap);
