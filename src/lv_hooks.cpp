#include "lv_hooks.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_game.h"
#include "lv_persist.h"
#include "lv_hud.h"
#include "lv_parts.h"

#if defined(LIMBVIGOR_IDE)
#include "stubs/kenshi_ide_stubs.h"
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Character.h>
#include <kenshi/Item.h>
#include <kenshi/gui/DatapanelGUI.h>
#endif

#include <cstring>
#include <cstdio>
#include <cstddef>

#if defined(_MSC_VER)
#define LV_TRY    __try
#define LV_EXCEPT __except (1)
#else
#define LV_TRY    if (true)
#define LV_EXCEPT if (false)
#endif

static void (*orig_medUpdate)(MedicalSystem*, float) = nullptr;
static void (*orig_medGui)(MedicalSystem*, DatapanelGUI*) = nullptr;
static void (*orig_charGui)(Character*, DatapanelGUI*, int) = nullptr;
static bool (*orig_doctor)(MedicalSystem*, float, Item*, float, Character*) = nullptr;
static void (*orig_tip1)(InventoryItemBase*, void*) = nullptr;

static CharSnap g_hudSnap;
static int      g_hudHave = 0;

static int g_inTick = 0;
static int g_loggedInGame = 0;
static int g_oncePlayer = 0;

static unsigned NowMs()
{
#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
    return GetTickCount();
#else
    return 0;
#endif
}

static int IsDead(MedicalSystem* med)
{
    if (!med) return 1;
    int d = 0;
    LV_TRY { d = med->isDead() ? 1 : 0; }
    LV_EXCEPT { d = 0; }
    if (!d)
    {
        LV_TRY { d = med->dead ? 1 : 0; }
        LV_EXCEPT { d = 0; }
    }
    return d;
}

static void Heartbeat(CharSnap* live)
{
    if (!live || !LvCfg().debugLog) return;
    const unsigned now = NowMs();
    if (live->lastLogMs && now && (now - live->lastLogMs) < 15000u) return;
    live->lastLogMs = now ? now : 1;

    char line[192];
    LvHeartbeatLine(live, line, (int)sizeof(line));
    if (line[0])
        LvLogf("LimbVigor: %s", line);

    const int stump = LvFirstStump(live);
    if (stump < 0)
        return;
    /* Persist 100% on a live stump already put the block/retry copy on line 1. */
    if (live->progress[stump] >= 99.5f || live->lastStage[stump] == LV_PART_GROWN)
        return;
    char why[96];
    if (!LvEligible(live, why, (int)sizeof(why)) && why[0])
        LvLog(why);
}

static CharSnap* Bind(MedicalSystem* med)
{
    CharSnap tmp;
    std::memset(&tmp, 0, sizeof(tmp));
    for (int i = 0; i < LIMB_COUNT; ++i) tmp.lastStage[i] = -1;
    LvReadSnap(med, &tmp);
    if (!tmp.name[0]) std::snprintf(tmp.name, sizeof(tmp.name), "%s", "?");

    if (tmp.name[0] == '?' && tmp.name[1] == 0)
        return nullptr;

    CharSnap* live = LvPersistFind(tmp.name, 1);
    if (!live) return nullptr;

    Character* speaker = LvCharFromMed(med);

    const int firstSeen = !live->seen;
    live->seen = 1;
    live->race = tmp.race;
    live->toughness = tmp.toughness;
    live->medic = tmp.medic;
    live->blood = tmp.blood;
    live->maxBlood = tmp.maxBlood;
    live->bleedRate = tmp.bleedRate;
    live->fed = tmp.fed;
    live->starving = tmp.starving;
    live->inBed = tmp.inBed;
    live->inCombat = tmp.inCombat;
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        const LimbKind was = live->limbs[i];
        live->limbs[i] = tmp.limbs[i];
        live->limbHp[i] = tmp.limbHp[i];
        live->limbMax[i] = tmp.limbMax[i];
        if (firstSeen)
        {
            /* Already-missing limb on load: do not bark. */
            // Empty -15 socket reads as a stump. If progress already
            // finished, keep 100% so Sync slots LV Grown immediately.
            if ((tmp.limbs[i] == LIMB_KIND_STUMP || tmp.limbs[i] == LIMB_KIND_CRUSHED)
                && (live->lastStage[i] == LV_PART_GROWN || live->progress[i] >= 99.5f))
            {
                live->progress[i] = 100.f;
                live->lastStage[i] = LV_PART_GROWN;
            }
            continue;
        }

        if ((tmp.limbs[i] == LIMB_KIND_STUMP || tmp.limbs[i] == LIMB_KIND_CRUSHED)
            && was == LIMB_KIND_WHOLE)
        {
            if (live->progress[i] >= 99.f)
                continue;
            live->progress[i] = 0.f;
            live->lastStage[i] = -1;
            LvMarkDirty();
            if (tmp.race == RACE_HIVE)
                LvSay(speaker, "The stump itches. It will grow.");
            else if (tmp.race == RACE_SHEK)
                LvSay(speaker, "The stump wants a fight, or a splint.");
            else if (tmp.race == RACE_HUMAN)
                LvSay(speaker, "The stump will not grow on its own.");
        }
        if (tmp.limbs[i] == LIMB_KIND_WHOLE && was != LIMB_KIND_WHOLE)
        {
            if (live->progress[i] >= 99.f)
            {
                live->progress[i] = 0.f;
                live->lastStage[i] = -1;
            }
        }
    }
    return live;
}

static void LogSkip(const char* why)
{
    static char last[128];
    static unsigned lastMs = 0;
    if (!why || !why[0]) return;
    const unsigned now = NowMs();
    if (last[0] && std::strcmp(last, why) == 0 && lastMs && now && (now - lastMs) < 15000u)
        return;
    std::snprintf(last, sizeof(last), "%s", why);
    lastMs = now ? now : 1;
    LvLogf("LimbVigor: skip — %s", why);
}

static void DriveTick(MedicalSystem* med, float frameTime)
{
    if (!med || !LvCfg().enableHooks) return;
    if (!LvWorldInGame()) return;
    if (!g_loggedInGame)
    {
        g_loggedInGame = 1;
        LvLog("LimbVigor: In-game");
    }
    if (IsDead(med))
    {
        if (!g_oncePlayer) LogSkip("dead");
        return;
    }
    if (g_inTick)
    {
        static int rec = 0;
        if (!rec)
        {
            rec = 1;
            LvErr("LimbVigor: medical tick latch recovered — growth continues");
        }
        g_inTick = 0;
    }

    Character* who = nullptr;
    LV_TRY { who = LvCharFromMed(med); }
    LV_EXCEPT { who = nullptr; }
    if (!who)
    {
        LogSkip("no character on medical state");
        return;
    }
    if (!LvIsPlayerSquad(who))
    {
        if (!g_oncePlayer) LogSkip("no player squad");
        return;
    }

    float secPerHour = LvCfg().secondsPerGameHour;
    if (secPerHour < 1.f) secPerHour = 53.33f;
    float dtHours = frameTime / secPerHour;
    if (dtHours <= 0.f) return;
    if (dtHours > 2.f) dtHours = 2.f;

    g_inTick = 1;

    CharSnap* live = nullptr;
    LV_TRY { live = Bind(med); }
    LV_EXCEPT
    {
        live = nullptr;
        static int once = 0;
        if (!once) { LvErr("LimbVigor: bind SEH"); once = 1; }
    }

    if (!live)
    {
        LogSkip("bind failed (no name)");
    }
    else
    {
        if (!g_oncePlayer)
        {
            g_oncePlayer = 1;
            LvLog("LimbVigor: player squad seen — I-key snap live, ticks on, parts on");
        }

        /* I-key follows the body the medical panel last drew.
         * Do not stamp every ticking squad pawn into g_hudSnap. */
        if (g_hudHave && live->name[0]
            && std::strcmp(g_hudSnap.name, live->name) == 0)
        {
            g_hudSnap = *live;
            LvHudNote(live);
        }

        if (live->race == RACE_SKELETON)
        {
            LogSkip("skeleton — vigor not applied");
        }
        else if (live->race == RACE_ANIMAL)
        {
            LogSkip("animal — vigor not applied");
        }
        else
        {
            TickResult r;
            LvClearResult(&r);
            LV_TRY
            {
                LvTick(live, dtHours, &r);
                LvMarkDirty();
            }
            LV_EXCEPT
            {
                static int once = 0;
                if (!once) { LvErr("LimbVigor: LvTick SEH — HUD isolated, will retry"); once = 1; }
            }

            LV_TRY
            {
                if (r.speech[0]) LvSay(who, r.speech);
            }
            LV_EXCEPT
            {
                static int once = 0;
                if (!once) { LvErr("LimbVigor: say SEH — growth continues"); once = 1; }
            }

            /* Per-limb SEH so one AV does not skip the growth tick. */
            for (int li = 0; li < LIMB_COUNT; ++li)
            {
                LV_TRY { LvSyncOneLimb(med, live, li); }
                LV_EXCEPT
                {
                    static int once[LIMB_COUNT] = {};
                    if (!once[li])
                    {
                        once[li] = 1;
                        LvLogf("LimbVigor: parts SEH site=LvSyncOneLimb %s hp=%.1f/%.1f kind=%d — growth tick continues",
                            LvLimbLabel((LimbId)li), live->limbHp[li], live->limbMax[li],
                            (int)live->limbs[li]);
                    }
                }
            }

            if (r.stageChanged >= 0 && r.stageChanged < LIMB_COUNT)
            {
                const int st = r.stageValue;
                const int limb = r.stageChanged;
                LV_TRY
                {
                    if (st >= 0 && st < LV_PART_COUNT)
                        LvEquipGrowthPart(med, limb, st);
                }
                LV_EXCEPT
                {
                    static int once = 0;
                    if (!once)
                    {
                        once = 1;
                        LvLogf("LimbVigor: parts SEH site=stageEquip %s hp=%.1f/%.1f — growth numbers kept",
                            LvLimbLabel((LimbId)limb), live->limbHp[limb], live->limbMax[limb]);
                    }
                }
            }

            if (r.restored >= 0 && r.restored < LIMB_COUNT && live->restoreLock <= 0.f)
            {
                const int limb = r.restored;
                LV_TRY
                {
                    CharSnap now;
                    std::memset(&now, 0, sizeof(now));
                    LvReadSnap(med, &now);
                    const LimbKind gameLimb = now.limbs[limb];
                    if (gameLimb == LIMB_KIND_WHOLE || gameLimb == LIMB_KIND_PROSTHETIC)
                    {
                        live->limbs[limb] = gameLimb;
                        live->progress[limb] = 0.f;
                        live->lastStage[limb] = -1;
                        LvLogf("LimbVigor: %s %s already attached — clearing 100%%",
                            live->name, LvLimbLabel((LimbId)limb));
                    }
                    else if (LvEquipGrowthPart(med, limb, LV_PART_GROWN))
                    {
                        live->limbs[limb] = LIMB_KIND_WHOLE;
                        live->progress[limb] = 0.f;
                        live->lastStage[limb] = LV_PART_GROWN;
                        if (!r.speech[0])
                        {
                            char grown[96];
                            std::snprintf(grown, sizeof(grown), "The %s has grown back.",
                                LvLimbLabel((LimbId)limb));
                            LvSay(who, grown);
                        }
                        LvLogf("LimbVigor: slotted grown part on %s %s",
                            live->name, LvLimbLabel((LimbId)limb));
                    }
                    else
                    {
                        live->limbs[limb] = LIMB_KIND_STUMP;
                        live->progress[limb] = 100.f;
                        live->restoreLock = 20.f / secPerHour;
                        LvEquipGrowthPart(med, limb, LV_PART_KNITTING);
                        LvLogf("LimbVigor: grown part deferred on %s %s — knitting stays",
                            live->name, LvLimbLabel((LimbId)limb));
                    }
                }
                LV_EXCEPT
                {
                    static int once = 0;
                    if (!once)
                    {
                        once = 1;
                        LvLogf("LimbVigor: parts SEH site=restore %s hp=%.1f/%.1f — growth numbers kept",
                            LvLimbLabel((LimbId)limb), live->limbHp[limb], live->limbMax[limb]);
                    }
                }
            }

            /* Persist 100% / Grown but the socket is still a stump: retry LV Grown.
             * Re-read the socket the player sees. Do not call setLimb(ORIGINAL). */
            LV_TRY
            {
                CharSnap now;
                std::memset(&now, 0, sizeof(now));
                LvReadSnap(med, &now);
                for (int i = 0; i < LIMB_COUNT; ++i)
                {
                    if (live->progress[i] < 99.5f && live->lastStage[i] != LV_PART_GROWN)
                        continue;
                    if (now.limbs[i] != LIMB_KIND_STUMP && now.limbs[i] != LIMB_KIND_CRUSHED)
                        continue;
                    const int slotted = LvEquipGrowthPart(med, i, LV_PART_GROWN);
                    CharSnap again;
                    std::memset(&again, 0, sizeof(again));
                    LvReadSnap(med, &again);
                    if (again.limbs[i] != LIMB_KIND_STUMP && again.limbs[i] != LIMB_KIND_CRUSHED)
                        continue;
                    static unsigned lastWhyMs[LIMB_COUNT] = {};
                    const unsigned t = NowMs();
                    if (lastWhyMs[i] && t && (t - lastWhyMs[i]) < 15000u)
                        continue;
                    lastWhyMs[i] = t ? t : 1;
                    if (slotted)
                        LvLogf("LimbVigor: %s %s persist 100%% still a stump — LV Grown on, game still reads stump",
                            live->name, LvLimbLabel((LimbId)i));
                    else
                        LvLogf("LimbVigor: %s %s persist 100%% still a stump — LV Grown retry failed",
                            live->name, LvLimbLabel((LimbId)i));
                }
            }
            LV_EXCEPT {}

            LV_TRY { Heartbeat(live); }
            LV_EXCEPT {}
        }
    }

    LV_TRY { LvPersistSave(0); }
    LV_EXCEPT {}
    g_inTick = 0;
}

static void hook_medUpdate(MedicalSystem* self, float frameTime)
{
    if (orig_medUpdate) orig_medUpdate(self, frameTime);
    if (!self) return;
    LvNoteMedicalPulse();
    DriveTick(self, frameTime);
}

static void WalkSeh(DatapanelGUI* panel)
{
    LV_TRY { LvWalkSelPanel(panel); }
    LV_EXCEPT
    {
        LvNoteHudProbeSeh();
        static int once = 0;
        if (!once) { LvErr("LimbVigor: GUI probe SEH — growth continues"); once = 1; }
    }
}

static void WalkSafe(DatapanelGUI* panel)
{
    try { WalkSeh(panel); }
    catch (...)
    {
        static int once = 0;
        LvNoteHudProbeSeh();
        if (!once) { LvErr("LimbVigor: GUI probe C++ throw — growth continues"); once = 1; }
    }
}

static void PaintSeh(MedicalSystem* med, Character* who, CharSnap* live)
{
    LV_TRY
    {
        if (!who || !LvIsSelectedCharacter(who) || (live && live->race == RACE_ANIMAL))
            LvClearHud(nullptr);
        else if (live)
            LvPaintHud(med, nullptr, live);
    }
    LV_EXCEPT
    {
        LvNoteHudProbeSeh();
        static int once = 0;
        if (!once) { LvErr("LimbVigor: HUD paint SEH — growth continues"); once = 1; }
    }
}

static void PaintSafe(MedicalSystem* med, Character* who, CharSnap* live)
{
    try { PaintSeh(med, who, live); }
    catch (...)
    {
        LvNoteHudProbeSeh();
        static int once = 0;
        if (!once) { LvErr("LimbVigor: HUD paint C++ throw — growth continues"); once = 1; }
    }
}

static void AfterGuiRebuild(MedicalSystem* med, DatapanelGUI* panel, Character* who)
{
    if (!panel)
        return;
    /* Load / title: do not touch DatapanelGUI. v1.9.7 ctor/font/update hooks died here. */
    if (!LvWorldInGame())
        return;

    WalkSafe(panel);

    CharSnap* live = nullptr;
    if (med)
        live = Bind(med);
    if (live && who && LvIsPlayerSquad(who))
    {
        g_hudSnap = *live;
        g_hudHave = 1;
        LvHudNote(live);
    }

    /* After orig: show LifeBar10* + ISub on Datapanel + numeric Value + Green fill. */
    PaintSafe(med, who, live);
}

static void hook_medGui(MedicalSystem* self, DatapanelGUI* panel)
{
    if (orig_medGui) orig_medGui(self, panel);
    if (!self || !panel) return;
    /* Do not touch Character or write DatapanelGUI until In-game. */
    if (!LvWorldInGame())
        return;
    Character* who = LvCharFromMed(self);
    LV_TRY { AfterGuiRebuild(self, panel, who); }
    LV_EXCEPT
    {
        LvNoteHudProbeSeh();
        static int once = 0;
        if (!once) { LvErr("LimbVigor: medical GUI SEH"); once = 1; }
    }
}

static void hook_charGui(Character* self, DatapanelGUI* panel, int cat)
{
    if (orig_charGui) orig_charGui(self, panel, cat);
    if (!self || !panel) return;
    if (!LvWorldInGame())
        return;
    MedicalSystem* med = LvMedFromChar(self);
    LV_TRY { AfterGuiRebuild(med, panel, self); }
    LV_EXCEPT
    {
        LvNoteHudProbeSeh();
        static int once = 0;
        if (!once) { LvErr("LimbVigor: _NV_getGUIData SEH"); once = 1; }
    }
}

static bool hook_doctor(MedicalSystem* self, float skill, Item* equipment, float dt, Character* who)
{
    bool ok = false;
    if (orig_doctor) ok = orig_doctor(self, skill, equipment, dt, who);
    if (!ok || !self) return ok;
    if (!LvWorldInGame()) return ok;
    LV_TRY
    {
        CharSnap* live = Bind(self);
        if (live && live->race != RACE_SKELETON && live->race != RACE_ANIMAL)
        {
            if (LvItemLooksLikeCatalyst(equipment) && live->catalystHours < 1.f)
            {
                TickResult cat;
                LvApplyCatalyst(live, &cat);
                LvMarkDirty();
                Character* me = LvCharFromMed(self);
                if (cat.speech[0]) LvSay(me, cat.speech);
                LvLog("LimbVigor: doctoring catalyst applied");
            }
        }
    }
    LV_EXCEPT {}
    return ok;
}

// StringPair: vtable + s1@0x8 + s2@0x30 + float@0x58. Ogre/MSVC vector = 3 pointers.
static int WriteGameStrInPlace(void* strObj, const char* text)
{
    if (!strObj || !text) return 0;
    char* p = (char*)strObj;
    size_t cap = 0;
    std::memcpy(&cap, p + 24, sizeof(cap));
    if (cap < 24 || cap > (size_t)1 << 16) return 0;
    size_t n = std::strlen(text);
    if (n > cap) n = cap;
    if (n < 8) return 0;
    char* src = nullptr;
    if (cap > 15)
        std::memcpy(&src, p, sizeof(src));
    else
        src = p;
    if (!src) return 0;
    std::memcpy(src, text, n);
    src[n] = 0;
    std::memcpy(p + 16, &n, sizeof(n));
    return 1;
}

static void RewriteTooltipLines(void* linesVec, const char* text)
{
    if (!linesVec || !text || !text[0]) return;
    struct OgVec { char* first; char* last; char* end; };
    OgVec* v = (OgVec*)linesVec;
    if (!v->first || !v->last || v->last <= v->first) return;
    const ptrdiff_t bytes = v->last - v->first;
    if (bytes < 0x50 || bytes > 0x60 * 24) return;
    static const int kSizes[] = { 0x60, 0x68, 0x58, 0x70, 0 };
    int pair = 0;
    for (int i = 0; kSizes[i]; ++i)
    {
        if (bytes % kSizes[i] == 0)
        {
            const int n = (int)(bytes / kSizes[i]);
            if (n > 0 && n < 28) { pair = kSizes[i]; break; }
        }
    }
    if (!pair) return;

    char* best = nullptr;
    size_t bestCap = 0;
    for (char* p = v->first; p + pair <= v->last; p += pair)
    {
        char* s2 = p + 0x30;
        size_t cap = 0;
        std::memcpy(&cap, s2 + 24, sizeof(cap));
        if (cap > bestCap && cap >= 24 && cap < (size_t)1 << 16)
        {
            bestCap = cap;
            best = s2;
        }
    }
    if (best)
        WriteGameStrInPlace(best, text);
}

static void hook_tip1(InventoryItemBase* self, void* lines)
{
    if (orig_tip1) orig_tip1(self, lines);
    if (!self || !lines) return;
    LV_TRY
    {
        if (!LvIsGrowthPart((Item*)self)) return;
        if (!g_hudHave) return;
        char text[256];
        LvItemTooltipText(&g_hudSnap, text, (int)sizeof(text));
        if (text[0]) RewriteTooltipLines(lines, text);
    }
    LV_EXCEPT {}
}

static int HookOne(const char* label, intptr_t addr, void* detour, void** orig)
{
#if defined(LIMBVIGOR_IDE)
    (void)label; (void)addr; (void)detour; (void)orig;
    return 0;
#else
    if (!addr)
    {
        LvErr("LimbVigor: missing address");
        LvErr(label);
        return 0;
    }
    if (KenshiLib::SUCCESS != KenshiLib::AddHook((void*)addr, detour, orig))
    {
        LvErr("LimbVigor: AddHook failed");
        LvErr(label);
        return 0;
    }
    LvLog(label);
    return 1;
#endif
}

void LvInstallHooks()
{
    if (!LvCfg().enableHooks)
    {
        LvLog("LimbVigor: hooks disabled in config");
        return;
    }

#if defined(LIMBVIGOR_IDE)
    LvLog("LimbVigor: IDE build — no hooks");
    return;
#else
    intptr_t med = KenshiLib::GetRealAddress(&MedicalSystem::medicalUpdate);
    intptr_t gui = KenshiLib::GetRealAddress(&MedicalSystem::getMedicalGUIData);
    intptr_t cgui = KenshiLib::GetRealAddress(&Character::_NV_getGUIData);
    intptr_t doc = KenshiLib::GetRealAddress(&MedicalSystem::applyDoctoring);

    void* base = nullptr;
    {
        HMODULE exe = GetModuleHandleA(nullptr);
        if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
        if (!exe) exe = GetModuleHandleA("kenshi_GOG_x64.exe");
        if (exe) base = (void*)exe;
    }
    if (!med && base) med = (intptr_t)((unsigned char*)base + 0x651880);
    if (!gui && base) gui = (intptr_t)((unsigned char*)base + 0x889140);
    if (!cgui && base) cgui = (intptr_t)((unsigned char*)base + 0x5D3AE0);
    if (!doc && base) doc = (intptr_t)((unsigned char*)base + 0x649280);

    HookOne("LimbVigor: medicalUpdate", med, (void*)hook_medUpdate, (void**)&orig_medUpdate);
    HookOne("LimbVigor: getMedicalGUIData", gui, (void*)hook_medGui, (void**)&orig_medGui);
    HookOne("LimbVigor: _NV_getGUIData", cgui, (void*)hook_charGui, (void**)&orig_charGui);
    HookOne("LimbVigor: applyDoctoring (splint)", doc, (void*)hook_doctor, (void**)&orig_doctor);

    LvHudInstall();

    // NEVER GetRealAddress on InventoryItemBase::getTooltipData1.
    // It is virtual. The compiler emits a thunk in LimbVigor.dll, KenshiLib
    // asserts "Incorrect address … LimbVigor.dll+0x????" and the game dies
    // at plugin load (v1.8.3 crash). Use the documented Item override RVA.
    if (base)
    {
        intptr_t tip = (intptr_t)((unsigned char*)base + 0x7A8E30);
        HookOne("LimbVigor: item tooltip (I-key hover)", tip, (void*)hook_tip1, (void**)&orig_tip1);
    }
    else
    {
        LvLog("LimbVigor: no exe base — I-key tooltip skipped");
    }
#endif
}
