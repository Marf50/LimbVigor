#include "lv_parts.h"
#include "lv_config.h"
#include "lv_msvcstr.h"
#include "lv_sim.h"

#include <cstdio>
#include <cstring>

#if defined(LIMBVIGOR_IDE)
#include "stubs/kenshi_ide_stubs.h"
#else
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Debug.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Item.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/Enums.h>
#include <kenshi/util/hand.h>
#endif

// C++ try/catch — this file builds GameStr. Access violations are
// caught by the medicalUpdate hook SEH around DriveTick.
#define LV_TRY    try
#define LV_EXCEPT catch (...)

static const RobotLimbs::Limb kGameLimb[LIMB_COUNT] = {
    RobotLimbs::RIGHT_LEG,
    RobotLimbs::LEFT_LEG,
    RobotLimbs::RIGHT_ARM,
    RobotLimbs::LEFT_ARM
};

// FCS LimbSlot / RobotLimbs::Limb numbers.
static const int kFcsSlot[LIMB_COUNT] = {
    3, // RIGHT_LEG
    2, // LEFT_LEG
    1, // RIGHT_ARM
    0  // LEFT_ARM
};

static const char* kEconomyName[LIMB_COUNT] = {
    "Economy Leg (right)",
    "Economy Leg (left)",
    "Economy Arm (right)",
    "Economy Arm (left)"
};

// 4 limbs × 4 stages. Names start with "LV " so ReadLimb can tell
// a growing part from a real prosthetic.
static const LvPartDef kParts[LIMB_COUNT][LV_PART_COUNT] = {
    // RIGHT LEG
    {
        { "LV Stump Right Leg",    "lv-stump-r-leg", "A raw stump. Almost no push-off.",           "Economy Leg (right)", 3, 30.f, 0.15f, 0.20f, 0.10f, 1.f, 1.f, 1.f, 1.f, 0.4f },
        { "LV Budding Right Leg",  "lv-bud-r-leg",   "Flesh is budding on the stump.",             "Economy Leg (right)", 3, 45.f, 0.35f, 0.40f, 0.25f, 1.f, 1.f, 1.f, 1.f, 0.8f },
        { "LV Forming Right Leg",  "lv-form-r-leg",  "Bone and tendon are finding their shape.",   "Economy Leg (right)", 3, 65.f, 0.60f, 0.65f, 0.50f, 1.f, 1.f, 1.f, 1.f, 1.4f },
        { "LV Knitting Right Leg", "lv-knit-r-leg",  "Almost a leg. Soft. Do not kick anyone.",    "Economy Leg (right)", 3, 85.f, 0.85f, 0.85f, 0.75f, 1.f, 1.f, 1.f, 1.f, 2.0f },
    },
    // LEFT LEG
    {
        { "LV Stump Left Leg",    "lv-stump-l-leg", "A raw stump. Almost no push-off.",           "Economy Leg (left)", 2, 30.f, 0.15f, 0.20f, 0.10f, 1.f, 1.f, 1.f, 1.f, 0.4f },
        { "LV Budding Left Leg",  "lv-bud-l-leg",   "Flesh is budding on the stump.",             "Economy Leg (left)", 2, 45.f, 0.35f, 0.40f, 0.25f, 1.f, 1.f, 1.f, 1.f, 0.8f },
        { "LV Forming Left Leg",  "lv-form-l-leg",  "Bone and tendon are finding their shape.",   "Economy Leg (left)", 2, 65.f, 0.60f, 0.65f, 0.50f, 1.f, 1.f, 1.f, 1.f, 1.4f },
        { "LV Knitting Left Leg", "lv-knit-l-leg",  "Almost a leg. Soft. Do not kick anyone.",    "Economy Leg (left)", 2, 85.f, 0.85f, 0.85f, 0.75f, 1.f, 1.f, 1.f, 1.f, 2.0f },
    },
    // RIGHT ARM
    {
        { "LV Stump Right Arm",    "lv-stump-r-arm", "A raw stump. The hand is a memory.",         "Economy Arm (right)", 1, 30.f, 1.f, 1.f, 0.10f, 0.15f, 0.20f, 0.15f, 0.10f, 0.3f },
        { "LV Budding Right Arm",  "lv-bud-r-arm",   "Fingers are suggestions, not facts.",        "Economy Arm (right)", 1, 45.f, 1.f, 1.f, 0.25f, 0.35f, 0.40f, 0.30f, 0.25f, 0.6f },
        { "LV Forming Right Arm",  "lv-form-r-arm",  "A forearm you can almost trust.",            "Economy Arm (right)", 1, 65.f, 1.f, 1.f, 0.50f, 0.60f, 0.70f, 0.55f, 0.50f, 1.1f },
        { "LV Knitting Right Arm", "lv-knit-r-arm",  "Almost a hand. Soft. Do not make a fist.",   "Economy Arm (right)", 1, 85.f, 1.f, 1.f, 0.75f, 0.85f, 0.90f, 0.80f, 0.80f, 1.6f },
    },
    // LEFT ARM
    {
        { "LV Stump Left Arm",    "lv-stump-l-arm", "A raw stump. The hand is a memory.",         "Economy Arm (left)", 0, 30.f, 1.f, 1.f, 0.10f, 0.15f, 0.20f, 0.15f, 0.10f, 0.3f },
        { "LV Budding Left Arm",  "lv-bud-l-arm",   "Fingers are suggestions, not facts.",        "Economy Arm (left)", 0, 45.f, 1.f, 1.f, 0.25f, 0.35f, 0.40f, 0.30f, 0.25f, 0.6f },
        { "LV Forming Left Arm",  "lv-form-l-arm",  "A forearm you can almost trust.",            "Economy Arm (left)", 0, 65.f, 1.f, 1.f, 0.50f, 0.60f, 0.70f, 0.55f, 0.50f, 1.1f },
        { "LV Knitting Left Arm", "lv-knit-l-arm",  "Almost a hand. Soft. Do not make a fist.",   "Economy Arm (left)", 0, 85.f, 1.f, 1.f, 0.75f, 0.85f, 0.90f, 0.80f, 0.80f, 1.6f },
    },
};

const LvPartDef* LvPartFor(int limbId, int stage)
{
    if (limbId < 0 || limbId >= LIMB_COUNT) return nullptr;
    if (stage < 0 || stage >= LV_PART_COUNT) return nullptr;
    return &kParts[limbId][stage];
}

const char* LvPartStageName(int stage)
{
    switch (stage)
    {
    case LV_PART_STUMP:    return "stump";
    case LV_PART_BUDDING:  return "budding";
    case LV_PART_FORMING:  return "forming";
    case LV_PART_KNITTING: return "knitting";
    default: return "limb";
    }
}

int LvPartStageFromProgress(float progress)
{
    if (progress < 0.f) return LV_PART_STUMP;
    if (progress < 25.f) return LV_PART_STUMP;
    if (progress < 50.f) return LV_PART_BUDDING;
    if (progress < 75.f) return LV_PART_FORMING;
    if (progress < 100.f) return LV_PART_KNITTING;
    return LV_PART_KNITTING;
}

int LvPartSlotForLimb(int limbId)
{
    if (limbId < 0 || limbId >= LIMB_COUNT) return -1;
    return kFcsSlot[limbId];
}

#if defined(LIMBVIGOR_IDE)

int  LvIsGrowthPart(Item*) { return 0; }
int  LvGrowthPartStage(Item*) { return -1; }
int  LvEquipGrowthPart(MedicalSystem*, int, int) { return 0; }
void LvClearGrowthPart(MedicalSystem*, int) {}
void LvSyncGrowthParts(MedicalSystem*, const CharSnap*) {}

#else

static std::string& GS(GameStr* s)
{
    return *reinterpret_cast<std::string*>(s);
}

static GameData* ItemData(Item* item)
{
    if (!item) return nullptr;
    GameData* data = nullptr;
    LV_TRY { std::memcpy(&data, (const char*)(const void*)item + 0x10, sizeof(data)); }
    LV_EXCEPT { data = nullptr; }
    return data;
}

static int NameLooksLikeOurs(const char* s)
{
    if (!s || !s[0]) return 0;
    if (s[0] == 'L' && s[1] == 'V' && s[2] == ' ') return 1;
    if (s[0] == 'l' && s[1] == 'v' && s[2] == '-') return 1;
    return 0;
}

static int DataLooksLikeOurs(GameData* data)
{
    if (!data) return 0;
    const char* base = (const char*)(const void*)data;
    if (GameStrContainsI(base + 0x28, "lv ")) return 1;
    if (GameStrContainsI(base + 0x58, "lv-")) return 1;
    return 0;
}

int LvIsGrowthPart(Item* item)
{
    return DataLooksLikeOurs(ItemData(item)) ? 1 : 0;
}

int LvGrowthPartStage(Item* item)
{
    GameData* data = ItemData(item);
    if (!data) return -1;
    const char* base = (const char*)(const void*)data;
    char name[80] = {};
    if (!GameStrRead(base + 0x28, name, (int)sizeof(name)))
        GameStrRead(base + 0x58, name, (int)sizeof(name));
    if (!NameLooksLikeOurs(name) && !DataLooksLikeOurs(data)) return -1;
    if (std::strstr(name, "tump") || std::strstr(name, "stump")) return LV_PART_STUMP;
    if (std::strstr(name, "ud") || std::strstr(name, "bud")) return LV_PART_BUDDING;
    if (std::strstr(name, "orm") || std::strstr(name, "form")) return LV_PART_FORMING;
    if (std::strstr(name, "nit") || std::strstr(name, "knit")) return LV_PART_KNITTING;
    return LV_PART_STUMP;
}

static Item* Equipped(MedicalSystem* med, int limbId)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return nullptr;
    RobotLimbs* robots = nullptr;
    LV_TRY { robots = med->robotLimbs; }
    LV_EXCEPT { robots = nullptr; }
    if (!robots) return nullptr;
    Item* it = nullptr;
    LV_TRY { it = robots->getLimb(kGameLimb[limbId]); }
    LV_EXCEPT { it = nullptr; }
    return it;
}

static void* ExeBase()
{
    HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
    if (!exe) exe = GetModuleHandleA("kenshi_GOG_x64.exe");
    return (void*)exe;
}

static GameData* LookupData(const char* stringId, const char* name)
{
    if (!ou) return nullptr;
    GameData* gd = nullptr;
    if (stringId && stringId[0])
    {
        GameStr sid;
        GameStrSet(&sid, stringId);
        LV_TRY { gd = ou->gamedata.getData(GS(&sid)); }
        LV_EXCEPT { gd = nullptr; }
        if (gd) return gd;
        LV_TRY { gd = ou->gamedata.getData(GS(&sid), LIMB_REPLACEMENT); }
        LV_EXCEPT { gd = nullptr; }
        if (gd) return gd;
    }
    if (name && name[0])
    {
        GameStr nm;
        GameStrSet(&nm, name);
        LV_TRY { gd = ou->gamedata.getDataByName(GS(&nm), LIMB_REPLACEMENT); }
        LV_EXCEPT { gd = nullptr; }
    }
    return gd;
}

static Item* MakeItem(GameData* gd)
{
    if (!gd || !ou || !ou->theFactory) return nullptr;
    Item* item = nullptr;
    void* base = ExeBase();
    const hand* nullHand = nullptr;
    if (base)
        nullHand = (const hand*)((unsigned char*)base + 0x1E375F8);
    LV_TRY
    {
        if (nullHand)
            item = ou->theFactory->createItem(gd, *nullHand, nullptr, nullptr, 0, nullptr);
        else
        {
            unsigned char buf[40];
            std::memset(buf, 0, sizeof(buf));
            item = ou->theFactory->createItem(gd, *reinterpret_cast<hand*>(buf), nullptr, nullptr, 0, nullptr);
        }
    }
    LV_EXCEPT { item = nullptr; }
    return item;
}

static int SlotPart(MedicalSystem* med, int limbId, Item* item)
{
    if (!med) return 0;
    const RobotLimbs::Limb limb = kGameLimb[limbId];
    int ok = 0;
    RobotLimbs* robots = nullptr;
    LV_TRY { robots = med->robotLimbs; }
    LV_EXCEPT { robots = nullptr; }
    if (robots)
    {
        LV_TRY
        {
            robots->setLimb(limb, item ? LIMB_REPLACED : LIMB_STUMP, item);
            ok = 1;
        }
        LV_EXCEPT { ok = 0; }
    }
    LV_TRY { med->setRobotLimbItem(limb, item, false); }
    LV_EXCEPT {}
    LV_TRY { med->validateHealthValues(); }
    LV_EXCEPT {}
    LV_TRY { med->updateStats(); }
    LV_EXCEPT {}
    return ok;
}

int LvEquipGrowthPart(MedicalSystem* med, int limbId, int stage)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return 0;
    if (stage < 0 || stage >= LV_PART_COUNT) return 0;

    const LvPartDef* def = LvPartFor(limbId, stage);
    if (!def) return 0;

    Item* cur = Equipped(med, limbId);
    if (cur && LvGrowthPartStage(cur) == stage)
        return 1;

    static unsigned failUntil[LIMB_COUNT] = {};
#if defined(_WIN32)
    unsigned now = GetTickCount();
    if (failUntil[limbId] && now < failUntil[limbId])
        return 0;
#else
    unsigned now = 0;
#endif

    if (!ou)
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: no GameWorld (ou) — cannot create growth parts"); once = 1; }
        return 0;
    }

    GameData* gd = LookupData(def->stringId, def->name);
    const char* src = "ours";
    if (!gd)
    {
        gd = LookupData(nullptr, def->vanillaVisual);
        src = "economy";
    }
    if (!gd)
    {
        gd = LookupData(nullptr, kEconomyName[limbId]);
        src = "economy-slot";
    }
    if (!gd)
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: no GameData for %s (or %s)", def->name, def->vanillaVisual);
        return 0;
    }

    Item* item = MakeItem(gd);
    if (!item)
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: createItem failed for %s", def->name);
        return 0;
    }

    if (!SlotPart(med, limbId, item))
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: setLimb REPLACED failed for %s", def->name);
        return 0;
    }

    failUntil[limbId] = 0;
    LvLogf("LimbVigor: slotted %s (%s) on %s",
        def->name, src, LvLimbLabel((LimbId)limbId));
    return 1;
}

void LvClearGrowthPart(MedicalSystem* med, int limbId)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return;
    Item* cur = Equipped(med, limbId);
    if (cur && !LvIsGrowthPart(cur))
        return;
    SlotPart(med, limbId, nullptr);
}

void LvSyncGrowthParts(MedicalSystem* med, const CharSnap* snap)
{
    if (!med || !snap) return;
    if (snap->race == RACE_SKELETON || snap->race == RACE_ANIMAL) return;
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        if (snap->limbs[i] != LIMB_KIND_STUMP && snap->limbs[i] != LIMB_KIND_CRUSHED)
            continue;
        const int stage = LvPartStageFromProgress(snap->progress[i]);
        LvEquipGrowthPart(med, i, stage);
    }
}

#endif
