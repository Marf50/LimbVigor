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
static int            g_paintLogged = 0;

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

static int g_dumpBlood = 0;
static int g_dumpSummary = 0;
static int g_extraDump = 0;
static void* g_dumpSeen[24] = {};
static int g_dumpSeenN = 0;
static void* g_hookReject = nullptr; /* Goal/State DatapanelGUI* — never paint */
static DatapanelGUI* g_lifeBarPanel = nullptr;

static int lvPanelHasGoalKeys(DatapanelGUI* p)
{
    if (!p)
        return 0;
    return (lvLiveKeyCat(p, "Goal") >= 0
         || lvLiveKeyCat(p, "State") >= 0
         || lvLiveKeyCat(p, "Encumbrance:") >= 0
         || lvLiveKeyCat(p, "Current Skill") >= 0) ? 1 : 0;
}

static int lvDumpSeen(void* p)
{
    if (!p)
        return 1;
    for (int i = 0; i < g_dumpSeenN; ++i)
    {
        if (g_dumpSeen[i] == p)
            return 1;
    }
    if (g_dumpSeenN >= 24)
        return 1;
    g_dumpSeen[g_dumpSeenN++] = p;
    return 0;
}

static void lvDumpSummary()
{
    if (g_dumpSummary)
        return;
    if (!g_extraDump || g_dumpSeenN < 1)
        return;
    g_dumpSummary = 1;
    if (g_dumpBlood)
        LvLog("LimbVigor: dump saw live key Blood on a DatapanelGUI — still not a paint target");
    else
        LvLog("LimbVigor: dump: no DatapanelGUI has live key Blood (expected — Blood is LifeBar1)");
}

static void lvDumpOnePanel(DatapanelGUI* panel, const char* src)
{
    if (!panel || lvDumpSeen((void*)panel))
        return;

    void* med = lvMedicalPanel();
    const int match = (med && (void*)panel == med) ? 1 : 0;
    LvLogf("LimbVigor: dump panel=%p medicalPanel=%p match=%d src=%s",
           (void*)panel, med, match, src ? src : "?");

    int liveBlood = 0;
    if (!g_numLines)
    {
        LvLog("LimbVigor: dump getNumLines missing");
        return;
    }
    for (int cat = 0; cat < 4; ++cat)
    {
        int n = 0;
        int seh = 0;
        LV_TRY { n = g_numLines(panel, cat); }
        LV_EXCEPT { n = 0; seh = 1; }
        if (seh)
        {
            LvLogf("LimbVigor: dump getNumLines(%d)=SEH", cat);
            continue;
        }
        LvLogf("LimbVigor: dump getNumLines(%d)=%d", cat, n);
        if (n <= 0 || !g_lineByNum)
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
            LvLogf("LimbVigor: dump line[%d][%d] key='%s'", cat, i, buf);
            if (std::strcmp(buf, "Blood") == 0)
            {
                liveBlood = 1;
                g_dumpBlood = 1;
            }
        }
    }
    if (liveBlood)
        LvLogf("LimbVigor: dump panel=%p has live key Blood", (void*)panel);
    else
        LvLogf("LimbVigor: dump panel=%p has no live key Blood", (void*)panel);
}

static void lvDumpExtrasOnce()
{
    if (g_extraDump)
        return;
    g_extraDump = 1;

    void* med = lvMedicalPanel();
    /* Addendum: MainBar+0x188 is MedicalDatapanel*, not DatapanelGUI.
     * getNumLines=0 there is a type mismatch, not an empty panel. */
    LvLogf("LimbVigor: medicalPanel=%p is MedicalDatapanel* (MainBar+0x188) — not DatapanelGUI, not setLineProgress",
           med);
    lvDumpSummary();
}

#if !defined(LIMBVIGOR_IDE)
/* v1.18: bind Gui::findWidgetT (throw=false) and look up LifeBar1*.
 * Do not walk the Gui tree (v1.17 walk SEH'd). Do not treat
 * DatapanelGUI* as Widget*. Do not treat medicalPanel / 0x188 as
 * DatapanelGUI. No createWidget. Names via GameStr. */

typedef void*         (*FnGuiInst)();
typedef void*         (*FnFindW)(void* gui, const GameStr* name, unsigned char throwFlag);
typedef const void*   (*FnGetName)(void* self);
typedef std::size_t   (*FnChildN)(void* self);
typedef unsigned char (*FnVisible)(void* self);
typedef const void*   (*FnCaption)(void* self);

static FnGuiInst  g_guiInst  = nullptr;
static FnFindW    g_findW    = nullptr;
static FnGuiInst  g_wmInst   = nullptr;
static FnFindW    g_wmFind   = nullptr;
static FnGetName  g_wName    = nullptr;
static FnChildN   g_wCount   = nullptr;
static FnVisible  g_wVis     = nullptr;
static FnCaption  g_wCaption = nullptr;
static int        g_exportDumped = 0;
static int        g_foundLogged = 0;
static int        g_lifeBarWhy = 0;
static const char* g_findSym = nullptr;

static const char kGuiGetInstanceExact[] =
    "?getInstancePtr@?$Singleton@VGui@MyGUI@@@MyGUI@@SAPEAVGui@2@XZ";
static const char kFindWidgetTExact[] =
    "?findWidgetT@Gui@MyGUI@@QEAAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z";
static const char kWmGetInstanceExact[] =
    "?getInstancePtr@?$Singleton@VWidgetManager@MyGUI@@@MyGUI@@SAPEAVWidgetManager@2@XZ";

static int lvHasI(const char* hay, const char* needle)
{
    if (!hay || !needle || !needle[0])
        return 0;
    for (const char* h = hay; *h; ++h)
    {
        const char* a = h;
        const char* b = needle;
        while (*a && *b)
        {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (!*b) return 1;
    }
    return 0;
}

static int lvMangledStringArgs(const char* n)
{
    int c = 0;
    if (!n) return 0;
    for (const char* p = n; *p; ++p)
    {
        if (std::strncmp(p, "basic_string", 12) == 0)
        {
            c++;
            p += 11;
        }
    }
    return c;
}

static int lvIsOtherMyGuiSingleton(const char* n)
{
    return (lvHasI(n, "Clipboard") || lvHasI(n, "InputManager")
         || lvHasI(n, "PointerManager") || lvHasI(n, "FontManager")
         || lvHasI(n, "SkinManager") || lvHasI(n, "LanguageManager")
         || lvHasI(n, "ResourceManager") || lvHasI(n, "FactoryManager")
         || lvHasI(n, "PluginManager") || lvHasI(n, "DynLibManager")
         || lvHasI(n, "ControllerManager") || lvHasI(n, "LayoutManager")
         || lvHasI(n, "RenderManager") || lvHasI(n, "ToolTipManager")
         || lvHasI(n, "SubWidgetManager") || lvHasI(n, "LayerManager")
         || lvHasI(n, "WidgetManager") || lvHasI(n, "DataManager")
         || lvHasI(n, "LogManager")) ? 1 : 0;
}

static int lvIsGuiSingletonGetInstance(const char* n)
{
    if (!n || !lvHasI(n, "getInstancePtr"))
        return 0;
    if (lvIsOtherMyGuiSingleton(n))
        return 0;
    if (lvHasI(n, "Singleton@VGui@MyGUI") || lvHasI(n, "$Singleton@VGui@"))
        return 1;
    if (std::strstr(n, "VGui@MyGUI") || std::strstr(n, "@VGui@"))
        return 1;
    return 0;
}

static int lvIsGuiFindWidgetT2(const char* n)
{
    if (!n || lvHasI(n, "createWidget") || lvHasI(n, "destroyWidget"))
        return 0;
    if (!lvHasI(n, "findWidget"))
        return 0;
    if (lvHasI(n, "WidgetManager"))
        return 0;
    if (!lvHasI(n, "@Gui@MyGUI") && !std::strstr(n, "VGui@MyGUI"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return lvMangledStringArgs(n) == 1 ? 1 : 0;
}

static int lvIsWmFindById2(const char* n)
{
    if (!n || !lvHasI(n, "findById") || !lvHasI(n, "WidgetManager"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return lvMangledStringArgs(n) == 1 ? 1 : 0;
}

static void lvTryBindExport(const char* n, void* addr)
{
    if (!n || !addr)
        return;
    if (!g_guiInst && lvIsGuiSingletonGetInstance(n))
    {
        g_guiInst = (FnGuiInst)addr;
        LvLogf("LimbVigor: mygui bind getInstancePtr '%s'", n);
    }
    if (!g_findW && lvIsGuiFindWidgetT2(n))
    {
        g_findW = (FnFindW)addr;
        g_findSym = n;
        LvLogf("LimbVigor: mygui bind findWidget '%s'", n);
    }
    if (!g_wmInst && lvHasI(n, "getInstancePtr") && lvHasI(n, "WidgetManager")
     && lvHasI(n, "Singleton"))
    {
        g_wmInst = (FnGuiInst)addr;
        LvLogf("LimbVigor: mygui bind WidgetManager getInstancePtr '%s'", n);
    }
    if (!g_wmFind && lvIsWmFindById2(n))
    {
        g_wmFind = (FnFindW)addr;
        if (!g_findSym) g_findSym = n;
        LvLogf("LimbVigor: mygui bind WidgetManager findById '%s'", n);
    }
    if (!g_wName && lvHasI(n, "getName") && lvHasI(n, "Widget")
     && !lvHasI(n, "getNameAt") && !lvHasI(n, "getNameBy"))
    {
        g_wName = (FnGetName)addr;
        LvLogf("LimbVigor: mygui bind getName '%s'", n);
    }
    if (!g_wCount && lvHasI(n, "getChildCount") && lvHasI(n, "Widget"))
    {
        g_wCount = (FnChildN)addr;
        LvLogf("LimbVigor: mygui bind getChildCount '%s'", n);
    }
    if (!g_wVis && lvHasI(n, "getVisible") && lvHasI(n, "Widget")
     && !lvHasI(n, "Inherited") && !lvHasI(n, "setVisible"))
    {
        g_wVis = (FnVisible)addr;
        LvLogf("LimbVigor: mygui bind getVisible '%s'", n);
    }
    if (!g_wCaption && lvHasI(n, "getCaption") && lvHasI(n, "Widget")
     && !lvHasI(n, "setCaption"))
    {
        g_wCaption = (FnCaption)addr;
        LvLogf("LimbVigor: mygui bind getCaption '%s'", n);
    }
}

static HMODULE lvMyGuiMod()
{
    HMODULE mod = GetModuleHandleA("MyGUIEngine_x64.dll");
    if (!mod)
        mod = GetModuleHandleA("MyGUIEngine.dll");
    return mod;
}

static void lvDumpMyGuiExports()
{
    if (g_exportDumped)
        return;
    g_exportDumped = 1;

    HMODULE mod = lvMyGuiMod();
    if (!mod)
    {
        LvLog("LimbVigor: mygui export list — MyGUIEngine not loaded");
        return;
    }
    LvLogf("LimbVigor: mygui module=%p", (void*)mod);

    FARPROC exactGui = GetProcAddress(mod, kGuiGetInstanceExact);
    if (exactGui)
    {
        g_guiInst = (FnGuiInst)exactGui;
        LvLogf("LimbVigor: mygui bind getInstancePtr EXACT '%s'", kGuiGetInstanceExact);
    }
    static const char kFindWidgetTConst[] =
        "?findWidgetT@Gui@MyGUI@@QEBAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z";
    FARPROC exactFind = GetProcAddress(mod, kFindWidgetTExact);
    if (!exactFind)
        exactFind = GetProcAddress(mod, kFindWidgetTConst);
    if (exactFind)
    {
        g_findW = (FnFindW)exactFind;
        g_findSym = GetProcAddress(mod, kFindWidgetTExact) ? kFindWidgetTExact : kFindWidgetTConst;
        LvLogf("LimbVigor: mygui bind findWidget EXACT '%s'", g_findSym);
    }
    else
        LvLog("LimbVigor: mygui exact findWidgetT missing — scanning exports");
    FARPROC exactWm = GetProcAddress(mod, kWmGetInstanceExact);
    if (exactWm)
        g_wmInst = (FnGuiInst)exactWm;

    const unsigned char* base = (const unsigned char*)(const void*)mod;
    IMAGE_DOS_HEADER dos;
    std::memcpy(&dos, base, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000)
        return;
    IMAGE_NT_HEADERS64 nt;
    std::memcpy(&nt, base + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE)
        return;
    const IMAGE_DATA_DIRECTORY dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress || dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY))
        return;
    IMAGE_EXPORT_DIRECTORY exp;
    std::memcpy(&exp, base + dir.VirtualAddress, sizeof(exp));
    if (!exp.NumberOfNames || exp.NumberOfNames > 20000)
        return;

    const DWORD* names = (const DWORD*)(base + exp.AddressOfNames);
    const WORD* ords = (const WORD*)(base + exp.AddressOfNameOrdinals);
    const DWORD* funcs = (const DWORD*)(base + exp.AddressOfFunctions);
    int loggedFind = 0;
    for (DWORD i = 0; i < exp.NumberOfNames; ++i)
    {
        DWORD nrva = 0;
        WORD ord = 0;
        DWORD frva = 0;
        LV_TRY
        {
            nrva = names[i];
            ord = ords[i];
            frva = funcs[ord];
        }
        LV_EXCEPT { continue; }
        if (!nrva)
            continue;
        const char* n = (const char*)(base + nrva);
        char en[256];
        en[0] = 0;
        LV_TRY
        {
            int k = 0;
            while (n[k] && k < 255) { en[k] = n[k]; ++k; }
            en[k] = 0;
        }
        LV_EXCEPT { en[0] = 0; }
        if (!en[0])
            continue;
        void* addr = (void*)(base + frva);
        lvTryBindExport(en, addr);
        if (lvHasI(en, "findWidget") || lvHasI(en, "findById"))
        {
            LvLogf("LimbVigor: mygui export find '%s' %p", en, addr);
            loggedFind++;
        }
    }
    LvLogf("LimbVigor: mygui find exports=%d bind gui=%d findWidget=%d wm=%d name=%d vis=%d childN=%d caption=%d",
           loggedFind, g_guiInst ? 1 : 0, g_findW ? 1 : 0, g_wmFind ? 1 : 0,
           g_wName ? 1 : 0, g_wVis ? 1 : 0, g_wCount ? 1 : 0, g_wCaption ? 1 : 0);
    if (g_findSym)
        LvLogf("LimbVigor: mygui findWidget symbol '%s'", g_findSym);
    if (!g_findW && !g_wmFind)
        LvLog("LimbVigor: mygui findWidget missing after full export scan");
}

static int lvGameStrLooksName(const char* s)
{
    if (!s || !s[0])
        return 0;
    int n = 0;
    for (; s[n]; ++n)
    {
        unsigned char c = (unsigned char)s[n];
        if (n > 64)
            return 0;
        if (c < 32 || c > 126)
            return 0;
    }
    return 1;
}

static void lvReadCaption(const void* s, char* out, int n)
{
    if (!out || n < 2)
        return;
    out[0] = 0;
    if (!s)
        return;
    if (GameStrRead(s, out, n) && lvGameStrLooksName(out))
        return;
    out[0] = 0;
    size_t size = 0, cap = 0;
    LV_TRY
    {
        std::memcpy(&size, (const char*)s + 16, sizeof(size));
        std::memcpy(&cap, (const char*)s + 24, sizeof(cap));
    }
    LV_EXCEPT { return; }
    if (size == 0 || size > 64 || cap > (size_t)1 << 20)
        return;
    const wchar_t* src = nullptr;
    LV_TRY
    {
        if (cap > 7)
            std::memcpy(&src, s, sizeof(src));
        else
            src = (const wchar_t*)s;
    }
    LV_EXCEPT { src = nullptr; }
    if (!src)
        return;
    int o = 0;
    LV_TRY
    {
        for (size_t i = 0; i < size && o < n - 1; ++i)
        {
            unsigned c = (unsigned)src[i];
            if (c == 0)
                break;
            if (c < 32 || c > 126)
                continue;
            out[o++] = (char)c;
        }
        out[o] = 0;
    }
    LV_EXCEPT { out[0] = 0; }
}

static void* lvFindWidget(const char* name)
{
    if (!name || !name[0])
        return nullptr;
    GameStr gs;
    GameStrSet(&gs, name);
    const unsigned char noThrow = 0;
    if (g_findW && g_guiInst)
    {
        void* gui = nullptr;
        LV_TRY { gui = g_guiInst(); }
        LV_EXCEPT { gui = nullptr; }
        if (gui)
        {
            void* w = nullptr;
            int seh = 0;
            LV_TRY { w = g_findW(gui, &gs, noThrow); }
            LV_EXCEPT { w = nullptr; seh = 1; }
            if (seh)
            {
                static int once = 0;
                if (!once) { LvErr("LimbVigor: findWidget SEH — growth continues"); once = 1; }
                return nullptr;
            }
            return w;
        }
    }
    if (g_wmFind && g_wmInst)
    {
        void* wm = nullptr;
        LV_TRY { wm = g_wmInst(); }
        LV_EXCEPT { wm = nullptr; }
        if (wm)
        {
            void* w = nullptr;
            LV_TRY { w = g_wmFind(wm, &gs, noThrow); }
            LV_EXCEPT { w = nullptr; }
            return w;
        }
    }
    return nullptr;
}

static void lvLogFound(const char* want, void* w)
{
    if (!w)
    {
        LvLogf("LimbVigor: findWidget '%s' = null", want);
        return;
    }
    int vis = -1;
    if (g_wVis)
    {
        LV_TRY { vis = g_wVis(w) ? 1 : 0; }
        LV_EXCEPT { vis = -1; }
    }
    int kids = -1;
    if (g_wCount)
    {
        std::size_t n = 0;
        int seh = 0;
        LV_TRY { n = g_wCount(w); }
        LV_EXCEPT { n = 0; seh = 1; }
        if (!seh && n <= 256)
            kids = (int)n;
    }
    char cap[96];
    cap[0] = 0;
    if (g_wCaption)
    {
        const void* s = nullptr;
        LV_TRY { s = g_wCaption(w); }
        LV_EXCEPT { s = nullptr; }
        if (s)
            lvReadCaption(s, cap, (int)sizeof(cap));
    }
    LvLogf("LimbVigor: findWidget '%s' ptr=%p vis=%d childN=%d caption='%s'",
           want, w, vis, kids, cap);
}

static int lvCouldBeDatapanel(void* p)
{
    if (!p || !g_numLines)
        return 0;
    if (p == lvMedicalPanel())
        return 0;
    if (g_hookReject && p == g_hookReject)
        return 0;
    for (int cat = 0; cat < 4; ++cat)
    {
        int n = -1;
        int seh = 0;
        LV_TRY { n = g_numLines((DatapanelGUI*)p, cat); }
        LV_EXCEPT { n = -1; seh = 1; }
        if (seh || n < 0 || n > 32)
            return 0;
    }
    if (lvPanelHasGoalKeys((DatapanelGUI*)p))
        return 0;
    return 1;
}

static void* lvScanAttached(void* widget)
{
    if (!widget)
        return nullptr;
    for (int off = 8; off <= 0x200; off += (int)sizeof(void*))
    {
        void* p = nullptr;
        LV_TRY { std::memcpy(&p, (const char*)widget + off, sizeof(p)); }
        LV_EXCEPT { p = nullptr; }
        if (!p || p == widget || p == lvMedicalPanel())
            continue;
        if (lvCouldBeDatapanel(p))
        {
            LvLogf("LimbVigor: DatapanelGUI %p attached on LifeBar1Datapanel+0x%X", p, off);
            return p;
        }
    }
    void* med = lvMedicalPanel();
    if (!med)
        return nullptr;
    for (int off = 0; off <= 0x400; off += (int)sizeof(void*))
    {
        void* slot = nullptr;
        LV_TRY { std::memcpy(&slot, (const char*)med + off, sizeof(slot)); }
        LV_EXCEPT { slot = nullptr; }
        if (slot != widget)
            continue;
        for (int d = -0x40; d <= 0x40; d += (int)sizeof(void*))
        {
            const int at = off + d;
            if (at < 0 || at > 0x400)
                continue;
            void* emb = (char*)med + at;
            if (emb != med && lvCouldBeDatapanel(emb))
            {
                LvLogf("LimbVigor: DatapanelGUI embedded in MedicalDatapanel+0x%X (LifeBar1Datapanel slot 0x%X)",
                       at, off);
                return emb;
            }
            void* p = nullptr;
            LV_TRY { std::memcpy(&p, (const char*)med + at, sizeof(p)); }
            LV_EXCEPT { p = nullptr; }
            if (p && p != widget && p != med && lvCouldBeDatapanel(p))
            {
                LvLogf("LimbVigor: DatapanelGUI %p next to LifeBar1Datapanel in MedicalDatapanel+0x%X",
                       p, at);
                return p;
            }
        }
    }
    return nullptr;
}

static void lvWhyOnce(const char* why)
{
    if (g_lifeBarWhy)
        return;
    g_lifeBarWhy = 1;
    LvLog(why);
}

static void lvResolveLifeBar()
{
    lvDumpMyGuiExports();
    void* names[6];
    static const char* kNames[] = {
        "MedicalPanel", "LifeBar1", "LifeBar1Datapanel",
        "LifeBar1Value", "LifeBar1Green", "HealthText"
    };
    for (int i = 0; i < 6; ++i)
    {
        names[i] = lvFindWidget(kNames[i]);
        if (!g_foundLogged)
            lvLogFound(kNames[i], names[i]);
    }
    if (!g_foundLogged)
    {
        g_foundLogged = 1;
        if (g_findSym)
            LvLogf("LimbVigor: findWidget bound '%s'", g_findSym);
    }

    void* w = names[2];
    if (!w)
    {
        lvWhyOnce("LimbVigor: LifeBar1Datapanel findWidget=null — not painting, not creating a widget");
        g_lifeBarPanel = nullptr;
        return;
    }
    if (lvCouldBeDatapanel(w))
    {
        g_lifeBarPanel = (DatapanelGUI*)w;
        LvLogf("LimbVigor: LifeBar1Datapanel Widget* %p accepts getNumLines — using as DatapanelGUI", w);
        return;
    }
    void* att = lvScanAttached(w);
    if (att)
    {
        g_lifeBarPanel = (DatapanelGUI*)att;
        return;
    }
    g_lifeBarPanel = nullptr;
    lvWhyOnce("LimbVigor: LifeBar1Datapanel is a Widget, no DatapanelGUI attached — not painting, not creating a widget");
}

static void lvDumpMyGuiOnce()
{
    if (!LvWorldInGame())
        return;
    lvResolveLifeBar();
}
#else
static void lvDumpMyGuiOnce() {}
static void lvResolveLifeBar() {}
static void lvWhyOnce(const char* why) { (void)why; }
#endif

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
        if (std::strcmp(key, kRes[i]) == 0)
            return 1;
    return 0;
}

static int IsBannedHudKey(const char* key)
{
    static const char* kBan[] = {
        "Time", "Look", "How", "Need", "Regrowth", "Wait", nullptr
    };
    if (!key || !key[0]) return 1;
    for (int i = 0; kBan[i]; ++i)
        if (KeyEqI(key, kBan[i]))
            return 1;
    return 0;
}

void LvWalkSelPanel(DatapanelGUI* panel)
{
    if (!panel || !LvWorldInGame())
        return;
    lvDumpOnePanel(panel, "hook");
    if (lvPanelHasGoalKeys(panel) && !g_hookReject)
        g_hookReject = (void*)panel;
    lvDumpExtrasOnce();
    lvDumpMyGuiOnce();
}

static void lvRemoveKey(DatapanelGUI* panel, const char* key, int cat)
{
    if (!panel || !g_removeLine || !key || !key[0] || cat < 0)
        return;
    GameStr gs;
    GameStrSet(&gs, key);
    LV_TRY { g_removeLine(panel, &gs, cat); }
    LV_EXCEPT {}
}

static DatapanelGUI* lvPaintDest()
{
    lvResolveLifeBar();
    DatapanelGUI* dest = g_lifeBarPanel;
    if (!dest)
        return nullptr;
    if ((void*)dest == lvMedicalPanel())
    {
        lvWhyOnce("LimbVigor: refusing medicalPanel MedicalDatapanel* as setLineProgress dest");
        g_lifeBarPanel = nullptr;
        return nullptr;
    }
    if (lvPanelHasGoalKeys(dest) || (g_hookReject && (void*)dest == g_hookReject))
    {
        lvWhyOnce("LimbVigor: refusing Goal/State panel — not painting");
        g_lifeBarPanel = nullptr;
        return nullptr;
    }
    return dest;
}

void LvClearHud(DatapanelGUI* panel)
{
    (void)panel;
    DatapanelGUI* dest = g_lifeBarPanel;
    if (!dest || !g_removeLine)
        return;
    if ((void*)dest == lvMedicalPanel() || lvPanelHasGoalKeys(dest))
        return;
    static const char* kOurs[] = {
        "Hemolymph", "Vigor", "Battle-heat", "Limb Vigor",
        "left leg", "right leg", "left arm", "right arm",
        "Regrowth", "Wait",
        nullptr
    };
    for (int i = 0; kOurs[i]; ++i)
    {
        int cat = lvLiveKeyCat(dest, kOurs[i]);
        if (cat >= 0)
            lvRemoveKey(dest, kOurs[i], cat);
    }
}

void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap)
{
    (void)panel;
    if (!snap || !LvCfg().enableHud || !LvWorldInGame())
        return;
    if (snap->race == RACE_ANIMAL)
    {
        LvClearHud(nullptr);
        return;
    }
    if (med && !LvIsPlayerSquad(LvCharFromMed(med)))
        return;

    DatapanelGUI* dest = lvPaintDest();
    if (!dest)
        return;
    if (!g_setLineProg)
    {
        lvWhyOnce("LimbVigor: setLineProgress missing — LifeBar1Datapanel not painted");
        return;
    }

    const char* key1 = LvHudResourceKey(snap);
    if (!key1 || !key1[0] || IsReservedKey(key1) || IsBannedHudKey(key1))
        return;

    char bar1[96], bar2[96], limbKey[32];
    float fill1 = 0.f, fill2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &fill1, bar2, (int)sizeof(bar2), &fill2, limbKey, (int)sizeof(limbKey));
    if (!bar1[0])
        return;
    if (fill1 < 0.f) fill1 = 0.f;
    if (fill1 > 1.f) fill1 = 1.f;

    /* Empty overlay panel: no Blood DataPanelLine. Use cat 0.
     * Do not write Blood / LifeBar1Green / LifeBar1Value. */
    int cat = 0;
    GameStr gkey, gtext;
    GameStrSet(&gkey, key1);
    GameStrSet(&gtext, bar1);
    int excepted = 0;
    LV_TRY { g_setLineProg(dest, &gkey, cat, fill1, &gtext, true); }
    LV_EXCEPT { excepted = 1; }
    if (excepted)
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvErr("LimbVigor: setLineProgress SEH on LifeBar1Datapanel — will retry, growth continues");
        }
        return;
    }
    if (lvLiveKeyCat(dest, key1) < 0)
    {
        lvWhyOnce("LimbVigor: setLineProgress on LifeBar1Datapanel did not create a line — not guess-painting Goal/State");
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
        LV_TRY { g_setLineProg(dest, &lkey, cat, fill2, &ltext, true); }
        LV_EXCEPT { excepted = 1; }
        if (excepted)
        {
            static int once = 0;
            if (!once)
            {
                once = 1;
                LvErr("LimbVigor: setLineProgress SEH — stump row, will retry, growth continues");
            }
            return;
        }
        for (int i = 0; kLimbKeys[i]; ++i)
        {
            if (!KeyEqI(kLimbKeys[i], limbKey))
            {
                int oc = lvLiveKeyCat(dest, kLimbKeys[i]);
                if (oc >= 0)
                    lvRemoveKey(dest, kLimbKeys[i], oc);
            }
        }
    }
    else
    {
        for (int i = 0; kLimbKeys[i]; ++i)
        {
            int oc = lvLiveKeyCat(dest, kLimbKeys[i]);
            if (oc >= 0)
                lvRemoveKey(dest, kLimbKeys[i], oc);
        }
    }
    {
        int oc = lvLiveKeyCat(dest, "Regrowth");
        if (oc >= 0) lvRemoveKey(dest, "Regrowth", oc);
        oc = lvLiveKeyCat(dest, "Wait");
        if (oc >= 0) lvRemoveKey(dest, "Wait", oc);
    }

    if (!g_paintLogged)
    {
        g_paintLogged = 1;
        LvLogf("LimbVigor: setLineProgress %s cat=%d %s on LifeBar1Datapanel %p",
               key1, cat, bar1, (void*)dest);
        if (bar2[0] && limbKey[0])
            LvLogf("LimbVigor: setLineProgress %s cat=%d %s", limbKey, cat, bar2);
    }
}
