#include "lv_sim.h"
#include "lv_config.h"

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

    if (g_fail)
    {
        std::fprintf(stderr, "%d failed\n", g_fail);
        return 1;
    }
    std::printf("all sim tests passed\n");
    return 0;
}
