#include "lv_sim.h"
#include "lv_config.h"
#include "lv_parts.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_fail = 0;

static void Expect(int cond, const char* msg)
{
    if (!cond)
    {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fail;
    }
    else
        std::printf("ok  %s\n", msg);
}

static CharSnap Hive()
{
    CharSnap c;
    std::memset(&c, 0, sizeof(c));
    c.race = RACE_HIVE;
    c.vigor = 40.f;
    c.fed = 1;
    c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_STUMP;
    for (int i = 0; i < LIMB_COUNT; ++i) c.lastStage[i] = -1;
    std::snprintf(c.name, sizeof(c.name), "%s", "Worker");
    return c;
}

int main()
{
    LvLoadConfig("");

    {
        CharSnap c = Hive();
        char why[96];
        Expect(LvEligible(&c, why, 96) == 1, "hive is eligible without a kit");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_SKELETON;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "skeleton is never eligible");
        Expect(std::strstr(why, "flesh") != nullptr, "skeleton explains itself");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "low-stat human is blocked");
        TickResult r;
        LvApplyCatalyst(&c, &r);
        Expect(r.restored == -1, "catalyst does not restore a limb");
        Expect(LvEligible(&c, why, 96) == 1, "splint unlocks the human");
        Expect(c.catalystHours > 0.f, "catalyst timer is set");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_SHEK;
        c.toughness = 19;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "shek 19 blocked");
        c.toughness = 20;
        Expect(LvEligible(&c, why, 96) == 1, "shek 20 grows");
    }

    {
        CharSnap c = Hive();
        TickResult r;
        LvTick(&c, 0.1f, &r);
        Expect(r.restored == -1, "a tenth of an hour does not restore");
        Expect(c.progress[LIMB_RIGHT_LEG] > 0.f, "a tenth of an hour grows");
        Expect(c.limbs[LIMB_RIGHT_LEG] == LIMB_KIND_STUMP, "stump stays a stump mid-growth");
    }

    {
        // The old memset bug: TickResult.restored was 0, so every tick
        // "restored" the right leg. Zero-init must not look like a restore.
        TickResult blank;
        std::memset(&blank, 0, sizeof(blank));
        Expect(blank.restored == 0, "raw memset restored==0 (the bug)");
        LvClearResult(&blank);
        Expect(blank.restored == -1, "LvClearResult sets restored=-1");
    }

    {
        CharSnap c = Hive();
        c.vigor = 80.f;
        // Open-air hive growth ~1.70 / hour → 100 needs ~59 hours
        for (int i = 0; i < 80; ++i)
            LvTick(&c, 1.f, nullptr);
        Expect(c.limbs[LIMB_RIGHT_LEG] == LIMB_KIND_WHOLE, "hive finishes a leg in under 80h");
    }

    {
        CharSnap c = Hive();
        c.progress[LIMB_RIGHT_LEG] = 99.9f;
        c.vigor = 80.f;
        TickResult r;
        LvTick(&c, 1.f, &r);
        Expect(r.restored == LIMB_RIGHT_LEG, "completing tick reports the stump");
        Expect(c.limbs[LIMB_RIGHT_LEG] == LIMB_KIND_WHOLE, "completing tick marks the limb whole");
        Expect(c.lastStage[LIMB_RIGHT_LEG] == 4, "completing tick announces once");
        TickResult r2;
        LvTick(&c, 1.f, &r2);
        Expect(r2.speech[0] == 0, "second complete tick stays quiet");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        c.vigor = 80.f;
        float before = c.progress[LIMB_RIGHT_LEG];
        TickResult r;
        LvTick(&c, 5.f, &r);
        Expect(c.progress[LIMB_RIGHT_LEG] == before, "blocked human does not grow");
        Expect(r.restored == -1, "blocked human does not restore");
        Expect(c.vigor > 0.f, "blocked human still fills vigor");
    }

    {
        Expect(std::strstr(LvRaceHint(RACE_HIVE), "fed") != nullptr, "hive hint mentions fed");
        CharSnap c = Hive();
        char eta[96];
        LvEtaText(&c, eta, 96);
        Expect(eta[0] != 0, "eligible hive has an ETA");
        Expect(std::strstr(eta, "~") != nullptr && std::strstr(eta, "h") != nullptr,
            "growing ETA is a short ~Nh tag");
        c.race = RACE_HUMAN;
        c.toughness = 1;
        c.medic = 1;
        LvEtaText(&c, eta, 96);
        Expect(std::strstr(eta, "Need a Splint Kit") != nullptr,
            "blocked human ETA is the Line 2 reason");
    }

    {
        /* QA: Hemolymph 72/100 then left leg  budding 30%  ~Xh bed */
        CharSnap c = Hive();
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.limbs[LIMB_LEFT_LEG] = LIMB_KIND_STUMP;
        c.progress[LIMB_LEFT_LEG] = 30.f;
        c.vigor = 72.f;
        c.inBed = 1;
        char bar1[96], bar2[96], tip[64], hover[256];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(LvHudResourceKey(&c), "Hemolymph") == 0, "hive Line 1 key is Hemolymph");
        Expect(std::strcmp(bar1, "72 / 100") == 0, "hive Line 1 right is 72 / 100");
        Expect(std::strcmp(tip, "left leg") == 0, "hive Line 2 key is left leg");
        Expect(std::strstr(bar2, "budding 30%") == bar2, "hive Line 2 starts budding 30%");
        Expect(std::strstr(bar2, "~") != nullptr && std::strstr(bar2, "bed") != nullptr,
            "hive Line 2 has ~Nh bed");
        Expect(std::strstr(bar2, "Regrowth") == nullptr && std::strstr(bar2, "Wait") == nullptr,
            "Line 2 is not Regrowth/Wait");
        Expect(f2 > 0.29f && f2 < 0.31f, "growing Line 2 fill is 30%");
        LvItemTooltipText(&c, hover, 256);
        Expect(std::strstr(hover, "budding") != nullptr, "I-key tooltip has stage");
    }

    {
        /* QA: Vigor 40/100 then left leg  Need a Splint Kit... */
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        c.vigor = 40.f;
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.limbs[LIMB_LEFT_LEG] = LIMB_KIND_STUMP;
        c.progress[LIMB_LEFT_LEG] = 30.f;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(LvHudResourceKey(&c), "Vigor") == 0, "human Line 1 key is Vigor");
        Expect(std::strcmp(bar1, "40 / 100") == 0, "human Line 1 right is 40 / 100");
        Expect(std::strcmp(tip, "left leg") == 0, "blocked Line 2 key is left leg");
        Expect(std::strcmp(bar2, "Need a Splint Kit, or toughness 40 and medic 25.") == 0,
            "human blocked Line 2 is the Designer reason");
        Expect(std::strstr(bar2, "30%") == nullptr, "blocked Line 2 has no percent");
        Expect(f2 == 0.f, "blocked Line 2 fill is empty");
    }

    {
        /* QA: Battle-heat 88/100 then right arm  forming 55%  heat */
        CharSnap c = Hive();
        c.race = RACE_SHEK;
        c.toughness = 20;
        c.vigor = 88.f;
        c.inCombat = 1;
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.limbs[LIMB_RIGHT_ARM] = LIMB_KIND_STUMP;
        c.progress[LIMB_RIGHT_ARM] = 55.f;
        c.lastStage[LIMB_RIGHT_ARM] = 2;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(LvHudResourceKey(&c), "Battle-heat") == 0, "shek Line 1 key is Battle-heat");
        Expect(std::strcmp(bar1, "88 / 100") == 0, "shek Line 1 right is 88 / 100");
        Expect(std::strcmp(tip, "right arm") == 0, "shek Line 2 key is right arm");
        Expect(std::strcmp(bar2, "forming 55%  heat") == 0, "shek combat Line 2 is forming 55%  heat");
        std::snprintf(c.name, sizeof(c.name), "%s", "ShekHeat");
        TickResult heat;
        LvTick(&c, 0.1f, &heat);
        Expect(std::strstr(heat.speech, "heat is in it") != nullptr, "shek first combat tick speaks heat");
        TickResult heat2;
        LvTick(&c, 0.1f, &heat2);
        Expect(std::strstr(heat2.speech, "heat is in it") == nullptr, "shek heat speech is once");
    }

    {
        /* QA: Limb Vigor  Frames do not grow flesh. */
        CharSnap c = Hive();
        c.race = RACE_SKELETON;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(LvHudResourceKey(&c), "Limb Vigor") == 0, "skeleton Line 1 key is Limb Vigor");
        Expect(std::strcmp(bar1, "Frames do not grow flesh.") == 0, "skeleton Line 1 is frames copy");
        Expect(bar2[0] == 0 && tip[0] == 0, "skeleton has no Line 2");
        Expect(f1 == 0.f, "skeleton Line 1 fill is 0");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_ANIMAL;
        char bar1[96], bar2[96], tip[64];
        float f1 = 1.f, f2 = 1.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(bar1[0] == 0 && bar2[0] == 0, "animal paints nothing");
        Expect(LvHudResourceKey(&c)[0] == 0, "animal has no Line 1 key");
    }

    {
        CharSnap c = Hive();
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.progress[LIMB_RIGHT_LEG] = 100.f;
        c.lastStage[LIMB_RIGHT_LEG] = 4;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(bar2[0] == 0, "100% Grown drops Line 2");
        Expect(std::strcmp(bar1, "40 / 100") == 0, "idle Line 1 still paints with no stump");
    }

    {
        /* Persist 100% while the socket is still a stump: not Grown. */
        CharSnap c = Hive();
        std::snprintf(c.name, sizeof(c.name), "%s", "Boop");
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.limbs[LIMB_LEFT_LEG] = LIMB_KIND_STUMP;
        c.progress[LIMB_LEFT_LEG] = 100.f;
        c.lastStage[LIMB_LEFT_LEG] = 4;
        c.vigor = 100.f;
        c.starving = 1;
        char bar1[96], bar2[96], tip[64], beat[192];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(tip, "left leg") == 0, "persist 100% stump keeps Line 2");
        Expect(std::strcmp(bar2, "Starving. Nothing left to grow with.") == 0,
            "starving copy is the block line, not grown BLOCKED");
        Expect(std::strstr(bar2, "grown") == nullptr, "do not paint grown on a live stump");
        Expect(std::strstr(bar2, "100%") == nullptr, "do not paint 100% on a live stump");
        Expect(f2 == 0.f, "blocked persist-100% stump fill is empty");
        LvHeartbeatLine(&c, beat, 192);
        Expect(std::strstr(beat, "Starving. Nothing left to grow with.") != nullptr,
            "heartbeat starving is the block line");
        Expect(std::strstr(beat, "grown") == nullptr, "heartbeat does not say grown on a stump");
        Expect(std::strstr(beat, "BLOCKED") == nullptr, "heartbeat does not say BLOCKED when persist 100%");
        c.starving = 0;
        c.fed = 1;
        LvHeartbeatLine(&c, beat, 192);
        Expect(std::strstr(beat, "still a stump") != nullptr, "heartbeat retries LV Grown on a persist-100% stump");
        Expect(std::strstr(beat, "grown  BLOCKED") == nullptr, "heartbeat does not say 100% grown BLOCKED");
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strstr(bar2, "grown") == nullptr, "eligible persist-100% stump is not grown 100%");
        Expect(std::strstr(bar2, "knitting") == bar2, "eligible persist-100% stump shows knitting");
    }

    {
        CharSnap c = Hive();
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_WHOLE;
        c.limbs[LIMB_LEFT_LEG] = LIMB_KIND_STUMP;
        c.limbs[LIMB_RIGHT_ARM] = LIMB_KIND_STUMP;
        c.progress[LIMB_LEFT_LEG] = 30.f;
        c.vigor = 80.f;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(tip, "left leg") == 0, "legs first for Line 2");
        Expect(std::strstr(bar2, "then right arm") != nullptr, "next waiting stump is then right arm");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        c.catalystHours = 12.f;
        c.vigor = 80.f;
        c.progress[LIMB_RIGHT_LEG] = 30.f;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strstr(bar2, "splint 12h") != nullptr, "active splint tags Line 2");
        c.catalystHours = 0.f;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "expired splint blocks a low-stat human");
    }

    {
        CharSnap c = Hive();
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_PROSTHETIC;
        c.progress[LIMB_RIGHT_LEG] = 40.f;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(tip, "right leg") == 0, "metal Line 2 key is the socket");
        Expect(std::strcmp(bar2, "Metal. Progress kept.") == 0, "metal pauses and keeps progress");
        Expect(f2 == 0.f, "metal Line 2 fill is empty");
    }

    {
        CharSnap c = Hive();
        c.inBed = 1;
        c.vigor = 80.f;
        c.progress[LIMB_RIGHT_LEG] = 90.f;
        char eta[96];
        LvEtaText(&c, eta, 96);
        Expect(std::strstr(eta, "bed") != nullptr, "bed ETA mentions the bed");
        c.inBed = 0;
        c.bleedRate = 50.f;
        LvEtaText(&c, eta, 96);
        Expect(std::strcmp(eta, "Too much bleeding. Bandage first.") == 0,
            "bleed ETA is the Line 2 reason");
    }

    {
        CharSnap c = Hive();
        c.vigor = 0.f;
        char bar1[96], bar2[96], tip[64];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 64);
        Expect(std::strcmp(bar2, "Hemolymph is spent. Eat or rest.") == 0,
            "spent Line 2 is Designer copy");
        c.race = RACE_HUMAN;
        c.toughness = 40;
        c.medic = 25;
        c.fed = 0;
        TickResult r;
        LvTick(&c, 1.f, &r);
        Expect(std::strcmp(r.speech, "Vigor is spent.") == 0, "spent speech is Designer short copy");
        TickResult r2;
        LvTick(&c, 1.f, &r2);
        Expect(r2.speech[0] == 0, "spent speech is once");
    }

    {
        TickResult cat;
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        LvApplyCatalyst(&c, &cat);
        Expect(std::strcmp(cat.speech, "The splint takes.") == 0, "splint speech is Designer copy");
    }

    {
        const LvPartDef* stump = LvPartFor(LIMB_LEFT_LEG, LV_PART_STUMP);
        const LvPartDef* knit = LvPartFor(LIMB_RIGHT_ARM, LV_PART_KNITTING);
        const LvPartDef* grown = LvPartFor(LIMB_LEFT_LEG, LV_PART_GROWN);
        Expect(stump && std::strcmp(stump->desc, "A raw stump. Almost no push-off.") == 0,
            "stump I-key is Designer copy");
        Expect(knit && std::strcmp(knit->desc, "Almost a limb. Soft. Do not test it.") == 0,
            "knitting I-key is Designer copy");
        Expect(grown && std::strcmp(grown->desc, "A new limb. Soft. Yours.") == 0,
            "grown I-key is Designer copy");
    }

    {
        Expect(LvPartStageFromProgress(0.f) == LV_PART_STUMP, "0% is stump");
        Expect(LvPartStageFromProgress(24.f) == LV_PART_STUMP, "24% is stump");
        Expect(LvPartStageFromProgress(25.f) == LV_PART_BUDDING, "25% is budding");
        Expect(LvPartStageFromProgress(49.f) == LV_PART_BUDDING, "49% is budding");
        Expect(LvPartStageFromProgress(50.f) == LV_PART_FORMING, "50% is forming");
        Expect(LvPartStageFromProgress(75.f) == LV_PART_KNITTING, "75% is knitting");
        Expect(LvPartStageFromProgress(99.f) == LV_PART_KNITTING, "99% is knitting");
        Expect(LvPartStageFromProgress(100.f) == LV_PART_GROWN, "100% is grown");
        const LvPartDef* p = LvPartFor(LIMB_LEFT_LEG, LV_PART_BUDDING);
        Expect(p && std::strstr(p->name, "LV ") == p->name, "left-leg budding is named LV");
        Expect(p && std::strstr(p->stringId, "lv-") == p->stringId, "stringId is lv-");
        Expect(LvPartFor(LIMB_RIGHT_ARM, LV_PART_KNITTING) != nullptr, "right-arm knitting exists");
        Expect(LvPartFor(LIMB_LEFT_LEG, LV_PART_GROWN) != nullptr, "left-leg grown exists");
        Expect(LvPartFor(LIMB_LEFT_LEG, LV_PART_GROWN)
            && std::strstr(LvPartFor(LIMB_LEFT_LEG, LV_PART_GROWN)->name, "Grown"),
            "grown part is named Grown");
        {
            int missing = 0;
            for (int limb = 0; limb < LIMB_COUNT; ++limb)
            {
                for (int st = 0; st < LV_PART_COUNT; ++st)
                {
                    const LvPartDef* d = LvPartFor(limb, st);
                    if (!d || !d->mesh || !std::strstr(d->mesh, ".mesh")
                     || !d->icon || !d->icon[0])
                        ++missing;
                }
            }
            Expect(missing == 0, "all 20 parts have FileValue mesh/icon paths");
        }
        Expect(LvPartFor(-1, 0) == nullptr, "bad limb rejected");
        Expect(LvPartSlotForLimb(LIMB_LEFT_ARM) == 0, "left arm FCS slot 0");
        Expect(LvPartSlotForLimb(LIMB_RIGHT_LEG) == 3, "right leg FCS slot 3");
    }

    {
        CharSnap c = Hive();
        c.starving = 1;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "starving hive is blocked");
        char eta[96];
        LvEtaText(&c, eta, 96);
        Expect(std::strcmp(eta, "Starving. Nothing left to grow with.") == 0,
            "starving ETA mentions starve / nothing left");
        const float before = c.progress[LIMB_RIGHT_LEG];
        TickResult r;
        LvTick(&c, 1.f, &r);
        Expect(c.progress[LIMB_RIGHT_LEG] == before, "starving hive does not grow");
        Expect(r.restored == -1, "starving hive does not restore");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_ANIMAL;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "animal is never eligible");
        Expect(std::strcmp(why, "This body cannot regrow a limb.") == 0, "animal explains itself");
        TickResult r;
        LvTick(&c, 1.f, &r);
        Expect(c.progress[LIMB_RIGHT_LEG] == 0.f, "animal does not grow");
        Expect(r.restored == -1, "animal does not restore");
    }

    {
        CharSnap c = Hive();
        c.limbs[LIMB_RIGHT_LEG] = LIMB_KIND_PROSTHETIC;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 0, "no stump is not eligible");
        Expect(std::strcmp(why, "No stump to grow from. Remove a prosthetic first.") == 0,
            "no stump says remove a prosthetic first");
        Expect(LvFirstStump(&c) == -1, "prosthetic is not a stump");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        c.catalystHours = 2.5f;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 1, "catalyst makes the human eligible");
        const float start = c.catalystHours;
        LvTick(&c, 1.f, nullptr);
        Expect(c.catalystHours < start && c.catalystHours > 0.f, "catalyst hours count down");
        LvTick(&c, 2.f, nullptr);
        Expect(c.catalystHours == 0.f, "catalyst hours expire");
        Expect(LvEligible(&c, why, 96) == 0, "expired catalyst leaves the human blocked");
    }

    {
        CharSnap c = Hive();
        c.limbs[LIMB_LEFT_LEG] = LIMB_KIND_STUMP;
        Expect(LvFirstStump(&c) == LIMB_RIGHT_LEG, "first stump is the right leg");
        TickResult r;
        LvTick(&c, 0.1f, &r);
        Expect(c.progress[LIMB_RIGHT_LEG] > 0.f, "growth goes to the first stump");
        Expect(c.progress[LIMB_LEFT_LEG] == 0.f, "the other stump stays 0");
        Expect(c.limbs[LIMB_LEFT_LEG] == LIMB_KIND_STUMP, "the other stump stays a stump");
    }

    {
        CharSnap c = Hive();
        c.fed = 0;
        c.progress[LIMB_RIGHT_LEG] = 40.f;
        const float dt = 1.f;
        // LvTick fills hemolymph, then spends hive_growth_drain * hours.
        // Hive passive beats drain, so a zero tank still grows. Leave the
        // tank under the post-fill spend so the spoken line is "spent".
        c.vigor = LvCfg().hiveGrowthDrain * dt - LvCfg().hivePassive * dt - 1.f;
        char why[96];
        Expect(LvEligible(&c, why, 96) == 1, "hive with too-low vigor is still eligible");
        const float before = c.progress[LIMB_RIGHT_LEG];
        TickResult r;
        LvTick(&c, dt, &r);
        Expect(c.progress[LIMB_RIGHT_LEG] == before, "too-low vigor keeps progress");
        Expect(r.restored == -1, "too-low vigor does not restore");
        Expect(std::strcmp(r.speech, "Hemolymph is spent.") == 0, "speech about spent");
    }

    if (g_fail)
    {
        std::fprintf(stderr, "%d failed\n", g_fail);
        return 1;
    }
    std::printf("all sim tests passed\n");
    return 0;
}
