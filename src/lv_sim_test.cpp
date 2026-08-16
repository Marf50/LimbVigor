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
        c.race = RACE_HUMAN;
        c.toughness = 1;
        c.medic = 1;
        LvEtaText(&c, eta, 96);
        Expect(std::strstr(eta, "Splint") != nullptr || std::strstr(eta, "toughness") != nullptr,
            "blocked human ETA explains why");
    }

    {
        CharSnap c = Hive();
        c.progress[LIMB_RIGHT_LEG] = 30.f;
        c.vigor = 42.f;
        char bar1[96], bar2[96], tip[220], hover[256];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 220);
        Expect(std::strstr(bar1, "Hemolymph") != nullptr, "HUD bar 1 names Hemolymph");
        Expect(std::strstr(bar1, "42") != nullptr, "HUD bar 1 shows vigor");
        Expect(std::strstr(bar2, "right leg") != nullptr, "HUD bar 2 names the stump");
        Expect(std::strstr(bar2, "budding") != nullptr, "HUD bar 2 shows stage");
        Expect(std::strstr(bar2, "30%") != nullptr, "HUD bar 2 shows percent");
        Expect(f2 > 0.29f && f2 < 0.31f, "HUD bar 2 fill is 30%");
        Expect(!std::strstr(bar2, "Blood") && !std::strstr(bar2, "Hunger"),
            "HUD bar 2 is not a reserved Blood/Hunger key");
        Expect(tip[0] != 0, "HUD tooltip line is never empty");
        {
            char eta[96];
            LvEtaText(&c, eta, 96);
            Expect(eta[0] != 0, "ETA text is filled for an eligible hive");
            Expect(std::strstr(eta, "h") != nullptr || std::strstr(eta, "hour") != nullptr
                || std::strstr(eta, "day") != nullptr,
                "ETA text is a wait time");
        }
        LvItemTooltipText(&c, hover, 256);
        Expect(std::strstr(hover, "Hemolymph") != nullptr, "I-key tooltip has Hemolymph");
        Expect(std::strstr(hover, "budding") != nullptr, "I-key tooltip has stage");
    }

    {
        CharSnap c = Hive();
        c.race = RACE_HUMAN;
        c.toughness = 10;
        c.medic = 5;
        c.progress[LIMB_RIGHT_LEG] = 30.f;
        char bar1[96], bar2[96], tip[220];
        float f1 = 0.f, f2 = 0.f;
        LvHudLines(&c, bar1, 96, &f1, bar2, 96, &f2, tip, 220);
        Expect(std::strstr(bar2, "right leg") != nullptr, "blocked bar 2 still names the stump");
        Expect(std::strstr(bar2, "30%") == nullptr, "blocked bar 2 is the reason, not a percent");
        Expect(bar2[0] != 0, "blocked bar 2 is not empty");
        Expect(f2 == 0.f, "blocked bar 2 fill is empty");
        {
            char eta[96];
            LvEtaText(&c, eta, 96);
            Expect(std::strstr(eta, "Splint") != nullptr || std::strstr(eta, "toughness") != nullptr,
                "blocked ETA is the wait reason");
        }
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
        Expect(std::strstr(eta, "bleeding") != nullptr, "bleed ETA says bandage first");
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

    if (g_fail)
    {
        std::fprintf(stderr, "%d failed\n", g_fail);
        return 1;
    }
    std::printf("all sim tests passed\n");
    return 0;
}
