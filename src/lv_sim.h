#pragma once

#include "lv_types.h"

// Pure simulation. Same numbers as the field-manual bench.
// No Kenshi types — unit-testable and safe.

const char* LvResourceName(RaceKind k);
const char* LvLimbLabel(LimbId id);
const char* LvStageName(float progress01to100);

int  LvAnyStump(const CharSnap* c);
int  LvFirstStump(const CharSnap* c); // LimbId or -1, legs first
int  LvNextStump(const CharSnap* c, int after); // next waiting stump, or -1
int  LvHudLimbSlot(const CharSnap* c); // Line 2 socket, or -1
void LvHeartbeatLine(const CharSnap* c, char* out, int n); // no "100% grown BLOCKED" on a live stump
int  LvEligible(const CharSnap* c, char* why, int whySize);
const char* LvHudResourceKey(const CharSnap* c); // Hemolymph/Vigor/Battle-heat/Limb Vigor

// Player-facing copy. Growing: short tag (~4h / ~2h bed / heat / heat bed).
// Blocked: Line 2 reason.
const char* LvRaceHint(RaceKind k);
float       LvHoursToFinish(const CharSnap* c); // <0 if paused / no stump
void        LvEtaText(const CharSnap* c, char* out, int outsz);

// bar1 = Line 1 right. bar2 = Line 2 right (empty if omitted).
// tip = Line 2 key (lowercase limb) or empty.
void LvHudLines(const CharSnap* c,
    char* bar1, int n1, float* fill1,
    char* bar2, int n2, float* fill2,
    char* tip, int nt);

// One paragraph for I-key hover (resource + stage + time + race rule).
void LvItemTooltipText(const CharSnap* c, char* out, int outsz);

void LvClearResult(TickResult* out); // restored / stageChanged = -1
void LvTick(CharSnap* c, float dtHours, TickResult* out);
void LvApplyCatalyst(CharSnap* c, TickResult* out);
