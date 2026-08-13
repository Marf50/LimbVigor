#pragma once

#include "lv_types.h"

// Pure simulation. Same numbers as the field-manual bench.
// No Kenshi types — unit-testable and safe.

const char* LvResourceName(RaceKind k);
const char* LvLimbLabel(LimbId id);
const char* LvStageName(float progress01to100);

int  LvAnyStump(const CharSnap* c);
int  LvFirstStump(const CharSnap* c); // LimbId or -1
int  LvEligible(const CharSnap* c, char* why, int whySize);

// Player-facing copy for the STATS panel / speech.
const char* LvRaceHint(RaceKind k);
float       LvHoursToFinish(const CharSnap* c); // <0 if paused / no stump
void        LvEtaText(const CharSnap* c, char* out, int outsz);

// HUD / I-key tooltip lines. bar2 empty if no stump. tip is always filled.
void LvHudLines(const CharSnap* c,
    char* bar1, int n1, float* fill1,
    char* bar2, int n2, float* fill2,
    char* tip, int nt);

// One paragraph for I-key hover (resource + stage + time + race rule).
void LvItemTooltipText(const CharSnap* c, char* out, int outsz);

void LvClearResult(TickResult* out); // restored / stageChanged = -1
void LvTick(CharSnap* c, float dtHours, TickResult* out);
void LvApplyCatalyst(CharSnap* c, TickResult* out);
