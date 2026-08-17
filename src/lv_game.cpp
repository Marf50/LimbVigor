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
/* Read-only MyGUI walk via MyGUIEngine exports. No createWidget.
 * getName/getChildCount/getChildAt/getParent/getVisible live in the DLL
 * (not inline). Names are GameStr-read — we never touch std::string.
 * No GetRealAddress on virtuals. */

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
static int        g_myguiOk  = 0;
static int        g_myguiDump = 0;

static void* lvProc(HMODULE mod, const char* a, const char* b)
{
    void* p = (void*)GetProcAddress(mod, a);
    if (!p && b)
        p = (void*)GetProcAddress(mod, b);
    return p;
}

static int lvResolveMyGui()
{
    if (g_myguiOk)
        return 1;
    HMODULE mod = GetModuleHandleA("MyGUIEngine_x64.dll");
    if (!mod)
        mod = GetModuleHandleA("MyGUIEngine.dll");
    if (!mod)
    {
        LvLog("LimbVigor: mygui dump skipped — MyGUIEngine not loaded");
        return 0;
    }
    g_guiInst = (FnGuiInst)lvProc(mod,
        "?getInstancePtr@Gui@MyGUI@@SAPEAV12@XZ",
        "?getInstancePtr@Gui@MyGUI@@SAPAV12@XZ");
    g_wName = (FnGetName)lvProc(mod,
        "?getName@Widget@MyGUI@@QEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ",
        "?getName@Widget@MyGUI@@QBEABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ");
    g_wCount = (FnChildN)lvProc(mod,
        "?getChildCount@Widget@MyGUI@@QEAA_KXZ",
        "?getChildCount@Widget@MyGUI@@QAEIXZ");
    g_wAt = (FnChildAt)lvProc(mod,
        "?getChildAt@Widget@MyGUI@@QEAAPEAV12@_K@Z",
        "?getChildAt@Widget@MyGUI@@QAEPAV12@I@Z");
    g_wParent = (FnParent)lvProc(mod,
        "?getParent@Widget@MyGUI@@QEBAPEAV12@XZ",
        "?getParent@Widget@MyGUI@@QBEPAV12@XZ");
    g_wVis = (FnVisible)lvProc(mod,
        "?getVisible@Widget@MyGUI@@QEBA_NXZ",
        "?getVisible@Widget@MyGUI@@QBE_NXZ");
    if (!g_guiInst || !g_wName || !g_wCount || !g_wAt || !g_wParent || !g_wVis)
    {
        LvLog("LimbVigor: mygui dump skipped — exports missing");
        return 0;
    }
    g_myguiOk = 1;
    return 1;
}

static int lvWidgetName(void* w, char* out, int n)
{
    if (!w || !g_wName || !out || n < 2)
        return 0;
    out[0] = 0;
    const void* s = nullptr;
    int seh = 0;
    LV_TRY { s = g_wName(w); }
    LV_EXCEPT { s = nullptr; seh = 1; }
    if (seh || !s)
        return 0;
    GameStrRead(s, out, n);
    return 1;
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
    return lvWidgetName(w, n, (int)sizeof(n));
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

static void lvDumpMyGuiOnce()
{
    if (g_myguiDump)
        return;
    g_myguiDump = 1;
    if (!LvWorldInGame())
        return;
    if (!lvResolveMyGui())
        return;

    void* gui = nullptr;
    LV_TRY { gui = g_guiInst(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui)
    {
        LvLog("LimbVigor: mygui dump skipped — Gui instance null");
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
        LvLog("LimbVigor: mygui dump — no root widgets on Gui");
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

        char name[96];
        char type[64];
        char pname[96];
        name[0] = 0;
        type[0] = 0;
        pname[0] = 0;
        lvWidgetName(w, name, (int)sizeof(name));
        lvWidgetType(w, type, (int)sizeof(type));
        void* par = nullptr;
        LV_TRY { par = g_wParent(w); }
        LV_EXCEPT { par = nullptr; }
        if (par)
            lvWidgetName(par, pname, (int)sizeof(pname));
        int vis = 0;
        LV_TRY { vis = g_wVis(w) ? 1 : 0; }
        LV_EXCEPT { vis = 0; }
        std::size_t kids = 0;
        LV_TRY { kids = g_wCount(w); }
        LV_EXCEPT { kids = 0; }
        if (kids > 256)
            kids = 256;

        if (logged < kLog)
        {
            LvLogf("LimbVigor: mygui name='%s' type='%s' parent='%s' visible=%d children=%u",
                   name, type[0] ? type : "?", pname, vis, (unsigned)kids);
            logged++;
        }
        lvEnqueueKids(w, q, &qn, kMax, seen, seenN);
    }
    if (logged >= kLog)
        LvLogf("LimbVigor: mygui tree dump capped at %d (visited=%d queued=%d)",
               kLog, seenN, qn);
    LvLogf("LimbVigor: mygui tree dump end visited=%d logged=%d", seenN, logged);
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
