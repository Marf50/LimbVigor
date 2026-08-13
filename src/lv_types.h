#pragma once

// Shared types. No Kenshi headers. Keep in lockstep with the field-manual
// simulator (src/lib/vigor in the companion app).

enum RaceKind
{
    RACE_HUMAN = 0,
    RACE_SHEK,
    RACE_HIVE,
    RACE_SKELETON,
    RACE_ANIMAL
};

enum LimbId
{
    LIMB_RIGHT_LEG = 0,
    LIMB_LEFT_LEG,
    LIMB_RIGHT_ARM,
    LIMB_LEFT_ARM,
    LIMB_COUNT
};

enum LimbKind
{
    LIMB_KIND_WHOLE = 0,
    LIMB_KIND_STUMP,
    LIMB_KIND_PROSTHETIC,
    LIMB_KIND_CRUSHED
};

struct CharSnap
{
    RaceKind race;
    float    vigor;
    float    progress[LIMB_COUNT];
    int      lastStage[LIMB_COUNT];
    LimbKind limbs[LIMB_COUNT];
    float    toughness;
    float    medic;
    float    blood;
    float    maxBlood;
    float    bleedRate;
    int      fed;
    int      starving;
    int      inBed;
    int      inCombat;
    float    catalystHours;
    char     lastBlock[96];
    char     name[48];
};

struct TickResult
{
    int  grew;
    int  restored;          // LimbId or -1
    int  stageChanged;      // LimbId or -1
    int  stageValue;        // 0..4
    int  blockedNew;
    char speech[128];
};
