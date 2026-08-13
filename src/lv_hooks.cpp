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

static CharSnap* Bind(MedicalSystem* med)
{
    CharSnap tmp;
    std::memset(&tmp, 0, sizeof(tmp));
    for (int i = 0; i < LIMB_COUNT; ++i) tmp.lastStage[i] = -1;
    LvReadSnap(med, &tmp);
    if (!tmp.name[0]) std::snprintf(tmp.name, sizeof(tmp.name), "%s", "?");

    CharSnap* live = LvPersistFind(tmp.name, 1);
    if (!live) return nullptr;

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
        // Fresh amputation — reset that slot's growth.
        if ((tmp.limbs[i] == LIMB_KIND_STUMP || tmp.limbs[i] == LIMB_KIND_CRUSHED)
            && was == LIMB_KIND_WHOLE)
        {
            live->progress[i] = 0.f;
            live->lastStage[i] = -1;
            LvMarkDirty();
            Character* me = LvCharFromMed(med);
            if (tmp.race == RACE_HIVE)
                LvSay(me, "The stump itches. Hemolymph will try to knit it.");
            else if (tmp.race == RACE_SHEK)
                LvSay(me, "The bone remembers. Survive. Stay fed.");
            else if (tmp.race == RACE_SKELETON)
                LvSay(me, "A machine does not grow flesh. Find a replacement.");
            else
                LvSay(me, "Flesh does not grow back on its own. You need a splint — or to have earned it.");
        }
        if (tmp.limbs[i] == LIMB_KIND_WHOLE && was != LIMB_KIND_WHOLE
            && live->progress[i] < 99.f)
        {
            // Game restored it (prosthetic removed / vanilla). Drop our bar.
            live->progress[i] = 0.f;
            live->lastStage[i] = -1;
        }
    }
    return live;
}

static void DriveTick(MedicalSystem* med, float frameTime)
{
    if (!med || !LvCfg().enableHooks) return;
    if (IsDead(med)) return;
    if (g_inTick) return;

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
            // Unused splint in the pack still unlocks a blocked human/shek.
            if (live->catalystHours <= 0.f)
            {
                Character* me = LvCharFromMed(med);
                if (LvHasSplint(me) && LvAnyStump(live))
                {
                    char why[96];
                    if (!LvEligible(live, why, (int)sizeof(why)))
                    {
                        TickResult cat = {};
                        LvApplyCatalyst(live, &cat);
                        LvMarkDirty();
                        if (cat.speech[0]) LvSay(me, cat.speech);
                    }
                }
            }

            TickResult r = {};
            LvTick(live, dtHours, &r);
            LvMarkDirty();

            Character* me = LvCharFromMed(med);
            if (r.speech[0]) LvSay(me, r.speech);

            if (r.restored >= 0)
            {
                if (!LvRestoreLimb(med, r.restored))
                {
                    // Keep progress just under done so we retry next ticks.
                    live->limbs[r.restored] = LIMB_KIND_STUMP;
                    live->progress[r.restored] = 99.5f;
                    LvLog("LimbVigor: restore deferred — will retry");
                }
                else
                {
                    LvLogf("LimbVigor: restored limb %d on %s", r.restored, live->name);
                }
            }
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
    LV_TRY
    {
        CharSnap* live = Bind(self);
        if (live) LvPaintHud(self, panel, live);
    }
    LV_EXCEPT
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: HUD SEH — turning HUD off"); once = 1; }
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
                TickResult cat = {};
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
