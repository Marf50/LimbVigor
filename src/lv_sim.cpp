#include "lv_sim.h"
#include "lv_config.h"

#include <cstdio>
#include <cstring>
#include <cmath>

static float Clamp(float n, float a, float b)
{
    if (n < a) return a;
    if (n > b) return b;
    return n;
}

const char* LvResourceName(RaceKind k)
{
    switch (k)
    {
    case RACE_HIVE: return "Hemolymph";
    case RACE_SHEK: return "Battle-heat";
    case RACE_HUMAN: return "Vigor";
    default: return "";
    }
}

const char* LvLimbLabel(LimbId id)
{
    switch (id)
    {
    case LIMB_RIGHT_LEG: return "right leg";
    case LIMB_LEFT_LEG:  return "left leg";
    case LIMB_RIGHT_ARM: return "right arm";
    case LIMB_LEFT_ARM:  return "left arm";
    default: return "limb";
    }
}

const char* LvStageName(float p)
{
    if (p < 25.f) return "dormant stump";
    if (p < 50.f) return "budding";
    if (p < 75.f) return "forming";
    if (p < 100.f) return "knitting";
    return "whole";
}

int LvAnyStump(const CharSnap* c)
{
    if (!c) return 0;
    for (int i = 0; i < LIMB_COUNT; ++i)
        if (c->limbs[i] == LIMB_KIND_STUMP || c->limbs[i] == LIMB_KIND_CRUSHED)
            return 1;
    return 0;
}

int LvFirstStump(const CharSnap* c)
{
    if (!c) return -1;
    for (int i = 0; i < LIMB_COUNT; ++i)
        if (c->limbs[i] == LIMB_KIND_STUMP || c->limbs[i] == LIMB_KIND_CRUSHED)
            return i;
    return -1;
}

int LvEligible(const CharSnap* c, char* why, int whySize)
{
    if (why && whySize > 0) why[0] = 0;
    if (!c) return 0;

    auto setWhy = [&](const char* s) {
        if (why && whySize > 0 && s)
            std::snprintf(why, (size_t)whySize, "%s", s);
    };

    if (c->race == RACE_SKELETON)
    {
        setWhy("Frames do not grow flesh.");
        return 0;
    }
    if (c->race == RACE_ANIMAL)
    {
        setWhy("This body cannot regrow a limb.");
        return 0;
    }
    if (!LvAnyStump(c))
    {
        setWhy("No stump to grow from. Remove a prosthetic first.");
        return 0;
    }
    if (c->bleedRate > LvCfg().bleedPause)
    {
        setWhy("Too much bleeding. Bandage first.");
        return 0;
    }
    if (c->starving)
    {
        setWhy("Starving. Nothing left to grow with.");
        return 0;
    }
    if (c->race == RACE_HIVE) return 1;

    const int hasCat = c->catalystHours > 0.f ? 1 : 0;
    if (c->race == RACE_SHEK)
    {
        if (c->toughness >= LvCfg().shekToughness || hasCat) return 1;
        char buf[96];
        std::snprintf(buf, sizeof(buf),
            "Shek need toughness %.0f, or a splint on the stump.",
            LvCfg().shekToughness);
        setWhy(buf);
        return 0;
    }

    if (c->toughness >= LvCfg().humanToughness && c->medic >= LvCfg().humanMedic)
        return 1;
    if (hasCat) return 1;

    char buf[112];
    std::snprintf(buf, sizeof(buf),
        "Need a Splint Kit, or toughness %.0f and medic %.0f.",
        LvCfg().humanToughness, LvCfg().humanMedic);
    setWhy(buf);
    return 0;
}

const char* LvRaceHint(RaceKind k)
{
    switch (k)
    {
    case RACE_HIVE:
        return "Hive: hemolymph knits on its own. Stay fed. Bandage. Sleep.";
    case RACE_SHEK:
        return "Shek: toughness 20, or use a Splint Kit on the stump.";
    case RACE_HUMAN:
        return "Human: toughness 40 and medic 25, or a used Splint Kit.";
    case RACE_SKELETON:
        return "Skeleton: frames do not grow flesh. Buy a replacement.";
    default:
        return "This body cannot regrow a limb.";
    }
}

static float GrowthRatePerHour(const CharSnap* c)
{
    if (!c) return 0.f;
    const LvConfig& cfg = LvCfg();
    float rate = cfg.humanGrowth;
    if (c->race == RACE_HIVE) rate = cfg.hiveGrowth;
    else if (c->race == RACE_SHEK) rate = cfg.shekGrowth;
    if (c->inBed) rate *= cfg.bedGrowthMult;
    return rate;
}

float LvHoursToFinish(const CharSnap* c)
{
    char why[8];
    if (!c || !LvEligible(c, why, (int)sizeof(why))) return -1.f;
    const int t = LvFirstStump(c);
    if (t < 0) return -1.f;
    const float left = 100.f - c->progress[t];
    if (left <= 0.f) return 0.f;
    const float rate = GrowthRatePerHour(c);
    if (rate < 0.01f) return -1.f;
    return left / rate;
}

void LvEtaText(const CharSnap* c, char* out, int outsz)
{
    if (!out || outsz < 8) return;
    out[0] = 0;
    if (!c) return;
    char why[96];
    if (!LvEligible(c, why, (int)sizeof(why)))
    {
        if (why[0]) std::snprintf(out, (size_t)outsz, "%s", why);
        else std::snprintf(out, (size_t)outsz, "%s", LvRaceHint(c->race));
        return;
    }
    const float h = LvHoursToFinish(c);
    if (h < 0.f)
    {
        std::snprintf(out, (size_t)outsz, "Paused. Eat, bandage, or rest.");
        return;
    }
    if (h < 1.f)
        std::snprintf(out, (size_t)outsz, "Almost there — under an hour.");
    else if (h < 24.f)
        std::snprintf(out, (size_t)outsz, c->inBed ? "~%.0fh in this bed." : "~%.0fh on the road. Bed is 2x.", h);
    else
        std::snprintf(out, (size_t)outsz, c->inBed ? "~%.1f days in this bed." : "~%.1f days walking. Bed is 2x.", h / 24.f);
}

void LvClearResult(TickResult* out)
{
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    out->restored = -1;
    out->stageChanged = -1;
}

void LvApplyCatalyst(CharSnap* c, TickResult* out)
{
    LvClearResult(out);
    if (!c) return;
    if (c->race == RACE_SKELETON || c->race == RACE_ANIMAL) return;
    c->catalystHours = LvCfg().catalystHours;
    if (out)
    {
        std::snprintf(out->speech, sizeof(out->speech),
            "The splint takes. The stump can grow if you stay fed.");
        out->grew = 1;
    }
}

void LvTick(CharSnap* c, float dtHours, TickResult* out)
{
    LvClearResult(out);
    if (!c || dtHours <= 0.f) return;
    if (dtHours > 2.f) dtHours = 2.f;

    if (c->catalystHours > 0.f)
    {
        c->catalystHours -= dtHours;
        if (c->catalystHours < 0.f) c->catalystHours = 0.f;
    }
    if (c->restoreLock > 0.f)
    {
        c->restoreLock -= dtHours;
        if (c->restoreLock < 0.f) c->restoreLock = 0.f;
    }

    if (c->race == RACE_SKELETON || c->race == RACE_ANIMAL)
    {
        c->vigor = 0.f;
        return;
    }

    const LvConfig& cfg = LvCfg();
    float gain = 0.f;
    float drain = 0.f;
    float rate = 0.f;
    switch (c->race)
    {
    case RACE_HIVE:
        gain = cfg.hivePassive;
        if (c->inCombat) gain += cfg.hiveCombat;
        drain = cfg.hiveGrowthDrain;
        rate = cfg.hiveGrowth;
        break;
    case RACE_SHEK:
        gain = cfg.shekPassive;
        if (c->inCombat) gain += cfg.shekCombat;
        drain = cfg.shekGrowthDrain;
        rate = cfg.shekGrowth;
        break;
    default:
        gain = cfg.humanPassive;
        if (c->inCombat) gain += cfg.humanCombat;
        drain = cfg.humanGrowthDrain;
        rate = cfg.humanGrowth;
        break;
    }
    if (c->fed) gain += cfg.fed;
    if (c->inBed) gain += cfg.bed;
    if (c->starving) gain -= cfg.starveDrain;

    c->vigor = Clamp(c->vigor + gain * dtHours, 0.f, cfg.maxVigor);

    char why[96];
    if (!LvEligible(c, why, (int)sizeof(why)))
    {
        if (LvAnyStump(c) && why[0] && std::strcmp(why, c->lastBlock) != 0)
        {
            std::snprintf(c->lastBlock, sizeof(c->lastBlock), "%s", why);
            if (out)
            {
                out->blockedNew = 1;
                std::snprintf(out->speech, sizeof(out->speech), "%s", why);
            }
        }
        return;
    }
    c->lastBlock[0] = 0;

    const int target = LvFirstStump(c);
    if (target < 0) return;

    const float spend = drain * dtHours;
    if (c->vigor < spend)
    {
        if (out && c->lastBlock[0] == 0)
        {
            std::snprintf(out->speech, sizeof(out->speech),
                "%s is spent. Eat, rest, or fight.", LvResourceName(c->race));
            std::snprintf(c->lastBlock, sizeof(c->lastBlock), "spent");
        }
        return;
    }

    if (c->inBed) rate *= cfg.bedGrowthMult;

    c->vigor -= spend;
    c->progress[target] = Clamp(c->progress[target] + rate * dtHours, 0.f, 100.f);
    if (out) out->grew = 1;

    const int stage = (int)(c->progress[target] / 25.f);
    if (stage != c->lastStage[target] && c->progress[target] < 100.f)
    {
        c->lastStage[target] = stage;
        if (out)
        {
            out->stageChanged = target;
            out->stageValue = stage;
            std::snprintf(out->speech, sizeof(out->speech),
                "The %s is %s (%.0f%%).",
                LvLimbLabel((LimbId)target),
                LvStageName(c->progress[target]),
                c->progress[target]);
        }
    }

    if (c->progress[target] >= 100.f && c->restoreLock <= 0.f)
    {
        // Mark whole for the simulator. The game hook keeps 100% until
        // setLimb actually sticks; Bind will not treat that as a new cut.
        c->limbs[target] = LIMB_KIND_WHOLE;
        c->progress[target] = 100.f;
        if (out)
        {
            out->restored = target;
            std::snprintf(out->speech, sizeof(out->speech),
                "The %s is whole again. Weak. Mine.",
                LvLimbLabel((LimbId)target));
        }
    }
}
