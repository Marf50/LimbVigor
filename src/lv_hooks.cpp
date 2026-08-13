#include "lv_hooks.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_game.h"
#include "lv_persist.h"

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

#if defined(_MSC_VER)
#define LV_TRY    __try
#define LV_EXCEPT __except (1)
#else
#define LV_TRY    if (true)
#define LV_EXCEPT if (false)
#endif

static void (*orig_medUpdate)(MedicalSystem*, float) = nullptr;
static void (*orig_medGui)(MedicalSystem*, DatapanelGUI*) = nullptr;
static bool (*orig_doctor)(MedicalSystem*, float, Item*, float, Character*) = nullptr;

static int g_inTick = 0;
static unsigned g_warmStart = 0;

static int WarmedUp()
{
#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
    unsigned now = GetTickCount();
    if (!g_warmStart) g_warmStart = now;
    return (now - g_warmStart) >= 5000u ? 1 : 0;
#else
    return 1;
#endif
}

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

    const int stump = LvFirstStump(live);
    char why[96];
    const int ok = LvEligible(live, why, (int)sizeof(why));
    if (stump < 0)
    {
        LvLogf("LimbVigor: %s  %s %.0f/%.0f  no stump",
            live->name, LvResourceName(live->race), live->vigor, LvCfg().maxVigor);
        return;
    }
    LvLogf("LimbVigor: %s  %s %.0f/%.0f  %s %.0f%% %s%s",
        live->name,
        LvResourceName(live->race),
        live->vigor, LvCfg().maxVigor,
        LvLimbLabel((LimbId)stump),
        live->progress[stump],
        LvStageName(live->progress[stump]),
        ok ? "" : "  BLOCKED");
    if (!ok && why[0]) LvLog(why);
}

static CharSnap* Bind(MedicalSystem* med)
{
    CharSnap tmp;
    std::memset(&tmp, 0, sizeof(tmp));
    for (int i = 0; i < LIMB_COUNT; ++i) tmp.lastStage[i] = -1;
    LvReadSnap(med, &tmp);
    if (!tmp.name[0]) std::snprintf(tmp.name, sizeof(tmp.name), "%s", "?");

    // Never persist unnamed / failed-name reads as one shared "?" slot.
    if (tmp.name[0] == '?' && tmp.name[1] == 0)
        return nullptr;

    CharSnap* live = LvPersistFind(tmp.name, 1);
    if (!live) return nullptr;

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
        if (firstSeen) continue;

        // Finished growth but the game still reports a stump: do not treat
        // as a fresh cut and do not wipe 100% progress.
        if ((tmp.limbs[i] == LIMB_KIND_STUMP || tmp.limbs[i] == LIMB_KIND_CRUSHED)
            && was == LIMB_KIND_WHOLE)
        {
            if (live->progress[i] >= 99.f)
                continue;
            live->progress[i] = 0.f;
            live->lastStage[i] = -1;
            LvMarkDirty();
            if (tmp.race == RACE_HIVE)
                LvSay(nullptr, "The stump itches. Hemolymph will try to knit it.");
            else if (tmp.race == RACE_SHEK)
                LvSay(nullptr, "The bone remembers. Survive. Stay fed.");
            else if (tmp.race == RACE_SKELETON)
                LvSay(nullptr, "A machine does not grow flesh. Find a replacement.");
            else
                LvSay(nullptr, "Flesh does not grow back on its own. You need a splint — or to have earned it.");
        }
        if (tmp.limbs[i] == LIMB_KIND_WHOLE && was != LIMB_KIND_WHOLE)
        {
            if (live->progress[i] >= 99.f)
            {
                live->progress[i] = 0.f;
                live->lastStage[i] = -1;
            }
            else if (live->progress[i] < 99.f && live->progress[i] > 0.f)
            {
                // Prosthetic or vanilla heal mid-growth — keep the number.
            }
        }
    }
    return live;
}

static void DriveTick(MedicalSystem* med, float frameTime)
{
    if (!med || !LvCfg().enableHooks) return;
    if (!WarmedUp()) return;
    if (IsDead(med)) return;
    if (g_inTick) return;

    Character* who = LvCharFromMed(med);
    if (!LvIsPlayerSquad(who)) return;

    float secPerHour = LvCfg().secondsPerGameHour;
    if (secPerHour < 1.f) secPerHour = 53.33f;
    float dtHours = frameTime / secPerHour;
    if (dtHours <= 0.f) return;
    if (dtHours > 2.f) dtHours = 2.f;

    g_inTick = 1;
    LV_TRY
    {
        CharSnap* live = Bind(med);
        if (live && live->race != RACE_SKELETON && live->race != RACE_ANIMAL)
        {
            TickResult r;
            LvTick(live, dtHours, &r);
            LvMarkDirty();

            static int onceTick = 0;
            if (!onceTick) { LvLog("LimbVigor: first player-squad tick"); onceTick = 1; }

            if (r.speech[0]) LvSay(who, r.speech);

            // restored is -1 unless a stump actually hit 100%. Never treat 0 as "yes".
            if (r.restored >= 0 && r.restored < LIMB_COUNT && live->restoreLock <= 0.f)
            {
                const int limb = r.restored;
                if (live->limbs[limb] != LIMB_KIND_STUMP && live->limbs[limb] != LIMB_KIND_CRUSHED)
                {
                    LvLogf("LimbVigor: skip restore limb %d on %s — not a stump", limb, live->name);
                }
                else if (!LvRestoreLimb(med, limb))
                {
                    live->limbs[limb] = LIMB_KIND_STUMP;
                    live->progress[limb] = 99.5f;
                    live->restoreLock = 20.f / secPerHour; // ~20 real seconds
                    LvLogf("LimbVigor: restore deferred on %s limb %d — will retry", live->name, limb);
                }
                else
                {
                    live->limbs[limb] = LIMB_KIND_WHOLE;
                    live->progress[limb] = 0.f;
                    live->lastStage[limb] = -1;
                    LvLogf("LimbVigor: restored limb %d on %s", limb, live->name);
                }
            }

            Heartbeat(live);
        }
        LvPersistSave(0);
    }
    LV_EXCEPT
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: medical tick SEH"); once = 1; }
    }
    g_inTick = 0;
}

static void hook_medUpdate(MedicalSystem* self, float frameTime)
{
    if (orig_medUpdate) orig_medUpdate(self, frameTime);
    if (!self) return;
    DriveTick(self, frameTime);
}

static void hook_medGui(MedicalSystem* self, DatapanelGUI* panel)
{
    if (orig_medGui) orig_medGui(self, panel);
    if (!self || !panel || !LvCfg().enableHud) return;
    if (!WarmedUp()) return;
    Character* who = LvCharFromMed(self);
    if (!LvIsPlayerSquad(who)) return;
    LV_TRY
    {
        CharSnap* live = Bind(self);
        if (live) LvPaintHud(self, panel, live);
    }
    LV_EXCEPT
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: HUD SEH — turning HUD off"); once = 1; }
        LvDisableHud();
    }
}

static bool hook_doctor(MedicalSystem* self, float skill, Item* equipment, float dt, Character* who)
{
    bool ok = false;
    if (orig_doctor) ok = orig_doctor(self, skill, equipment, dt, who);
    if (!ok || !self) return ok;
    LV_TRY
    {
        CharSnap* live = Bind(self);
        if (live && live->race != RACE_SKELETON && live->race != RACE_ANIMAL)
        {
            if (LvItemLooksLikeCatalyst(equipment) || equipment)
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
    intptr_t doc = KenshiLib::GetRealAddress(&MedicalSystem::applyDoctoring);

    // Documented RVAs from KenshiLib headers, only if GetRealAddress fails.
    void* base = nullptr;
    {
        HMODULE exe = GetModuleHandleA(nullptr);
        if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
        if (exe) base = (void*)exe;
    }
    if (!med && base) med = (intptr_t)((unsigned char*)base + 0x651880);
    if (!gui && base) gui = (intptr_t)((unsigned char*)base + 0x889140);
    if (!doc && base) doc = (intptr_t)((unsigned char*)base + 0x649280);

    HookOne("LimbVigor: medicalUpdate", med, (void*)hook_medUpdate, (void**)&orig_medUpdate);
    HookOne("LimbVigor: getMedicalGUIData", gui, (void*)hook_medGui, (void**)&orig_medGui);
    HookOne("LimbVigor: applyDoctoring (splint)", doc, (void*)hook_doctor, (void**)&orig_doctor);
#endif
}
