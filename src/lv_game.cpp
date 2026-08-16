#include "lv_game.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_parts.h"
#include "lv_msvcstr.h"
#include "lv_hud.h"

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
static int            g_paintLogged = 0;
static int            g_paintDead   = 0;
static int            g_noneLogged  = 0;

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

    /* MainBarGUI::getMedicalPanel — RVA only. No ctor hook (v1.9.7 load crash). */
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
        LvLog("LimbVigor: setLineProgress resolved");
    else
        LvLog("LimbVigor: setLineProgress missing — medical row skipped");
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

static LimbKind ReadLimb(MedicalSystem* med, int slot)
{
    if (!med) return LIMB_KIND_WHOLE;
    LimbState st = LIMB_ORIGINAL;
    LV_TRY { st = med->getLimbState(kGameLimb[slot]); }
    LV_EXCEPT { st = LIMB_ORIGINAL; }

    MedicalSystem::HealthPartStatus* part = nullptr;
    LV_TRY { part = med->getPart(kGameLimb[slot]); }
    LV_EXCEPT { part = nullptr; }

    // Our growth parts occupy the socket like a prosthetic. Mid-growth
    // they stay a stump so the sim keeps going. A Grown part IS the limb.
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
            return LIMB_KIND_WHOLE;
        return LIMB_KIND_STUMP;
    }

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
    // Unused. DriveTick slots LV Grown via LvEquipGrowthPart.
    // Do NOT call setLimb(ORIGINAL) at 100%. Do not re-hook this.
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
 * No MainBarGUI ctor hook. No stash. Read after In-game only. */
static void* lvMainBar()
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
static void* lvMedicalPanel()
{
    void* bar = lvMainBar();
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

static int lvLineExists(DatapanelGUI* panel, const char* key, int cat)
{
    if (!g_lineExists || !panel || !key)
        return 0;
    GameStr gs;
    GameStrSet(&gs, key);
    int yes = 0;
    LV_TRY { yes = g_lineExists(panel, &gs, cat) ? 1 : 0; }
    LV_EXCEPT { yes = 0; }
    return yes;
}

static int lvBloodCat(DatapanelGUI* panel)
{
    if (!panel)
        return -1;
    for (int cat = 0; cat < 4; ++cat)
    {
        if (lvLineExists(panel, "Blood", cat))
            return cat;
    }
    return -1;
}

static int KeyEqI(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0 ? 1 : 0;
}

/* Title-Case Kenshi rows only. "left leg" is ours and must not match "Left Leg". */
static int IsReservedKey(const char* key)
{
    static const char* kRes[] = {
        "Blood", "Head", "Hunger",
        "Left Arm", "Right Arm", "Left Leg", "Right Leg",
        nullptr
    };
    if (!key || !key[0]) return 1;
    for (int i = 0; kRes[i]; ++i)
    {
        if (std::strcmp(key, kRes[i]) == 0)
            return 1;
    }
    return 0;
}

static int IsBannedHudKey(const char* key)
{
    static const char* kBan[] = {
        "Time", "Look", "How", "Need", "Regrowth", "Wait", nullptr
    };
    if (!key || !key[0]) return 1;
    for (int i = 0; kBan[i]; ++i)
    {
        if (KeyEqI(key, kBan[i]))
            return 1;
    }
    return 0;
}

static void lvNoneExists()
{
    if (g_noneLogged)
        return;
    g_noneLogged = 1;
    LvLog("LimbVigor: none exists");
}

int LvPanelIsLeftMedical(DatapanelGUI* panel)
{
    if (!panel)
        return 0;
    void* med = lvMedicalPanel(); /* MedicalDatapanel* */
    /* Same address: MedicalDatapanel is the DatapanelGUI the hook received. */
    if (med && (void*)panel == med)
        return 1;
    /* Different pointer: do not cast MedicalDatapanel* and write to it.
     * Identify the hook's DatapanelGUI* via Blood (what orig just wrote). */
    if (lvBloodCat(panel) >= 0)
        return 1;
    return 0;
}

int LvPanelHasBlood(DatapanelGUI* panel)
{
    return lvBloodCat(panel) >= 0 ? 1 : 0;
}

void LvWalkSelPanel(DatapanelGUI* panel)
{
    if (!panel)
        return;

    static void* seenPtr[6] = {};
    static int seenBlood[6] = {};
    static int seenN = 0;
    const int hasBlood = lvBloodCat(panel) >= 0 ? 1 : 0;
    for (int i = 0; i < seenN; ++i)
    {
        if (seenPtr[i] == (void*)panel && seenBlood[i] == hasBlood)
            return;
    }
    if (seenN < 6)
    {
        seenPtr[seenN] = (void*)panel;
        seenBlood[seenN] = hasBlood;
        seenN++;
    }

    void* med = lvMedicalPanel();
    const int match = (med && (void*)panel == med) ? 1 : 0;
    LvLogf("LimbVigor: panel=%p medicalPanel=%p match=%d lineExists(Blood)=%d",
           (void*)panel, med, match, hasBlood);

    if (g_numLines)
    {
        for (int cat = 0; cat < 4; ++cat)
        {
            int n = 0;
            LV_TRY { n = g_numLines(panel, cat); }
            LV_EXCEPT { n = 0; }
            LvLogf("LimbVigor: getNumLines(%d)=%d", cat, n);
            if (n <= 0 || !g_lineByNum)
                continue;
            for (int i = 0; i < n && i < 24; ++i)
            {
                void* line = nullptr;
                LV_TRY { line = g_lineByNum(panel, cat, i); }
                LV_EXCEPT { line = nullptr; }
                if (!line)
                    continue;
                /* DataPanelLine::keyValue @ 0x28 */
                char buf[96];
                GameStrRead((const char*)line + 0x28, buf, (int)sizeof(buf));
                LvLogf("LimbVigor: line[%d][%d] key='%s'", cat, i, buf);
            }
        }
    }
}

static void lvRemoveKey(DatapanelGUI* panel, const char* key, int cat)
{
    if (!panel || !g_removeLine || !key || !key[0])
        return;
    if (!lvLineExists(panel, key, cat))
        return;
    GameStr gs;
    GameStrSet(&gs, key);
    LV_TRY { g_removeLine(panel, &gs, cat); }
    LV_EXCEPT {}
}

void LvClearHud(DatapanelGUI* panel)
{
    if (!panel || !g_removeLine)
        return;
    static const char* kOurs[] = {
        "Hemolymph", "Vigor", "Battle-heat", "Limb Vigor",
        "left leg", "right leg", "left arm", "right arm",
        "Regrowth", "Wait",
        nullptr
    };
    for (int cat = 0; cat < 4; ++cat)
    {
        for (int i = 0; kOurs[i]; ++i)
            lvRemoveKey(panel, kOurs[i], cat);
    }
}

void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap)
{
    if (!panel || !snap || !LvCfg().enableHud || g_paintDead)
        return;
    if (!LvWorldInGame())
        return;
    /* Selection info panel + the body this panel is drawing. Never C / skills. */
    if (!LvPanelIsLeftMedical(panel))
        return;
    if (!LvIsPlayerSquad(LvCharFromMed(med)))
        return;
    if (snap->race == RACE_ANIMAL)
    {
        LvClearHud(panel);
        return;
    }

    int cat = lvBloodCat(panel);
    if (cat < 0)
    {
        lvNoneExists();
        return;
    }
    if (!g_setLineProg)
    {
        lvNoneExists();
        return;
    }

    const char* key1 = LvHudResourceKey(snap);
    if (!key1 || !key1[0] || IsReservedKey(key1) || IsBannedHudKey(key1))
    {
        lvNoneExists();
        return;
    }

    static const char* kKeep[] = {
        "Blood", "Head", "Hunger",
        "Left Arm", "Right Arm", "Left Leg", "Right Leg",
        nullptr
    };
    int had[8] = {};
    for (int i = 0; kKeep[i] && i < 8; ++i)
        had[i] = lvLineExists(panel, kKeep[i], cat);

    char bar1[96], bar2[96], limbKey[32];
    float fill1 = 0.f, fill2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &fill1, bar2, (int)sizeof(bar2), &fill2, limbKey, (int)sizeof(limbKey));
    if (!bar1[0])
    {
        lvNoneExists();
        return;
    }
    if (fill1 < 0.f) fill1 = 0.f;
    if (fill1 > 1.f) fill1 = 1.f;

    GameStr gkey, gtext;
    GameStrSet(&gkey, key1);
    GameStrSet(&gtext, bar1);

    int excepted = 0;
    LV_TRY { g_setLineProg(panel, &gkey, cat, fill1, &gtext, true); }
    LV_EXCEPT { excepted = 1; }
    if (excepted)
    {
        g_paintDead = 1;
        lvNoneExists();
        LvErr("LimbVigor: setLineProgress SEH — medical row stopped");
        return;
    }

    static const char* kLimbKeys[] = {
        "left leg", "right leg", "left arm", "right arm", nullptr
    };
    if (bar2[0] && limbKey[0] && !IsReservedKey(limbKey) && !IsBannedHudKey(limbKey))
    {
        if (fill2 < 0.f) fill2 = 0.f;
        if (fill2 > 1.f) fill2 = 1.f;
        GameStr lkey, ltext;
        GameStrSet(&lkey, limbKey);
        GameStrSet(&ltext, bar2);
        LV_TRY { g_setLineProg(panel, &lkey, cat, fill2, &ltext, true); }
        LV_EXCEPT { excepted = 1; }
        if (excepted)
        {
            g_paintDead = 1;
            lvNoneExists();
            LvErr("LimbVigor: setLineProgress SEH — stump row stopped");
            return;
        }
        for (int i = 0; kLimbKeys[i]; ++i)
        {
            if (!KeyEqI(kLimbKeys[i], limbKey))
                lvRemoveKey(panel, kLimbKeys[i], cat);
        }
    }
    else
    {
        for (int i = 0; kLimbKeys[i]; ++i)
            lvRemoveKey(panel, kLimbKeys[i], cat);
    }
    lvRemoveKey(panel, "Regrowth", cat);
    lvRemoveKey(panel, "Wait", cat);

    int ate = 0;
    if (!lvLineExists(panel, "Blood", cat))
        ate = 1;
    for (int i = 0; kKeep[i] && i < 8; ++i)
    {
        if (had[i] && !lvLineExists(panel, kKeep[i], cat))
            ate = 1;
    }
    if (ate)
    {
        LvClearHud(panel);
        g_paintDead = 1;
        lvNoneExists();
        return;
    }

    if (!g_paintLogged)
    {
        g_paintLogged = 1;
        LvLogf("LimbVigor: setLineProgress %s cat=%d %s", key1, cat, bar1);
        if (bar2[0] && limbKey[0])
            LvLogf("LimbVigor: setLineProgress %s cat=%d %s", limbKey, cat, bar2);
    }
}
