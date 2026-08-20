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
    if (p < 25.f) return "dormant";
    if (p < 50.f) return "budding";
    if (p < 75.f) return "forming";
    if (p < 100.f) return "knitting";
    return "grown";
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

LimbKind LvClassifyFromHp(float hp, float mx, int haveHp, char* why, int whyN)
{
    if (why && whyN > 0) why[0] = 0;
    if (!haveHp)
    {
        if (why && whyN > 0)
            std::snprintf(why, (size_t)whyN, "%s", "whole (no hp, state ORIGINAL)");
        return LIMB_KIND_WHOLE;
    }
    /* Kenshi: flesh<=0 is crippled; flesh<=-max is cut off. UI may still
     * show a small positive (Left Leg 5) on a cut-off nub. */
    if (hp <= 0.f)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "crippled flesh<=0");
        return LIMB_KIND_STUMP;
    }
    if (mx > 1.f && hp <= -mx + 0.01f)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "cut-off flesh<=-max");
        return LIMB_KIND_STUMP;
    }
    /* Left Leg 5 is a stump even if a restore write collapsed max to 5. */
    if (hp > 0.f && hp < 10.f)
    {
        if (why && whyN > 0) std::snprintf(why, (size_t)whyN, "%s", "cut-off nub hp<10");
        return LIMB_KIND_STUMP;
    }
    if (why && whyN > 0)
    {
        if (hp >= 50.f)
            std::snprintf(why, (size_t)whyN, "%s", "intact");
        else
            std::snprintf(why, (size_t)whyN, "%s", "injured — not a stump");
    }
    return LIMB_KIND_WHOLE;
}

int LvNextStump(const CharSnap* c, int after)
{
    if (!c) return -1;
    int start = after + 1;
    if (start < 0) start = 0;
    for (int i = start; i < LIMB_COUNT; ++i)
    {
        if (c->limbs[i] != LIMB_KIND_STUMP && c->limbs[i] != LIMB_KIND_CRUSHED)
            continue;
        if (c->progress[i] >= 100.f) continue;
        return i;
    }
    return -1;
}

static int SocketIsStump(const CharSnap* c, int i)
{
    if (!c || i < 0 || i >= LIMB_COUNT) return 0;
    return (c->limbs[i] == LIMB_KIND_STUMP || c->limbs[i] == LIMB_KIND_CRUSHED) ? 1 : 0;
}

static int SlotOpenForHud(const CharSnap* c, int i)
{
    if (!c || i < 0 || i >= LIMB_COUNT) return 0;
    /* Grown drops Line 2 only when the socket is no longer a stump.
     * Persist 100% while the game still shows -33 must keep Line 2. */
    if (SocketIsStump(c, i))
        return 1;
    if (c->progress[i] >= 100.f) return 0;
    if (c->lastStage[i] == 4) return 0; // LV_PART_GROWN
    return 1;
}

void LvHeartbeatLine(const CharSnap* c, char* out, int n)
{
    if (!out || n < 8) return;
    out[0] = 0;
    if (!c) return;

    const float maxv = LvCfg().maxVigor > 0.f ? LvCfg().maxVigor : 100.f;
    const char* res = LvResourceName(c->race);
    if (c->race == RACE_SKELETON)
        res = "Limb Vigor";
    if (!res || !res[0])
        res = "Vigor";

    const int stump = LvFirstStump(c);
    char why[96];
    const int ok = LvEligible(c, why, (int)sizeof(why));
    if (stump < 0)
    {
        std::snprintf(out, (size_t)n,
            "%s  %s %.0f/%.0f  no stump (Rleg %.0f/%.0f %s, Lleg %.0f/%.0f %s, Rarm %.0f/%.0f %s, Larm %.0f/%.0f %s)",
            c->name, res, c->vigor, maxv,
            c->limbHp[LIMB_RIGHT_LEG], c->limbMax[LIMB_RIGHT_LEG],
            (c->limbs[LIMB_RIGHT_LEG] == LIMB_KIND_STUMP || c->limbs[LIMB_RIGHT_LEG] == LIMB_KIND_CRUSHED) ? "stump" : "whole",
            c->limbHp[LIMB_LEFT_LEG], c->limbMax[LIMB_LEFT_LEG],
            (c->limbs[LIMB_LEFT_LEG] == LIMB_KIND_STUMP || c->limbs[LIMB_LEFT_LEG] == LIMB_KIND_CRUSHED) ? "stump" : "whole",
            c->limbHp[LIMB_RIGHT_ARM], c->limbMax[LIMB_RIGHT_ARM],
            (c->limbs[LIMB_RIGHT_ARM] == LIMB_KIND_STUMP || c->limbs[LIMB_RIGHT_ARM] == LIMB_KIND_CRUSHED) ? "stump" : "whole",
            c->limbHp[LIMB_LEFT_ARM], c->limbMax[LIMB_LEFT_ARM],
            (c->limbs[LIMB_LEFT_ARM] == LIMB_KIND_STUMP || c->limbs[LIMB_LEFT_ARM] == LIMB_KIND_CRUSHED) ? "stump" : "whole");
        return;
    }

    const int persistGrown = (c->progress[stump] >= 99.5f || c->lastStage[stump] == 4) ? 1 : 0;
    if (persistGrown)
    {
        /* Socket is still a stump. Do not claim 100% grown / BLOCKED. */
        if (!ok && why[0])
        {
            std::snprintf(out, (size_t)n, "%s  %s %.0f/%.0f  %s",
                c->name, res, c->vigor, maxv, why);
            return;
        }
        std::snprintf(out, (size_t)n,
            "%s  %s %.0f/%.0f  %s still a stump (numbers kept, no restore write)",
            c->name, res, c->vigor, maxv, LvLimbLabel((LimbId)stump));
        return;
    }

    std::snprintf(out, (size_t)n, "%s  %s %.0f/%.0f  %s %.0f%% %s%s",
        c->name, res, c->vigor, maxv,
        LvLimbLabel((LimbId)stump),
        c->progress[stump],
        LvStageName(c->progress[stump]),
        ok ? "" : "  BLOCKED");
}

int LvHudLimbSlot(const CharSnap* c)
{
    if (!c) return -1;
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        if ((c->limbs[i] == LIMB_KIND_STUMP || c->limbs[i] == LIMB_KIND_CRUSHED)
            && SlotOpenForHud(c, i))
            return i;
    }
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        if (c->limbs[i] == LIMB_KIND_PROSTHETIC && c->progress[i] > 0.f
            && SlotOpenForHud(c, i))
            return i;
    }
    return -1;
}

const char* LvHudResourceKey(const CharSnap* c)
{
    if (!c || c->race == RACE_ANIMAL) return "";
    if (c->race == RACE_SKELETON) return "Limb Vigor";
    const char* res = LvResourceName(c->race);
    return (res && res[0]) ? res : "Vigor";
}

static int HasMetalProgress(const CharSnap* c)
{
    if (!c) return 0;
    for (int i = 0; i < LIMB_COUNT; ++i)
    {
        if (c->limbs[i] == LIMB_KIND_PROSTHETIC && c->progress[i] > 0.f
            && SlotOpenForHud(c, i))
            return 1;
    }
    return 0;
}

static float HourDrain(const CharSnap* c)
{
    if (!c) return 0.f;
    const LvConfig& cfg = LvCfg();
    if (c->race == RACE_HIVE) return cfg.hiveGrowthDrain;
    if (c->race == RACE_SHEK) return cfg.shekGrowthDrain;
    return cfg.humanGrowthDrain;
}

static int ResourceSpent(const CharSnap* c)
{
    if (!c) return 0;
    const float drain = HourDrain(c);
    return (c->vigor < drain) ? 1 : 0;
}

static void SpentLine(const CharSnap* c, char* out, int n)
{
    if (!out || n < 8) return;
    const char* res = LvResourceName(c ? c->race : RACE_HUMAN);
    if (!res || !res[0]) res = "Vigor";
    std::snprintf(out, (size_t)n, "%s is spent. Eat or rest.", res);
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
        if (HasMetalProgress(c))
            setWhy("Metal. Progress kept.");
        else
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
        setWhy("Need toughness 20, or a splint.");
        return 0;
    }

    if (c->toughness >= LvCfg().humanToughness && c->medic >= LvCfg().humanMedic)
        return 1;
    if (hasCat) return 1;

    setWhy("Need a Splint Kit, or toughness 40 and medic 25.");
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
        return;
    }
    if (ResourceSpent(c))
    {
        SpentLine(c, out, outsz);
        return;
    }
    if (c->race == RACE_SHEK && c->inCombat)
    {
        std::snprintf(out, (size_t)outsz, c->inBed ? "heat bed" : "heat");
        return;
    }
    const float h = LvHoursToFinish(c);
    if (h < 0.f)
        return;
    int hours = (int)(h + 0.5f);
    if (h > 0.f && hours < 1) hours = 1;
    if (c->inBed)
        std::snprintf(out, (size_t)outsz, "~%dh bed", hours);
    else
        std::snprintf(out, (size_t)outsz, "~%dh", hours);
}

void LvHudLines(const CharSnap* c,
    char* bar1, int n1, float* fill1,
    char* bar2, int n2, float* fill2,
    char* tip, int nt)
{
    if (fill1) *fill1 = 0.f;
    if (fill2) *fill2 = 0.f;
    if (bar1 && n1 > 0) bar1[0] = 0;
    if (bar2 && n2 > 0) bar2[0] = 0;
    if (tip && nt > 0) tip[0] = 0;
    if (!c || c->race == RACE_ANIMAL)
        return;

    if (c->race == RACE_SKELETON)
    {
        if (bar1 && n1 > 0)
            std::snprintf(bar1, (size_t)n1, "Frames do not grow flesh.");
        return;
    }

    const float maxv = LvCfg().maxVigor > 0.f ? LvCfg().maxVigor : 100.f;
    if (fill1) *fill1 = c->vigor / maxv;
    if (bar1 && n1 > 0)
        std::snprintf(bar1, (size_t)n1, "%d / %d", (int)c->vigor, (int)maxv);

    const int slot = LvHudLimbSlot(c);
    if (slot < 0)
        return;
    if (tip && nt > 0)
        std::snprintf(tip, (size_t)nt, "%s", LvLimbLabel((LimbId)slot));

    if (c->limbs[slot] == LIMB_KIND_PROSTHETIC)
    {
        if (bar2 && n2 > 0)
            std::snprintf(bar2, (size_t)n2, "Metal. Progress kept.");
        return;
    }

    char why[96];
    const int ok = LvEligible(c, why, (int)sizeof(why));
    if (!ok)
    {
        if (bar2 && n2 > 0)
            std::snprintf(bar2, (size_t)n2, "%s", why[0] ? why : "Paused.");
        return;
    }
    if (ResourceSpent(c))
    {
        if (bar2 && n2 > 0)
            SpentLine(c, bar2, n2);
        return;
    }

    /* Persist 100% on a live stump is not Grown. Do not paint grown 100%. */
    float showP = c->progress[slot];
    if (showP >= 100.f && SocketIsStump(c, slot))
        showP = 99.f;

    if (fill2) *fill2 = showP / 100.f;
    int pct = (int)showP;
    if (pct < 0) pct = 0;
    if (pct > 99) pct = 99;

    char tag[96];
    LvEtaText(c, tag, (int)sizeof(tag));

    char extra[96];
    extra[0] = 0;
    if (c->catalystHours > 0.f)
    {
        int sh = (int)(c->catalystHours + 0.5f);
        if (c->catalystHours > 0.f && sh < 1) sh = 1;
        std::snprintf(extra, sizeof(extra), " splint %dh", sh);
    }
    const int nxt = LvNextStump(c, slot);
    if (nxt >= 0)
    {
        char more[48];
        std::snprintf(more, sizeof(more), " then %s", LvLimbLabel((LimbId)nxt));
        std::strncat(extra, more, sizeof(extra) - std::strlen(extra) - 1);
    }

    if (bar2 && n2 > 0)
        std::snprintf(bar2, (size_t)n2, "%s %d%%  %s%s",
            LvStageName(showP), pct, tag, extra);
}

void LvItemTooltipText(const CharSnap* c, char* out, int outsz)
{
    if (!out || outsz < 8) return;
    out[0] = 0;
    if (!c) return;

    char bar1[96], bar2[96], tip[192];
    float a = 0.f, b = 0.f;
    LvHudLines(c, bar1, (int)sizeof(bar1), &a, bar2, (int)sizeof(bar2), &b, tip, (int)sizeof(tip));
    if (bar2[0])
        std::snprintf(out, (size_t)outsz, "%s. %s. %s", bar1, bar2, tip);
    else
        std::snprintf(out, (size_t)outsz, "%s. %s", bar1, tip);
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
        std::snprintf(out->speech, sizeof(out->speech), "The splint takes.");
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
    const int target = LvFirstStump(c);
    if (target < 0) return;

    const float spend = drain * dtHours;
    if (c->vigor < spend)
    {
        char spent[96];
        SpentLine(c, spent, (int)sizeof(spent));
        if (std::strcmp(c->lastBlock, spent) != 0)
        {
            std::snprintf(c->lastBlock, sizeof(c->lastBlock), "%s", spent);
            if (out)
            {
                const char* res = LvResourceName(c->race);
                if (!res || !res[0]) res = "Vigor";
                std::snprintf(out->speech, sizeof(out->speech), "%s is spent.", res);
            }
        }
        return;
    }
    c->lastBlock[0] = 0;

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
            if (stage >= 1 && stage <= 3)
            {
                std::snprintf(out->speech, sizeof(out->speech),
                    "The %s is %s.",
                    LvLimbLabel((LimbId)target),
                    LvStageName(c->progress[target]));
            }
        }
    }

    if (c->progress[target] >= 100.f && c->restoreLock <= 0.f)
    {
        // Simulator marks the socket whole. The game hook slots the
        // Grown part — that is the limb. Progress stays 100 until then.
        c->limbs[target] = LIMB_KIND_WHOLE;
        c->progress[target] = 100.f;
        if (out)
        {
            out->restored = target;
            if (c->lastStage[target] != 4)
            {
                c->lastStage[target] = 4;
                std::snprintf(out->speech, sizeof(out->speech),
                    "The %s has grown back.",
                    LvLimbLabel((LimbId)target));
            }
        }
    }

    if (out && !out->speech[0] && c->race == RACE_SHEK && c->inCombat && out->grew)
    {
        static char heatName[48][48];
        static int heatN = 0;
        int said = 0;
        for (int i = 0; i < heatN; ++i)
        {
            if (c->name[0] && std::strcmp(heatName[i], c->name) == 0)
            {
                said = 1;
                break;
            }
        }
        if (!said && heatN < 48 && c->name[0])
        {
            std::snprintf(heatName[heatN], sizeof(heatName[heatN]), "%s", c->name);
            heatN++;
            std::snprintf(out->speech, sizeof(out->speech), "The heat is in it now.");
        }
    }
}
