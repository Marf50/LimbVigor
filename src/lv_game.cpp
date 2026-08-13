#include "lv_game.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_parts.h"
#include "lv_msvcstr.h"

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
#include <core/Functions.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/RaceData.h>
#include <kenshi/GameData.h>
#include <kenshi/Enums.h>
#include <kenshi/Item.h>
#include <kenshi/gui/DatapanelGUI.h>
#endif

#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
#define LV_TRY    __try
#define LV_EXCEPT __except (1)
#else
#define LV_TRY    if (true)
#define LV_EXCEPT if (false)
#endif

// Official documented RVAs from KenshiLib headers (not guessed).
// Used only when GetRealAddress cannot pick an overload.
static const intptr_t kRvaSetLineProg = 0x6FCF00;

static const RobotLimbs::Limb kGameLimb[LIMB_COUNT] = {
    RobotLimbs::RIGHT_LEG,
    RobotLimbs::LEFT_LEG,
    RobotLimbs::RIGHT_ARM,
    RobotLimbs::LEFT_ARM
};

typedef void* (*FnSetLineProg)(DatapanelGUI*, const GameStr*, int, float, const GameStr*, bool);

static FnSetLineProg g_setLineProg = nullptr;
static int           g_gameReady   = 0;

static void* ExeBase()
{
#if defined(LIMBVIGOR_IDE)
    return nullptr;
#else
    HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
    if (!exe) exe = GetModuleHandleA("kenshi_GOG_x64.exe");
    return (void*)exe;
#endif
}

void LvResolvePluginDirFromSelf()
{
#if defined(LIMBVIGOR_IDE)
    LvSetPluginDir("");
#else
    HMODULE mod = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&LvResolvePluginDirFromSelf, &mod) || !mod)
    {
        LvSetPluginDir("");
        return;
    }
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(mod, path, MAX_PATH);
    if (!n || n >= MAX_PATH) { LvSetPluginDir(""); return; }
    char* slash = nullptr;
    for (char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/') slash = p;
    if (slash) *slash = 0;
    LvSetPluginDir(path);
#endif
}

void LvGameInit()
{
    if (g_gameReady) return;
    g_gameReady = 1;

#if !defined(LIMBVIGOR_IDE)
    intptr_t prog = KenshiLib::GetRealAddress(&DatapanelGUI::setLineProgress);
    void* base = ExeBase();
    if (prog)
        g_setLineProg = (FnSetLineProg)prog;
    else if (base)
        g_setLineProg = (FnSetLineProg)((unsigned char*)base + kRvaSetLineProg);
    LvLog(g_setLineProg ? "LimbVigor: game helpers ready" : "LimbVigor: no setLineProgress — HUD will stay empty");
#endif
}

Character* LvCharFromMed(MedicalSystem* med)
{
    if (!med) return nullptr;
    Character* me = nullptr;
    LV_TRY { me = med->me; }
    LV_EXCEPT { me = nullptr; }
    return me;
}

int LvIsPlayerSquad(Character* me)
{
    if (!me) return 0;
    int yes = 0;
    LV_TRY { yes = me->isWithThePlayer() ? 1 : 0; }
    LV_EXCEPT { yes = 0; }
    if (yes) return 1;
    LV_TRY { yes = me->isPlayerCharacter() ? 1 : 0; }
    LV_EXCEPT { yes = 0; }
    return yes;
}

int LvReadMsvcString(const void* strObj, char* out, int outsz)
{
    return GameStrRead(strObj, out, outsz);
}

static int CharName(Character* me, char* out, int outsz)
{
    if (!me || !out) return 0;
    out[0] = 0;
    LV_TRY { return GameStrRead((const char*)(const void*)me + 0x18, out, outsz); }
    LV_EXCEPT { return 0; }
}

static RaceKind DetectRace(Character* me)
{
    if (!me) return RACE_HUMAN;
    LV_TRY
    {
        if (me->isAnimal()) return RACE_ANIMAL;
    }
    LV_EXCEPT {}

    RaceData* race = nullptr;
    LV_TRY { race = me->getRace(); }
    LV_EXCEPT { race = nullptr; }
    if (!race) return RACE_HUMAN;

    if (race->robot) return RACE_SKELETON;

    GameData* data = race->data;
    if (data)
    {
        const char* base = (const char*)(const void*)data;
        if (GameStrContainsI(base + 0x58, "skeleton") || GameStrContainsI(base + 0x28, "skeleton")
         || GameStrContainsI(base + 0x58, "soldierbot") || GameStrContainsI(base + 0x28, "p4 unit"))
            return RACE_SKELETON;
        if (GameStrContainsI(base + 0x58, "hive") || GameStrContainsI(base + 0x28, "hive")
         || GameStrContainsI(base + 0x58, "hiver") || GameStrContainsI(base + 0x28, "drone")
         || GameStrContainsI(base + 0x28, "prince"))
            return RACE_HIVE;
        if (GameStrContainsI(base + 0x58, "shek") || GameStrContainsI(base + 0x28, "shek"))
            return RACE_SHEK;
    }

    if (race->gigantic) return RACE_SHEK;
    if (race->noHats && (race->noShoes || race->noShirts)) return RACE_HIVE;
    return RACE_HUMAN;
}

static LimbKind ReadLimb(MedicalSystem* med, int slot)
{
    if (!med) return LIMB_KIND_WHOLE;
    LimbState st = LIMB_ORIGINAL;
    LV_TRY { st = med->getLimbState(kGameLimb[slot]); }
    LV_EXCEPT { st = LIMB_ORIGINAL; }

    MedicalSystem::HealthPartStatus* part = nullptr;
    LV_TRY { part = med->getPart(kGameLimb[slot]); }
    LV_EXCEPT { part = nullptr; }

    // Our growth parts occupy the socket like a prosthetic. Treat them as
    // a stump so the sim keeps growing instead of stopping.
    Item* worn = nullptr;
    LV_TRY
    {
        RobotLimbs* robots = med->robotLimbs;
        if (robots) worn = robots->getLimb(kGameLimb[slot]);
    }
    LV_EXCEPT { worn = nullptr; }
    if (worn && LvIsGrowthPart(worn))
        return LIMB_KIND_STUMP;

    if (part)
    {
        LV_TRY
        {
            if (part->isRobotic())
            {
                // Growth parts report robotic. Name-check already ran.
                return LIMB_KIND_PROSTHETIC;
            }
            LimbState ps = part->getRobotLimbState();
            if (ps == LIMB_REPLACED) return LIMB_KIND_PROSTHETIC;
            if (ps == LIMB_STUMP) return LIMB_KIND_STUMP;
            if (ps == LIMB_CRUSHED) return LIMB_KIND_CRUSHED;
        }
        LV_EXCEPT {}
    }

    if (st == LIMB_REPLACED) return LIMB_KIND_PROSTHETIC;
    if (st == LIMB_STUMP) return LIMB_KIND_STUMP;
    if (st == LIMB_CRUSHED) return LIMB_KIND_CRUSHED;
    return LIMB_KIND_WHOLE;
}

void LvReadSnap(MedicalSystem* med, CharSnap* io)
{
    if (!med || !io) return;
    Character* me = LvCharFromMed(med);
    if (me)
    {
        char nm[48] = {};
        CharName(me, nm, (int)sizeof(nm));
        if (nm[0]) std::snprintf(io->name, sizeof(io->name), "%s", nm);
        io->race = DetectRace(me);

        LV_TRY { io->inCombat = me->isInCombatMode(true, true) ? 1 : 0; }
        LV_EXCEPT { io->inCombat = 0; }
    }

    CharStats* stats = nullptr;
    LV_TRY { stats = med->stats; }
    LV_EXCEPT { stats = nullptr; }
    if (stats)
    {
        LV_TRY { io->toughness = stats->getStat(STAT_TOUGHNESS, true); }
        LV_EXCEPT { io->toughness = 0.f; }
        LV_TRY { io->medic = stats->getStat(STAT_MEDIC, true); }
        LV_EXCEPT { io->medic = 0.f; }
        if (io->toughness != io->toughness || io->toughness < 0.f) io->toughness = 0.f;
        if (io->medic != io->medic || io->medic < 0.f) io->medic = 0.f;
        if (io->toughness > 500.f) io->toughness = 500.f;
        if (io->medic > 500.f) io->medic = 500.f;
    }

    LV_TRY { io->blood = med->blood; }
    LV_EXCEPT { io->blood = 0.f; }
    LV_TRY { io->maxBlood = med->getMaxBlood(); }
    LV_EXCEPT { io->maxBlood = 100.f; }
    LV_TRY { io->bleedRate = med->currentBleedRate; }
    LV_EXCEPT { io->bleedRate = 0.f; }
    if (io->bleedRate != io->bleedRate || io->bleedRate < 0.f) io->bleedRate = 0.f;

    int fed = 0, hungry = 0;
    LV_TRY { fed = med->isFed() ? 1 : 0; }
    LV_EXCEPT { fed = 0; }
    LV_TRY { hungry = med->isReallyHungry() ? 1 : 0; }
    LV_EXCEPT { hungry = 0; }
    io->fed = fed && !hungry ? 1 : 0;
    io->starving = hungry ? 1 : 0;

    float rest = 0.f;
    int fully = 0;
    LV_TRY { rest = med->restedState; }
    LV_EXCEPT { rest = 0.f; }
    LV_TRY { fully = med->isFullyRested() ? 1 : 0; }
    LV_EXCEPT { fully = 0; }
    io->inBed = (fully || rest > 0.15f) ? 1 : 0;

    for (int i = 0; i < LIMB_COUNT; ++i)
        io->limbs[i] = ReadLimb(med, i);
}

int LvRestoreLimb(MedicalSystem* med, int limbId)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return 0;
    const RobotLimbs::Limb limb = kGameLimb[limbId];

    LimbState before = LIMB_ORIGINAL;
    LV_TRY { before = med->getLimbState(limb); }
    LV_EXCEPT { before = LIMB_ORIGINAL; }

    MedicalSystem::HealthPartStatus* part = nullptr;
    LV_TRY { part = med->getPart(limb); }
    LV_EXCEPT { part = nullptr; }

    float flesh = 0.f;
    float mx = 100.f;
    int robotic = 0;
    LimbState partState = LIMB_ORIGINAL;
    if (part)
    {
        LV_TRY
        {
            mx = part->_maxHealth;
            flesh = part->flesh;
            robotic = part->isRobotic() ? 1 : 0;
            partState = part->getRobotLimbState();
        }
        LV_EXCEPT { flesh = 0.f; }
        if (mx < 1.f || mx > 10000.f) mx = 100.f;
    }

    LvLogf("LimbVigor: restore %s — state %d part %d flesh %.1f/%.0f robotic %d",
        LvLimbLabel((LimbId)limbId), (int)before, (int)partState, flesh, mx, robotic);

    Item* worn = nullptr;
    LV_TRY
    {
        RobotLimbs* robots = med->robotLimbs;
        if (robots) worn = robots->getLimb(limb);
    }
    LV_EXCEPT { worn = nullptr; }
    const int ours = worn && LvIsGrowthPart(worn) ? 1 : 0;

    if (robotic && partState == LIMB_REPLACED && !ours)
    {
        LvLogf("LimbVigor: refuse restore %s — prosthetic occupies the socket",
            LvLimbLabel((LimbId)limbId));
        return 0;
    }
    if (before == LIMB_REPLACED && !ours)
    {
        LvLogf("LimbVigor: refuse restore %s — replaced", LvLimbLabel((LimbId)limbId));
        return 0;
    }

    // Attached healthy flesh — do not rip it off.
    if (flesh > mx * 0.08f && before == LIMB_ORIGINAL && partState == LIMB_ORIGINAL)
        return 1;

    const float start = (mx * LvCfg().restoredFlesh > 1.f)
        ? mx * LvCfg().restoredFlesh
        : mx * 0.22f;

    if (part)
    {
        LV_TRY
        {
            part->flesh = start;
            part->updateDerivedHealths();
        }
        LV_EXCEPT {}
    }

    int ok = 0;
    if (ours || before == LIMB_STUMP || before == LIMB_CRUSHED
     || partState == LIMB_STUMP || partState == LIMB_CRUSHED
     || partState == LIMB_REPLACED)
    {
        if (ours)
            LvClearGrowthPart(med, limbId);

        RobotLimbs* robots = nullptr;
        LV_TRY { robots = med->robotLimbs; }
        LV_EXCEPT { robots = nullptr; }
        if (robots)
        {
            LV_TRY
            {
                robots->setLimb(limb, LIMB_ORIGINAL, nullptr);
                ok = 1;
            }
            LV_EXCEPT { ok = 0; }
        }
        LV_TRY { med->setRobotLimbItem(limb, nullptr, true); }
        LV_EXCEPT {}
    }
    else
    {
        ok = 1;
    }

    if (part)
    {
        LV_TRY
        {
            part->flesh = start;
            part->updateDerivedHealths();
        }
        LV_EXCEPT {}
    }

    LV_TRY { med->validateHealthValues(); }
    LV_EXCEPT {}
    LV_TRY { med->updateStats(); }
    LV_EXCEPT {}

    LimbState after = before;
    float afterFlesh = flesh;
    LV_TRY { after = med->getLimbState(limb); }
    LV_EXCEPT {}
    if (part)
    {
        LV_TRY { afterFlesh = part->flesh; }
        LV_EXCEPT {}
    }
    LvLogf("LimbVigor: restore %s after — state %d flesh %.1f (wrote %.1f)",
        LvLimbLabel((LimbId)limbId), (int)after, afterFlesh, start);

    if (after == LIMB_STUMP || after == LIMB_CRUSHED)
        return 0;
    if (afterFlesh > 0.f)
        return 1;
    return ok;
}

int LvHasSplint(Character* me)
{
    (void)me;
    return 0;
}

int LvItemLooksLikeCatalyst(Item* item)
{
    if (!item) return 0;
    GameData* data = nullptr;
    LV_TRY { std::memcpy(&data, (const char*)(const void*)item + 0x10, sizeof(data)); }
    LV_EXCEPT { data = nullptr; }
    if (!data) return 0;
    const char* base = (const char*)(const void*)data;
    if (GameStrContainsI(base + 0x28, "splint")
     || GameStrContainsI(base + 0x28, "stimulant")
     || GameStrContainsI(base + 0x28, "ichor")
     || GameStrContainsI(base + 0x28, "regrowth")
     || GameStrContainsI(base + 0x28, "growth kit")
     || GameStrContainsI(base + 0x28, "bone-knit")
     || GameStrContainsI(base + 0x28, "marrow")
     || GameStrContainsI(base + 0x58, "splint"))
        return 1;
    return 0;
}

void LvSay(Character* me, const char* text)
{
    (void)me;
    if (text && text[0]) LvLog(text);
}

static void AddTextLine(DatapanelGUI* panel, int cat, const char* name, const char* value)
{
    if (!g_setLineProg || !panel || !name) return;
    GameStr key, right;
    GameStrSet(&key, name);
    GameStrSet(&right, value ? value : "");
    LV_TRY { g_setLineProg(panel, &key, cat, 0.f, &right, false); }
    LV_EXCEPT {}
}

void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap)
{
    if (!med || !panel || !snap || !LvCfg().enableHud) return;
    if (!g_setLineProg) return;

    const int cat = 0;
    GameStr key, right;

    if (snap->race == RACE_SKELETON)
    {
        GameStrSet(&key, "Limb Vigor");
        GameStrSet(&right, "Frames do not grow flesh.");
        LV_TRY { g_setLineProg(panel, &key, cat, 0.f, &right, false); }
        LV_EXCEPT {}
        AddTextLine(panel, cat, "How", LvRaceHint(RACE_SKELETON));
        return;
    }

    if (snap->race == RACE_ANIMAL) return;

    const char* res = LvResourceName(snap->race);
    char buf[112];
    const float fill = (LvCfg().maxVigor > 0.f) ? (snap->vigor / LvCfg().maxVigor) : 0.f;
    std::snprintf(buf, sizeof(buf), "%.0f / %.0f", snap->vigor, LvCfg().maxVigor);
    GameStrSet(&key, res && res[0] ? res : "Vigor");
    GameStrSet(&right, buf);
    LV_TRY { g_setLineProg(panel, &key, cat, fill, &right, true); }
    LV_EXCEPT {}

    const int stump = LvFirstStump(snap);
    if (stump >= 0)
    {
        char why[96];
        const int ok = LvEligible(snap, why, (int)sizeof(why));
        if (!ok)
            std::snprintf(buf, sizeof(buf), "%s — %s", LvLimbLabel((LimbId)stump), why);
        else
            std::snprintf(buf, sizeof(buf), "%s  %s  %.0f%%",
                LvLimbLabel((LimbId)stump),
                LvStageName(snap->progress[stump]),
                snap->progress[stump]);
        GameStrSet(&key, "Regrowth");
        GameStrSet(&right, buf);
        const float p = ok ? (snap->progress[stump] / 100.f) : 0.f;
        LV_TRY { g_setLineProg(panel, &key, cat, p, &right, false); }
        LV_EXCEPT {}

        char eta[96];
        LvEtaText(snap, eta, (int)sizeof(eta));
        AddTextLine(panel, cat, "Time", eta);
        AddTextLine(panel, cat, "How", LvRaceHint(snap->race));
    }
    else if (snap->catalystHours > 0.f)
    {
        std::snprintf(buf, sizeof(buf), "splint  %.0fh left", snap->catalystHours);
        GameStrSet(&key, "Regrowth");
        GameStrSet(&right, buf);
        LV_TRY { g_setLineProg(panel, &key, cat, 0.f, &right, false); }
        LV_EXCEPT {}
        AddTextLine(panel, cat, "How", LvRaceHint(snap->race));
    }
    else
    {
        AddTextLine(panel, cat, "How", LvRaceHint(snap->race));
    }
}
