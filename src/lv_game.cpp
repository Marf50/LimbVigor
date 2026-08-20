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
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdlib>

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
static const intptr_t kRvaLineExists  = 0x6FC100;
static const intptr_t kRvaNumLines    = 0x6F5FA0;
static const intptr_t kRvaLineByNum   = 0x6FBD30;
static const intptr_t kRvaRemoveLine  = 0x6FC1F0; // DatapanelGUI::removeLine
static const intptr_t kRvaGetMedPanel = 0x71FDA0; // MainBarGUI::getMedicalPanel
static const intptr_t kRvaSayLimit    = 0x5CA790; // Character::_NV_say_WithARepeatLimiter
static const intptr_t kRvaSay         = 0x5C91D0; // Character::_NV_say

static const RobotLimbs::Limb kGameLimb[LIMB_COUNT] = {
    RobotLimbs::RIGHT_LEG,
    RobotLimbs::LEFT_LEG,
    RobotLimbs::RIGHT_ARM,
    RobotLimbs::LEFT_ARM
};

typedef void* (*FnSetLineProg)(DatapanelGUI*, const GameStr*, int, float, const GameStr*, bool);
typedef int   (*FnLineExists)(DatapanelGUI*, const GameStr*, int);
typedef int   (*FnNumLines)(DatapanelGUI*, int);
typedef void* (*FnLineByNum)(DatapanelGUI*, int, int);
typedef void  (*FnRemoveLine)(DatapanelGUI*, const GameStr*, int);
typedef void* (*FnGetMedPanel)(void*);
typedef void  (*FnSay)(Character*, const GameStr*);

static FnSetLineProg  g_setLineProg = nullptr;
static FnLineExists   g_lineExists  = nullptr;
static FnNumLines     g_numLines    = nullptr;
static FnLineByNum    g_lineByNum   = nullptr;
static FnRemoveLine   g_removeLine  = nullptr;
static FnGetMedPanel  g_getMedPanel = nullptr;
static FnSay          g_say         = nullptr;
static int            g_gameReady   = 0;
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
    void* base = ExeBase();

    intptr_t prog = KenshiLib::GetRealAddress(&DatapanelGUI::setLineProgress);
    if (prog)
        g_setLineProg = (FnSetLineProg)prog;
    else if (base)
        g_setLineProg = (FnSetLineProg)((unsigned char*)base + kRvaSetLineProg);

    intptr_t exists = KenshiLib::GetRealAddress(&DatapanelGUI::lineExists);
    if (exists)
        g_lineExists = (FnLineExists)exists;
    else if (base)
        g_lineExists = (FnLineExists)((unsigned char*)base + kRvaLineExists);

    intptr_t nlines = KenshiLib::GetRealAddress(&DatapanelGUI::getNumLines);
    if (nlines)
        g_numLines = (FnNumLines)nlines;
    else if (base)
        g_numLines = (FnNumLines)((unsigned char*)base + kRvaNumLines);

    intptr_t bynum = KenshiLib::GetRealAddress(&DatapanelGUI::getLineByNum);
    if (bynum)
        g_lineByNum = (FnLineByNum)bynum;
    else if (base)
        g_lineByNum = (FnLineByNum)((unsigned char*)base + kRvaLineByNum);

    intptr_t rem = KenshiLib::GetRealAddress(&DatapanelGUI::removeLine);
    if (rem)
        g_removeLine = (FnRemoveLine)rem;
    else if (base)
        g_removeLine = (FnRemoveLine)((unsigned char*)base + kRvaRemoveLine);

    /* MainBarGUI::getMedicalPanel — RVA only. Never ctor 0x72C1E0.
     * v1.23: do not resolve or call _getWidget 0x723780 (v1.19 death, v1.21 HUD hide). */
    if (base)
        g_getMedPanel = (FnGetMedPanel)((unsigned char*)base + kRvaGetMedPanel);

    /* _NV_say only — never GetRealAddress on virtual Character::say. */
    intptr_t say = KenshiLib::GetRealAddress(&Character::_NV_say_WithARepeatLimiter);
    if (!say)
        say = KenshiLib::GetRealAddress(&Character::_NV_say);
    if (say)
        g_say = (FnSay)say;
    else if (base)
        g_say = (FnSay)((unsigned char*)base + kRvaSayLimit);
    if (!g_say && base)
        g_say = (FnSay)((unsigned char*)base + kRvaSay);

    if (g_setLineProg)
        LvLog("LimbVigor: setLineProgress resolved (not the HUD path)");
    LvLog("LimbVigor: _getWidget 0x723780 not bound — prefixed findWidget only");
    if (g_say)
        LvLog("LimbVigor: _NV_say resolved");
    else
        LvLog("LimbVigor: _NV_say missing — speech stays log-only");
#endif
}

static unsigned g_pulseMs = 0;
static unsigned g_medPulse = 0;
static int      g_loadProbe = 0; // 0 try, 1 live, -1 dead (do not block)
static intptr_t g_loadAddr = 0;
static int      g_gateLogged = 0;
static int      g_ignoredReset = 0;
static unsigned g_waitLogMs = 0;

void LvNoteMedicalPulse()
{
    g_medPulse++;
#if defined(LIMBVIGOR_IDE)
    if (!g_pulseMs) g_pulseMs = 1;
#else
    unsigned now = GetTickCount();
    if (!g_pulseMs)
    {
        g_pulseMs = now ? now : 1;
        LvLog("LimbVigor: medical tick (waiting for in-game)");
    }
#endif
}

static void LogGateOnce(const char* why)
{
    if (g_gateLogged) return;
    g_gateLogged = 1;
    LvLog(why);
}

int LvWorldInGame()
{
#if defined(LIMBVIGOR_IDE)
    return 1;
#else
    // Do not touch Character here. Title / save-load medical ticks
    // still fire; isWithThePlayer() on a half-built pawn is what
    // v1.9.0 did every frame.
    //
    // v1.9.2 never armed: raw RVA 0x784C40 excepted (SEH → still
    // loading forever) and/or ou->player was null after RE_Kenshi
    // printed In-game. Do not require player. A dead loading-probe
    // must not block forever.
    //
    // v1.9.4 never armed: gameResetting stayed true after RE_Kenshi
    // In-game. Ignore that flag after a short run of medical pulses.

    if (!ou)
    {
        LogGateOnce("LimbVigor: not in-game yet — no GameWorld (ou)");
        return 0;
    }

    int init = 0;
    int reset = 0;
    LV_TRY { init = ou->initialized ? 1 : 0; }
    LV_EXCEPT { init = 0; }
    LV_TRY { reset = ou->gameResetting ? 1 : 0; }
    LV_EXCEPT { reset = 0; }

    unsigned now = GetTickCount();
    unsigned age = (g_pulseMs && now >= g_pulseMs) ? now - g_pulseMs : 0;

    // v1.9.4: gameResetting stayed true after RE_Kenshi printed
    // In-game. Do not block forever once medical ticks are running.
    int stuckReset = 0;
    if (reset)
    {
        if (g_medPulse >= 6u || age >= 8000u)
            stuckReset = 1;
        else
        {
            LogGateOnce("LimbVigor: not in-game yet — game resetting");
            if (g_waitLogMs && now >= g_waitLogMs && (now - g_waitLogMs) >= 8000u)
            {
                g_waitLogMs = now;
                LvLog("LimbVigor: still waiting — gameResetting still set");
            }
            else if (!g_waitLogMs)
                g_waitLogMs = now ? now : 1;
            return 0;
        }
    }

    int loading = 0;
    if (g_loadProbe == 0)
    {
        int resolved = 0;
        LV_TRY
        {
            g_loadAddr = KenshiLib::GetRealAddress(&GameWorld::isLoadingFromASaveGame);
            resolved = 1;
        }
        LV_EXCEPT { g_loadAddr = 0; }
        if (!resolved || !g_loadAddr)
        {
            g_loadProbe = -1;
            LvLog("LimbVigor: isLoadingFromASaveGame not usable — not blocking on it");
        }
    }
    if (g_loadProbe != -1 && g_loadAddr)
    {
        typedef bool (*FnLoading)(GameWorld*);
        FnLoading fn = (FnLoading)g_loadAddr;
        int excepted = 0;
        LV_TRY { loading = fn(ou) ? 1 : 0; }
        LV_EXCEPT { excepted = 1; loading = 0; }
        if (excepted)
        {
            g_loadProbe = -1;
            LvLog("LimbVigor: isLoadingFromASaveGame probe excepted — ignoring (not still-loading)");
        }
        else
            g_loadProbe = 1;
    }
    if (g_loadProbe == 1 && loading)
    {
        LogGateOnce("LimbVigor: not in-game yet — save still loading");
        return 0;
    }

    if (stuckReset)
    {
        if (!g_ignoredReset)
        {
            g_ignoredReset = 1;
            LvLog("LimbVigor: In-game — ignoring stuck gameResetting");
        }
        return 1;
    }

    if (init)
    {
        /* Probe dead: wait a few medical pulses so DriveTick does not
         * touch Character on the load screen. Probe live + not loading
         * arms immediately. */
        if (g_loadProbe != 1 && g_medPulse < 4)
            return 0;
        return 1;
    }

    // Fallback: medical ticks have been running. RE_Kenshi already
    // printed In-game; do not sit silent because initialized read 0.
    if (g_pulseMs && age >= 8000u)
    {
        LogGateOnce("LimbVigor: in-game fallback — medical ticks while initialized unread");
        return 1;
    }

    LogGateOnce("LimbVigor: not in-game yet — GameWorld not initialized");
    return 0;
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

MedicalSystem* LvMedFromChar(Character* me)
{
    if (!me) return nullptr;
    MedicalSystem* med = nullptr;
    LV_TRY { med = me->getMedical(); }
    LV_EXCEPT { med = nullptr; }
    return med;
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

static const char* lvLimbStateName(LimbState st)
{
    if (st == LIMB_STUMP) return "STUMP";
    if (st == LIMB_REPLACED) return "REPLACED";
    if (st == LIMB_CRUSHED) return "CRUSHED";
    return "ORIGINAL";
}

static const char* lvLimbKindName(LimbKind k)
{
    if (k == LIMB_KIND_STUMP) return "stump";
    if (k == LIMB_KIND_PROSTHETIC) return "prosthetic";
    if (k == LIMB_KIND_CRUSHED) return "crushed";
    return "whole";
}

/* Named HealthPartStatus* first — getPart(Limb) has AVed and hidden stumps.
 * KenshiLib: leftLeg@0x80 flesh@0x40. Fall back to getPart if named HP is unreadable. */
static MedicalSystem::HealthPartStatus* lvNamedPart(MedicalSystem* med, int slot)
{
    MedicalSystem::HealthPartStatus* p = nullptr;
    if (!med)
        return nullptr;
    LV_TRY
    {
        if (slot == LIMB_RIGHT_LEG) p = med->rightLeg;
        else if (slot == LIMB_LEFT_LEG) p = med->leftLeg;
        else if (slot == LIMB_RIGHT_ARM) p = med->rightArm;
        else if (slot == LIMB_LEFT_ARM) p = med->leftArm;
    }
    LV_EXCEPT { p = nullptr; }
    return p;
}

static MedicalSystem::HealthPartStatus* lvPartByType(MedicalSystem* med, int slot)
{
    MedicalSystem::HealthPartStatus* p = nullptr;
    if (!med)
        return nullptr;
    const int leg = (slot == LIMB_RIGHT_LEG || slot == LIMB_LEFT_LEG) ? 1 : 0;
    const LeftRight side = (slot == LIMB_LEFT_LEG || slot == LIMB_LEFT_ARM)
        ? SIDE_LEFT : SIDE_RIGHT;
    LV_TRY
    {
        p = med->getPart(leg ? MedicalSystem::HealthPartStatus::PART_LEG
                             : MedicalSystem::HealthPartStatus::PART_ARM, side);
    }
    LV_EXCEPT { p = nullptr; }
    return p;
}

static int lvReadPartHp(MedicalSystem::HealthPartStatus* part, float* hp, float* mx)
{
    if (hp) *hp = 0.f;
    if (mx) *mx = 0.f;
    if (!part)
        return 0;
    float flesh = 0.f, maxH = 0.f;
    LV_TRY
    {
        flesh = part->flesh;
        maxH = part->_maxHealth;
    }
    LV_EXCEPT { return 0; }
    if (flesh != flesh) flesh = 0.f;
    if (maxH != maxH || maxH < 0.f) maxH = 0.f;
    if (hp) *hp = flesh;
    if (mx) *mx = maxH;
    return 1;
}

/* v1.29: official STUMP/CRUSHED, OR crippled (flesh<=0), OR cut-off nub
 * (Left Leg 5 / crippled). Intact 75-HP arms stay WHOLE. */
static LimbKind ReadLimbEx(MedicalSystem* med, int slot, float* hpOut, float* maxOut, char* why, int whyN)
{
    if (hpOut) *hpOut = 0.f;
    if (maxOut) *maxOut = 0.f;
    if (why && whyN > 0) why[0] = 0;
    if (!med)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "no medical");
        return LIMB_KIND_WHOLE;
    }

    LimbState st = LIMB_ORIGINAL;
    int stSeh = 0;
    LV_TRY { st = med->getLimbState(kGameLimb[slot]); }
    LV_EXCEPT { st = LIMB_ORIGINAL; stSeh = 1; }

    MedicalSystem::HealthPartStatus* part = lvNamedPart(med, slot);
    float hp = 0.f, mx = 0.f;
    int haveHp = lvReadPartHp(part, &hp, &mx);
    const char* hpSrc = haveHp ? "named" : "none";
    if (!haveHp)
    {
        MedicalSystem::HealthPartStatus* alt = nullptr;
        LV_TRY { alt = med->getPart(kGameLimb[slot]); }
        LV_EXCEPT { alt = nullptr; }
        if (lvReadPartHp(alt, &hp, &mx))
        {
            part = alt;
            haveHp = 1;
            hpSrc = "getPart(Limb)";
        }
        else
        {
            alt = lvPartByType(med, slot);
            if (lvReadPartHp(alt, &hp, &mx))
            {
                part = alt;
                haveHp = 1;
                hpSrc = "getPart(type)";
            }
        }
    }
    if (hpOut) *hpOut = hp;
    if (maxOut) *maxOut = mx;

    Item* worn = nullptr;
    LV_TRY
    {
        RobotLimbs* robots = med->robotLimbs;
        if (robots) worn = robots->getLimb(kGameLimb[slot]);
    }
    LV_EXCEPT { worn = nullptr; }
    if (worn && LvIsGrowthPart(worn))
    {
        if (LvGrowthPartStage(worn) == LV_PART_GROWN)
        {
            if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "LV Grown is the limb");
            return LIMB_KIND_WHOLE;
        }
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "LV part mid-growth");
        return LIMB_KIND_STUMP;
    }

    LimbState ps = LIMB_ORIGINAL;
    int robotic = 0;
    if (part)
    {
        LV_TRY
        {
            robotic = part->isRobotic() ? 1 : 0;
            ps = part->getRobotLimbState();
        }
        LV_EXCEPT {}
    }

    if (robotic && !LvIsGrowthPart(worn))
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "robotic prosthetic");
        return LIMB_KIND_PROSTHETIC;
    }
    if (ps == LIMB_REPLACED || st == LIMB_REPLACED)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "REPLACED");
        return LIMB_KIND_PROSTHETIC;
    }
    if (ps == LIMB_STUMP || st == LIMB_STUMP)
    {
        if (why && whyN > 0)
            std::snprintf(why, (size_t)whyN, "%s",
                         stSeh ? "part STUMP (getLimbState SEH)" : "getLimbState/part STUMP");
        return LIMB_KIND_STUMP;
    }
    if (ps == LIMB_CRUSHED || st == LIMB_CRUSHED)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "CRUSHED");
        return LIMB_KIND_CRUSHED;
    }

    /* Official state was ORIGINAL (or SEH). HP decides: Left Leg 5 is a
     * stump; 75-HP arms are intact; Right Leg 23 is injured, not a stump. */
    char hpWhy[48];
    hpWhy[0] = 0;
    const LimbKind fromHp = LvClassifyFromHp(hp, mx, haveHp, hpWhy, (int)sizeof(hpWhy));
    if (why && whyN > 0)
    {
        if (stSeh && fromHp == LIMB_KIND_WHOLE && haveHp)
            std::snprintf(why, (size_t)whyN, "%s (%s, getLimbState SEH)", hpWhy, hpSrc);
        else
            std::snprintf(why, (size_t)whyN, "%s (%s)", hpWhy[0] ? hpWhy : "?", hpSrc);
    }
    return fromHp;
}

static LimbKind ReadLimb(MedicalSystem* med, int slot)
{
    float hp = 0.f, mx = 0.f;
    char why[48];
    why[0] = 0;
    return ReadLimbEx(med, slot, &hp, &mx, why, (int)sizeof(why));
}

static float lvSiblingRealMax(MedicalSystem* med, int skip)
{
    float best = 0.f;
    if (!med)
        return 0.f;
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        if (i == skip)
            continue;
        MedicalSystem::HealthPartStatus* p = lvNamedPart(med, i);
        float hp = 0.f, mx = 0.f;
        if (!lvReadPartHp(p, &hp, &mx))
            continue;
        if (mx >= 20.f && mx > best)
            best = mx;
    }
    return best;
}

/* Incremental flesh only. A few HP per tick (+1..+4). Never 5→75 in one write.
 * Max follows slowly (with flesh or a few behind). Stay STUMP. No GROWN / restore. */
int LvGrowStumpNub(MedicalSystem* med, int limbId, float progress, float realMaxHint,
                   float* hpBefore, float* hpAfter, float* maxAfter)
{
    if (hpBefore) *hpBefore = 0.f;
    if (hpAfter) *hpAfter = 0.f;
    if (maxAfter) *maxAfter = 0.f;
    if (!med || limbId < 0 || limbId >= LIMB_COUNT)
        return 0;
    if (progress < 0.f) progress = 0.f;
    if (progress > 100.f) progress = 100.f;

    MedicalSystem::HealthPartStatus* part = lvNamedPart(med, limbId);
    float hp = 0.f, mx = 0.f;
    if (!lvReadPartHp(part, &hp, &mx))
    {
        MedicalSystem::HealthPartStatus* alt = nullptr;
        LV_TRY { alt = med->getPart(kGameLimb[limbId]); }
        LV_EXCEPT { alt = nullptr; }
        if (!lvReadPartHp(alt, &hp, &mx))
        {
            alt = lvPartByType(med, limbId);
            if (!lvReadPartHp(alt, &hp, &mx))
                return 0;
            part = alt;
        }
        else
            part = alt;
    }
    if (hpBefore) *hpBefore = hp;
    if (hpAfter) *hpAfter = hp;
    if (maxAfter) *maxAfter = mx;

    if ((limbId == LIMB_LEFT_ARM || limbId == LIMB_RIGHT_ARM) && hp >= 10.f)
        return 0;

    float realMax = mx;
    if (realMaxHint > realMax)
        realMax = realMaxHint;
    if (realMax < 20.f)
    {
        const float sib = lvSiblingRealMax(med, limbId);
        if (sib > realMax)
            realMax = sib;
    }
    if (realMax < 20.f)
        realMax = 75.f;

    if (progress <= 0.f)
        return 0;

    /* +3 typical (5→~8→~12). Hard cap +4. Never a full-progress jump. */
    float add = 3.f;
    if (add < 1.f) add = 1.f;
    if (add > 4.f) add = 4.f;
    float newFlesh = hp + add;
    if (newFlesh > realMax)
        newFlesh = realMax;
    if (newFlesh < hp)
        newFlesh = hp;

    /* Max with flesh or a few behind. Never write 5→75 in one tick. */
    float newMax = mx;
    if (mx + 0.05f < realMax)
    {
        newMax = mx + add;
        if (newMax > newFlesh + 3.f)
            newMax = newFlesh + 3.f;
        if (newMax < newFlesh)
            newMax = newFlesh;
        if (newMax > realMax)
            newMax = realMax;
        if (newMax > mx + 4.f)
            newMax = mx + 4.f;
    }

    const int needMax = (newMax > mx + 0.05f) ? 1 : 0;
    const int needFlesh = (newFlesh > hp + 0.05f) ? 1 : 0;
    if (!needMax && !needFlesh)
    {
        if (hpAfter) *hpAfter = hp;
        if (maxAfter) *maxAfter = mx;
        return 0;
    }

    LV_TRY
    {
        if (needMax)
            part->_maxHealth = newMax;
        if (needFlesh)
            part->flesh = newFlesh;
    }
    LV_EXCEPT
    {
        return 0;
    }

    float after = hp, afterMx = mx;
    lvReadPartHp(part, &after, &afterMx);
    if (hpAfter) *hpAfter = after;
    if (maxAfter) *maxAfter = afterMx;
    return (after > hp + 0.05f || afterMx > mx + 0.05f) ? 1 : 0;
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
    {
        char why[48];
        why[0] = 0;
        io->limbs[i] = ReadLimbEx(med, i, &io->limbHp[i], &io->limbMax[i], why, (int)sizeof(why));
        static int logged[LIMB_COUNT] = {};
        static char lastName[48];
        if (!lastName[0] || std::strcmp(lastName, io->name) != 0)
        {
            lastName[0] = 0;
            if (io->name[0])
                std::snprintf(lastName, sizeof(lastName), "%s", io->name);
            for (int z = 0; z < LIMB_COUNT; ++z) logged[z] = 0;
        }
        if (!logged[i])
        {
            logged[i] = 1;
            LimbState st = LIMB_ORIGINAL;
            LV_TRY { st = med->getLimbState(kGameLimb[i]); }
            LV_EXCEPT { st = LIMB_ORIGINAL; }
            LvLogf("LimbVigor: %s %s hp=%.1f/%.1f state=%s kind=%s why=%s",
                   io->name[0] ? io->name : "?",
                   LvLimbLabel((LimbId)i),
                   io->limbHp[i], io->limbMax[i],
                   lvLimbStateName(st),
                   lvLimbKindName(io->limbs[i]),
                   why[0] ? why : "?");
        }
    }
}

int LvRestoreLimb(MedicalSystem* med, int limbId)
{
    // Unused. v1.30: never restore-on-stump. Grow the nub with numbers.
    // Do NOT call setLimb(ORIGINAL). Do not re-hook this.
    (void)med;
    (void)limbId;
    static int once = 0;
    if (!once)
    {
        LvLog("LimbVigor: LvRestoreLimb is unused — grown part stays, no setLimb(ORIGINAL)");
        once = 1;
    }
    return 0;
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
    if (!text || !text[0])
        return;
    LvLog(text);
    if (!me || !LvCfg().enableSpeech || !g_say)
        return;
    GameStr gs;
    GameStrSet(&gs, text);
    LV_TRY { g_say(me, &gs); }
    LV_EXCEPT
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: _NV_say SEH"); once = 1; }
        g_say = nullptr;
    }
}

int LvIsSelectedCharacter(Character* me)
{
    /* The after-orig Character* is the body the medical panel is
     * drawing. Hired Hive / Shek are squad, not the player pawn.
     * isPlayerCharacter() is the wrong gate (v1.11 door-cleared them). */
    return LvIsPlayerSquad(me);
}

/* Official path only: ForgottenGUI* gui → mainbar @ 0x10.
 * No MainBarGUI ctor (0x72C1E0). No stash. Read after In-game only.
 * Prefix is the +0x40 std::string. Do not call _getWidget 0x723780. */
void* LvMainBar()
{
#if !defined(LIMBVIGOR_IDE)
    if (!gui)
        return nullptr;
    void* bar = nullptr;
    LV_TRY { std::memcpy(&bar, (const char*)(const void*)gui + 0x10, sizeof(bar)); }
    LV_EXCEPT { bar = nullptr; }
    return bar;
#else
    return nullptr;
#endif
}

/* Official: MainBarGUI::getMedicalPanel() RVA 0x71FDA0 → MedicalDatapanel*.
 * Member medicalPanel @ 0x188. KenshiLib has no MedicalDatapanel body. */
void* LvMedicalPanel()
{
    void* bar = LvMainBar();
    void* med = nullptr;
    if (g_getMedPanel && bar)
    {
        LV_TRY { med = g_getMedPanel(bar); }
        LV_EXCEPT { med = nullptr; }
    }
    if (!med && bar)
    {
        LV_TRY { std::memcpy(&med, (const char*)bar + 0x188, sizeof(med)); }
        LV_EXCEPT { med = nullptr; }
    }
    return med;
}

/* +0x188 is a pointer TO MedicalDatapanel, not MedicalDatapanel itself.
 * Do not compute MainBar = medicalPanel - 0x188. */
int LvMainBarProven(void* bar, void* med)
{
    if (!bar || !med)
        return 0;
    void* slot = nullptr;
    LV_TRY { std::memcpy(&slot, (const char*)bar + 0x188, sizeof(slot)); }
    LV_EXCEPT { slot = nullptr; }
    return (slot == med) ? 1 : 0;
}

/* v1.14 probe: dump every DatapanelGUI we can see. Do not guess-paint
 * on Goal/State/Encumbrance. Blood bars may not be DataPanelLines.
 * Paint only if a dump line shows a live key "Blood" on that panel. */
static int lvReadLineKey(void* line, char* out, int outsz)
{
    if (!line || !out || outsz < 2)
        return 0;
    out[0] = 0;
    /* DataPanelLine::keyValue @ 0x28 */
    GameStrRead((const char*)line + 0x28, out, outsz);
    return out[0] ? 1 : 0;
}

static int lvLiveKeyCat(DatapanelGUI* panel, const char* key)
{
    if (!panel || !key || !key[0] || !g_numLines || !g_lineByNum)
        return -1;
    for (int cat = 0; cat < 4; ++cat)
    {
        int n = 0;
        LV_TRY { n = g_numLines(panel, cat); }
        LV_EXCEPT { n = 0; }
        if (n <= 0)
            continue;
        for (int i = 0; i < n && i < 24; ++i)
        {
            void* line = nullptr;
            LV_TRY { line = g_lineByNum(panel, cat, i); }
            LV_EXCEPT { line = nullptr; }
            if (!line)
                continue;
            char buf[96];
            if (!lvReadLineKey(line, buf, (int)sizeof(buf)))
                continue;
            if (std::strcmp(buf, key) == 0)
                return cat;
        }
    }
    return -1;
}

int LvPanelIsLeftMedical(DatapanelGUI* panel)
{
    return lvLiveKeyCat(panel, "Blood") >= 0 ? 1 : 0;
}

int LvPanelHasBlood(DatapanelGUI* panel)
{
    return lvLiveKeyCat(panel, "Blood") >= 0 ? 1 : 0;
}

int LvPanelHasGoalKeys(DatapanelGUI* p)
{
    if (!p)
        return 0;
    return (lvLiveKeyCat(p, "Goal") >= 0
         || lvLiveKeyCat(p, "State") >= 0
         || lvLiveKeyCat(p, "Encumbrance:") >= 0
         || lvLiveKeyCat(p, "Current Skill") >= 0) ? 1 : 0;
}
