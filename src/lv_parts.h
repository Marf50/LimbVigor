#pragma once

#include "lv_types.h"

// Growth stages as real limb parts — same idea as robot limbs:
// a named GameData item with stats, slotted into the socket.
// Grown (100%) IS the restored limb. We do not peel it for original flesh.

enum LvPartStage
{
    LV_PART_STUMP = 0,
    LV_PART_BUDDING,
    LV_PART_FORMING,
    LV_PART_KNITTING,
    LV_PART_GROWN,
    LV_PART_COUNT
};

struct LvPartDef
{
    const char* name;          // I-key slot title, "LV " prefix
    const char* stringId;      // GameData stringID
    const char* desc;
    const char* vanillaVisual; // Economy limb whose mesh/icon we reference — never createItem this
    const char* mesh;          // FileValue path (required)
    const char* icon;          // FileValue path (required)
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
int  LvGrowthPartStage(Item* item); // 0..4 or -1
/* First session write on a still-STUMP socket is always STUMP, even at
 * persist 100%. After nubWrote, one stage at a time. Never GROWN first. */
int  LvNubWriteStage(const CharSnap* snap, int limbId, int have);
int  LvEquipGrowthPart(MedicalSystem* med, int limbId, int stage, int alreadyWrote);
void LvClearGrowthPart(MedicalSystem* med, int limbId);
void LvSyncGrowthParts(MedicalSystem* med, CharSnap* snap);
int  LvSyncOneLimb(MedicalSystem* med, CharSnap* snap, int limbId);
