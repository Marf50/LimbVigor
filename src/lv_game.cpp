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
        LvLog("LimbVigor: dump saw live key Blood — not painting this build");
    else
        LvLog("LimbVigor: dump: no DatapanelGUI has live key Blood — not painting");
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
    if (med)
        lvDumpOnePanel((DatapanelGUI*)med, "medicalPanel");
    else
        LvLog("LimbVigor: dump medicalPanel=null");

    lvDumpSummary();
}

#if !defined(LIMBVIGOR_IDE)
/* v1.17: bind Gui's Singleton getInstancePtr (exact x64 symbol, not
 * the first getInstancePtr — ClipboardManager stole that in v1.16),
 * walk that Gui's widget tree, read Kenshi_MainPanel.layout from the
 * game/workshop disk. Do not treat DatapanelGUI* as Widget*.
 * No createWidget. Names via GameStr. No GetRealAddress on virtuals. */

typedef void*         (*FnGuiInst)();
typedef const void*   (*FnGetName)(void* self);
typedef std::size_t   (*FnChildN)(void* self);
typedef void*         (*FnChildAt)(void* self, std::size_t i);
typedef void*         (*FnParent)(void* self);
typedef unsigned char (*FnVisible)(void* self);

static FnGuiInst  g_guiInst  = nullptr;
static FnGetName  g_wName    = nullptr;
static FnChildN   g_wCount   = nullptr;
static FnChildAt  g_wAt      = nullptr;
static FnParent   g_wParent  = nullptr;
static FnVisible  g_wVis     = nullptr;
static int        g_exportDumped = 0;
static int        g_layoutDumped = 0;
static int        g_treeDumped = 0;

/* MSVC x64 MyGUI: Gui's Singleton, not ClipboardManager / InputManager.
   v1.16 used lvHasI(..., "Gui") which matches the "Gui" inside "MyGUI". */
static const char kGuiGetInstanceExact[] =
    "?getInstancePtr@?$Singleton@VGui@MyGUI@@@MyGUI@@SAPEAVGui@2@XZ";

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

/* Gui's Singleton getInstancePtr only. Do not take the first
 * getInstancePtr — "Gui" is a substring of every "MyGUI" symbol. */
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

static int lvExportWanted(const char* n)
{
    return (lvHasI(n, "Widget")
         || lvHasI(n, "getName")
         || lvHasI(n, "getChild")
         || lvHasI(n, "getParent")
         || lvHasI(n, "getByName")
         || lvHasI(n, "getInstancePtr")) ? 1 : 0;
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
    if (!g_wAt && lvHasI(n, "getChildAt") && lvHasI(n, "Widget"))
    {
        g_wAt = (FnChildAt)addr;
        LvLogf("LimbVigor: mygui bind getChildAt '%s'", n);
    }
    if (!g_wParent && lvHasI(n, "getParent") && lvHasI(n, "Widget")
     && !lvHasI(n, "getParentSize") && !lvHasI(n, "getParentInfo"))
    {
        g_wParent = (FnParent)addr;
        LvLogf("LimbVigor: mygui bind getParent '%s'", n);
    }
    if (!g_wVis && lvHasI(n, "getVisible") && lvHasI(n, "Widget")
     && !lvHasI(n, "Inherited") && !lvHasI(n, "setVisible"))
    {
        g_wVis = (FnVisible)addr;
        LvLogf("LimbVigor: mygui bind getVisible '%s'", n);
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

    const unsigned char* base = (const unsigned char*)(const void*)mod;
    IMAGE_DOS_HEADER dos;
    std::memcpy(&dos, base, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000)
    {
        LvLog("LimbVigor: mygui export list — bad DOS header");
        return;
    }
    IMAGE_NT_HEADERS64 nt;
    std::memcpy(&nt, base + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE)
    {
        LvLog("LimbVigor: mygui export list — bad NT header");
        return;
    }
    const IMAGE_DATA_DIRECTORY dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress || dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY))
    {
        LvLog("LimbVigor: mygui export list — no export directory");
        return;
    }
    IMAGE_EXPORT_DIRECTORY exp;
    std::memcpy(&exp, base + dir.VirtualAddress, sizeof(exp));
    if (!exp.NumberOfNames || exp.NumberOfNames > 20000)
    {
        LvLogf("LimbVigor: mygui export list — NumberOfNames=%u", (unsigned)exp.NumberOfNames);
        return;
    }

    FARPROC exact = GetProcAddress(mod, kGuiGetInstanceExact);
    if (exact)
    {
        g_guiInst = (FnGuiInst)exact;
        LvLogf("LimbVigor: mygui bind getInstancePtr EXACT '%s'", kGuiGetInstanceExact);
    }
    else
        LvLog("LimbVigor: mygui exact Gui getInstancePtr missing — scanning exports");

    const DWORD* names = (const DWORD*)(base + exp.AddressOfNames);
    const WORD* ords = (const WORD*)(base + exp.AddressOfNameOrdinals);
    const DWORD* funcs = (const DWORD*)(base + exp.AddressOfFunctions);
    int loggedWidget = 0;
    int loggedGetInst = 0;
    const int widgetCap = 80;
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
        /* Scan every export so Gui's getInstancePtr cannot hide behind a cap. */
        lvTryBindExport(en, addr);
        if (lvHasI(en, "getInstancePtr"))
        {
            LvLogf("LimbVigor: mygui export getInstancePtr '%s' %p%s",
                   en, addr,
                   lvIsGuiSingletonGetInstance(en) ? " [Gui Singleton]" : "");
            loggedGetInst++;
        }
        else if (lvExportWanted(en) && loggedWidget < widgetCap)
        {
            LvLogf("LimbVigor: mygui export '%s' %p", en, addr);
            loggedWidget++;
        }
    }
    if (loggedWidget >= widgetCap)
        LvLogf("LimbVigor: mygui Widget export log capped at %d (scan uncapped names=%u)",
               widgetCap, (unsigned)exp.NumberOfNames);
    LvLogf("LimbVigor: mygui export names=%u getInstancePtr listed=%d Widget listed=%d bind name=%d childN=%d childAt=%d parent=%d vis=%d gui=%d",
           (unsigned)exp.NumberOfNames, loggedGetInst, loggedWidget,
           g_wName ? 1 : 0, g_wCount ? 1 : 0, g_wAt ? 1 : 0,
           g_wParent ? 1 : 0, g_wVis ? 1 : 0, g_guiInst ? 1 : 0);
    if (!g_wName) LvLog("LimbVigor: mygui bind getName missing");
    if (!g_guiInst) LvLog("LimbVigor: mygui bind Gui getInstancePtr missing after full export scan");
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
    return n >= 1 ? 1 : 0;
}

static int lvWidgetName(void* w, char* out, int n)
{
    if (!w || !out || n < 2)
        return 0;
    out[0] = 0;
    if (g_wName)
    {
        const void* s = nullptr;
        int seh = 0;
        LV_TRY { s = g_wName(w); }
        LV_EXCEPT { s = nullptr; seh = 1; }
        if (!seh && s)
        {
            GameStrRead(s, out, n);
            return 1;
        }
    }
    for (int off = 0x20; off <= 0x180; off += 8)
    {
        char buf[96];
        buf[0] = 0;
        LV_TRY { GameStrRead((const char*)w + off, buf, (int)sizeof(buf)); }
        LV_EXCEPT { buf[0] = 0; }
        if (lvGameStrLooksName(buf))
        {
            std::snprintf(out, (size_t)n, "%s", buf);
            return 1;
        }
    }
    return 0;
}

static int lvLooksLikeType(const char* s)
{
    if (!s || !s[0])
        return 0;
    int n = 0;
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')))
        return 0;
    for (; s[n]; ++n)
    {
        char c = s[n];
        if (n > 40)
            return 0;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return n >= 3 ? 1 : 0;
}

/* IObject::getTypeName is vtable[1] after the dtor. Call that slot only.
 * Do not probe other slots (could run a destructor or a write). */
static void lvWidgetType(void* w, char* out, int n)
{
    if (out && n > 0)
        out[0] = 0;
    if (!w || !out || n < 2)
        return;
    void** vt = nullptr;
    LV_TRY { std::memcpy(&vt, w, sizeof(vt)); }
    LV_EXCEPT { vt = nullptr; }
    if (!vt || !vt[1])
        return;
    char buf[64];
    buf[0] = 0;
    int ok = 0;
    LV_TRY
    {
        const void* s = ((FnGetName)vt[1])(w);
        if (s && GameStrRead(s, buf, (int)sizeof(buf)) && lvLooksLikeType(buf))
            ok = 1;
    }
    LV_EXCEPT { ok = 0; }
    if (ok)
        std::snprintf(out, (size_t)n, "%s", buf);
}

static int lvIsWidget(void* w)
{
    if (!w)
        return 0;
    char n[8];
    if (g_wName)
        return lvWidgetName(w, n, (int)sizeof(n));
    void* vt = nullptr;
    LV_TRY { std::memcpy(&vt, w, sizeof(vt)); }
    LV_EXCEPT { vt = nullptr; }
    return vt ? 1 : 0;
}

static int lvVecAt(void* obj, int off, void*** start, int* count)
{
    void** s = nullptr;
    void** e = nullptr;
    void** c = nullptr;
    LV_TRY
    {
        std::memcpy(&s, (const char*)obj + off, sizeof(s));
        std::memcpy(&e, (const char*)obj + off + (int)sizeof(void*), sizeof(e));
        std::memcpy(&c, (const char*)obj + off + 2 * (int)sizeof(void*), sizeof(c));
    }
    LV_EXCEPT { return 0; }
    if (!s || !e || e < s || !c || c < e)
        return 0;
    const std::ptrdiff_t bytes = (char*)e - (char*)s;
    if (bytes <= 0 || (bytes % (int)sizeof(void*)) != 0)
        return 0;
    const int n = (int)(bytes / (int)sizeof(void*));
    if (n < 1 || n > 256)
        return 0;
    *start = s;
    *count = n;
    return 1;
}

static int lvSeenHas(void** seen, int n, void* p)
{
    for (int i = 0; i < n; ++i)
        if (seen[i] == p)
            return 1;
    return 0;
}

static void lvEnqueueKids(void* w, void** q, int* qn, int qmax, void** seen, int seenN)
{
    if (!w)
        return;
    if (g_wCount && g_wAt)
    {
        std::size_t n = 0;
        int seh = 0;
        LV_TRY { n = g_wCount(w); }
        LV_EXCEPT { n = 0; seh = 1; }
        if (!seh && n > 0 && n <= 256)
        {
            for (std::size_t i = 0; i < n && *qn < qmax; ++i)
            {
                void* c = nullptr;
                LV_TRY { c = g_wAt(w, i); }
                LV_EXCEPT { c = nullptr; }
                if (!c || lvSeenHas(seen, seenN, c) || lvSeenHas(q, *qn, c))
                    continue;
                if (lvIsWidget(c))
                    q[(*qn)++] = c;
            }
        }
    }
    for (int off = 0; off <= 0x1C0; off += (int)sizeof(void*))
    {
        void** start = nullptr;
        int n = 0;
        if (!lvVecAt(w, off, &start, &n))
            continue;
        int good = 0;
        for (int i = 0; i < n && i < 8; ++i)
        {
            if (lvIsWidget(start[i]))
                good++;
        }
        if (good < 1)
            continue;
        for (int i = 0; i < n && *qn < qmax; ++i)
        {
            void* c = start[i];
            if (!c || lvSeenHas(seen, seenN, c) || lvSeenHas(q, *qn, c))
                continue;
            if (lvIsWidget(c))
                q[(*qn)++] = c;
        }
    }
}

static void lvLogWidget(const char* tag, void* w)
{
    if (!w)
        return;
    char name[96];
    char type[64];
    name[0] = 0;
    type[0] = 0;
    lvWidgetName(w, name, (int)sizeof(name));
    lvWidgetType(w, type, (int)sizeof(type));
    int vis = -1;
    if (g_wVis)
    {
        LV_TRY { vis = g_wVis(w) ? 1 : 0; }
        LV_EXCEPT { vis = -1; }
    }
    void* parent = nullptr;
    if (g_wParent)
    {
        LV_TRY { parent = g_wParent(w); }
        LV_EXCEPT { parent = nullptr; }
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
    LvLogf("LimbVigor: %s %p name='%s' type='%s' parent=%p visible=%d children=%d",
           tag, w, name, type[0] ? type : "?", parent, vis, kids);
}

static void lvDumpTreeIfPossible()
{
    if (g_treeDumped)
        return;
    g_treeDumped = 1;
    if (!g_guiInst)
    {
        LvLog("LimbVigor: mygui full tree skipped — Gui getInstancePtr not bound");
        return;
    }
    void* gui = nullptr;
    LV_TRY { gui = g_guiInst(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui)
    {
        LvLog("LimbVigor: mygui full tree skipped — Gui instance null");
        return;
    }

    const int kMax = 1500;
    const int kLog = 400;
    void* seen[1500];
    void* q[1500];
    int seenN = 0;
    int qn = 0;

    for (int off = 0; off <= 0x80; off += (int)sizeof(void*))
    {
        void** start = nullptr;
        int n = 0;
        if (!lvVecAt(gui, off, &start, &n))
            continue;
        int good = 0;
        for (int i = 0; i < n && i < 8; ++i)
            if (lvIsWidget(start[i]))
                good++;
        if (good < 1)
            continue;
        for (int i = 0; i < n && qn < kMax; ++i)
        {
            void* c = start[i];
            if (c && !lvSeenHas(q, qn, c) && lvIsWidget(c))
                q[qn++] = c;
        }
    }
    if (qn == 0)
    {
        LvLog("LimbVigor: mygui full tree — no root widgets on Gui");
        return;
    }

    LvLog("LimbVigor: mygui tree dump begin");
    int logged = 0;
    int qi = 0;
    while (qi < qn && seenN < kMax)
    {
        void* w = q[qi++];
        if (!w || lvSeenHas(seen, seenN, w))
            continue;
        seen[seenN++] = w;
        if (logged < kLog)
        {
            lvLogWidget("mygui", w);
            logged++;
        }
        lvEnqueueKids(w, q, &qn, kMax, seen, seenN);
    }
    if (logged >= kLog)
        LvLogf("LimbVigor: mygui tree dump capped at %d (visited=%d queued=%d)",
               kLog, seenN, qn);
    LvLogf("LimbVigor: mygui tree dump end visited=%d logged=%d", seenN, logged);
}

static int lvLayoutMedical(const char* name)
{
    return (lvHasI(name, "Blood")
         || lvHasI(name, "Head")
         || lvHasI(name, "Hunger")
         || lvHasI(name, "LifeBar")
         || lvHasI(name, "Progress")
         || lvHasI(name, "medical")) ? 1 : 0;
}

static void lvParseLayoutFile(const char* path)
{
    if (!path || !path[0])
        return;
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0) f = nullptr;
#else
    f = std::fopen(path, "rb");
#endif
    if (!f)
    {
        LvLogf("LimbVigor: layout miss '%s'", path);
        return;
    }
    if (std::fseek(f, 0, SEEK_END) != 0)
    {
        std::fclose(f);
        return;
    }
    long sz = std::ftell(f);
    if (sz < 32 || sz > 2 * 1024 * 1024)
    {
        std::fclose(f);
        LvLogf("LimbVigor: layout skip size=%ld '%s'", sz, path);
        return;
    }
    char* buf = (char*)std::malloc((size_t)sz + 1);
    if (!buf)
    {
        std::fclose(f);
        return;
    }
    std::rewind(f);
    size_t got = std::fread(buf, 1, (size_t)sz, f);
    std::fclose(f);
    buf[got] = 0;
    LvLogf("LimbVigor: layout read '%s' bytes=%u", path, (unsigned)got);

    int hits = 0;
    const int cap = 80;
    for (char* p = buf; *p; ++p)
    {
        if (p[0] != 'n' || p[1] != 'a' || p[2] != 'm' || p[3] != 'e' || p[4] != '=')
            continue;
        char q = p[5];
        if (q != '"' && q != '\'')
            continue;
        p += 6;
        char name[96];
        int n = 0;
        while (p[n] && p[n] != q && n < 95)
        {
            name[n] = p[n];
            ++n;
        }
        name[n] = 0;
        if (!lvLayoutMedical(name))
            continue;
        char typ[64];
        typ[0] = 0;
        char* line = p;
        while (line > buf && *line != '<')
            --line;
        char* end = p;
        while (*end && *end != '>')
            ++end;
        for (char* t = line; t < end; ++t)
        {
            if (t[0] == 't' && t[1] == 'y' && t[2] == 'p' && t[3] == 'e' && t[4] == '=')
            {
                char tq = t[5];
                if (tq != '"' && tq != '\'')
                    continue;
                t += 6;
                int k = 0;
                while (t[k] && t[k] != tq && k < 63)
                {
                    typ[k] = t[k];
                    ++k;
                }
                typ[k] = 0;
                break;
            }
        }
        if (hits < cap)
        {
            LvLogf("LimbVigor: layout widget name='%s' type='%s'", name, typ[0] ? typ : "?");
            hits++;
        }
    }
    if (hits >= cap)
        LvLogf("LimbVigor: layout widget list capped at %d", cap);
    LvLogf("LimbVigor: layout medical-name hits=%d", hits);
    std::free(buf);
}

static void lvDirOf(char* path)
{
    if (!path)
        return;
    char* slash = nullptr;
    for (char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/') slash = p;
    if (slash) *slash = 0;
}

static int lvFileExistsA(const char* path)
{
    if (!path || !path[0])
        return 0;
    const DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

static int lvPathEqI(const char* a, const char* b)
{
    if (!a || !b)
        return 0;
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca == '/') ca = '\\';
        if (cb == '/') cb = '\\';
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return (!*a && !*b) ? 1 : 0;
}

static void lvAddRoot(char roots[][MAX_PATH], int* n, int maxn, const char* dir)
{
    if (!dir || !dir[0] || !n || *n >= maxn)
        return;
    for (int i = 0; i < *n; ++i)
        if (lvPathEqI(roots[i], dir))
            return;
    std::snprintf(roots[*n], MAX_PATH, "%s", dir);
    (*n)++;
}

static void lvWalkUpGameRoots(const char* start, char roots[][MAX_PATH], int* n, int maxn)
{
    if (!start || !start[0])
        return;
    char cur[MAX_PATH];
    std::snprintf(cur, MAX_PATH, "%s", start);
    for (int up = 0; up < 8 && *n < maxn; ++up)
    {
        char exe[MAX_PATH];
        char lay[MAX_PATH];
        std::snprintf(exe, MAX_PATH, "%s\\kenshi_x64.exe", cur);
        std::snprintf(lay, MAX_PATH, "%s\\data\\gui\\layout\\Kenshi_MainPanel.layout", cur);
        if (lvFileExistsA(exe) || lvFileExistsA(lay))
            lvAddRoot(roots, n, maxn, cur);
        char parent[MAX_PATH];
        std::snprintf(parent, MAX_PATH, "%s", cur);
        lvDirOf(parent);
        if (!parent[0] || lvPathEqI(parent, cur))
            break;
        std::snprintf(cur, MAX_PATH, "%s", parent);
    }
}

static void lvScanKidsForLayout(const char* parent, int* nfound)
{
    if (!parent || !parent[0])
        return;
    char pat[MAX_PATH];
    std::snprintf(pat, MAX_PATH, "%s\\*", parent);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    int tried = 0;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        if (fd.cFileName[0] == '.')
            continue;
        char lay[MAX_PATH];
        std::snprintf(lay, MAX_PATH, "%s\\%s\\gui\\layout\\Kenshi_MainPanel.layout",
                      parent, fd.cFileName);
        if (lvFileExistsA(lay))
        {
            lvParseLayoutFile(lay);
            if (nfound) (*nfound)++;
        }
        tried++;
        if (tried >= 64)
            break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* v1.16 used kenshi_x64 / GetModuleHandle(nullptr) and landed in
   ...\Kenshi\RE_Kenshi\ — layout is under the game root, mods, or
   steamapps\workshop\content\233860. Disk read only; do not load. */
static void lvDumpLayoutOnce()
{
    if (g_layoutDumped)
        return;
    g_layoutDumped = 1;

    char roots[12][MAX_PATH];
    int nroots = 0;

    HMODULE mygui = lvMyGuiMod();
    if (mygui)
    {
        char p[MAX_PATH];
        p[0] = 0;
        GetModuleFileNameA(mygui, p, MAX_PATH);
        LvLogf("LimbVigor: layout anchor MyGUIEngine '%s'", p);
        lvDirOf(p);
        lvAddRoot(roots, &nroots, 12, p);
        lvWalkUpGameRoots(p, roots, &nroots, 12);
    }

    HMODULE k = GetModuleHandleA("kenshi_x64.exe");
    if (!k) k = GetModuleHandleA("kenshi_GOG_x64.exe");
    if (k)
    {
        char p[MAX_PATH];
        p[0] = 0;
        GetModuleFileNameA(k, p, MAX_PATH);
        LvLogf("LimbVigor: layout anchor kenshi exe '%s'", p);
        lvDirOf(p);
        lvAddRoot(roots, &nroots, 12, p);
    }

    const char* plug = LvPluginDir();
    if (plug && plug[0])
    {
        LvLogf("LimbVigor: layout anchor plugin '%s'", plug);
        lvWalkUpGameRoots(plug, roots, &nroots, 12);
    }

    char self[MAX_PATH];
    self[0] = 0;
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    if (self[0])
    {
        lvDirOf(self);
        lvWalkUpGameRoots(self, roots, &nroots, 12);
    }

    int nfound = 0;
    for (int i = 0; i < nroots; ++i)
    {
        LvLogf("LimbVigor: layout root '%s'", roots[i]);
        char lay[MAX_PATH];
        std::snprintf(lay, MAX_PATH, "%s\\data\\gui\\layout\\Kenshi_MainPanel.layout",
                      roots[i]);
        if (lvFileExistsA(lay))
        {
            lvParseLayoutFile(lay);
            nfound++;
        }
        else
            LvLogf("LimbVigor: layout miss '%s'", lay);

        char mods[MAX_PATH];
        std::snprintf(mods, MAX_PATH, "%s\\mods", roots[i]);
        lvScanKidsForLayout(mods, &nfound);

        char cur[MAX_PATH];
        std::snprintf(cur, MAX_PATH, "%s", roots[i]);
        for (int up = 0; up < 5; ++up)
        {
            char ws[MAX_PATH];
            std::snprintf(ws, MAX_PATH, "%s\\workshop\\content\\233860", cur);
            lvScanKidsForLayout(ws, &nfound);
            char parent[MAX_PATH];
            std::snprintf(parent, MAX_PATH, "%s", cur);
            lvDirOf(parent);
            if (!parent[0] || lvPathEqI(parent, cur))
                break;
            std::snprintf(cur, MAX_PATH, "%s", parent);
        }
    }
    LvLogf("LimbVigor: layout files found=%d roots=%d", nfound, nroots);
}

static void lvDumpMyGuiOnce()
{
    if (!LvWorldInGame())
        return;
    lvDumpMyGuiExports();
    lvDumpLayoutOnce();
    lvDumpTreeIfPossible();
}
#else
static void lvDumpMyGuiOnce() {}
#endif

void LvWalkSelPanel(DatapanelGUI* panel)
{
    if (!panel || !LvWorldInGame())
        return;
    lvDumpOnePanel(panel, "hook");
    lvDumpExtrasOnce();
    if (g_dumpBlood && !g_dumpSummary)
    {
        g_dumpSummary = 1;
        LvLog("LimbVigor: dump saw live key Blood — not painting this build");
    }
    else
        lvDumpSummary();
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
    /* Only remove keys we can see on a live line. lineExists lies. */
    for (int i = 0; kOurs[i]; ++i)
    {
        int cat = lvLiveKeyCat(panel, kOurs[i]);
        if (cat >= 0)
            lvRemoveKey(panel, kOurs[i], cat);
    }
}

void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap)
{
    (void)med;
    (void)snap;
    /* v1.15 probe: never paint. */
    if (!panel || !LvWorldInGame())
        return;
    if (lvLiveKeyCat(panel, "Blood") < 0)
        return;
    if (!g_paintLogged)
    {
        g_paintLogged = 1;
        LvLog("LimbVigor: dump saw live key Blood — not painting this build");
    }
}
