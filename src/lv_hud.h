#pragma once

#include "lv_types.h"

// HUD module. Resolve, paint, and lifetime live here — not in lv_game.cpp.
//
// H1: findWidgetT HemolymphBar* THIS tick only. Never getName/getVisible/set*
// on yesterday's pointer. Forget the cache without touching it, then find.
//
// H2: do not ship a widget named LifeBar10*. Kenshi assignWidget-s that slot;
// Dark UI leaves it null and ESC lives. HemolymphBar is our row.

#if defined(LIMBVIGOR_IDE)
struct MedicalSystem;
struct Character;
struct DatapanelGUI;
#else
class MedicalSystem;
class Character;
class DatapanelGUI;
#endif

void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);

void LvHudTickBegin(); // new tick — allow writes after a prior skip
void LvWalkSelPanel(DatapanelGUI* panel); // hunt MainBar widgets (no tree walk)
void LvNoteHudProbeSeh(); // find/write SEH — do not probe a stale pointer
void LvHudCacheDrop(const char* why); // no-op — no epoch-drop
int  LvHudCacheAlive(); // 0 after skip this tick or no host
int  LvHudWritesOk(); // 0 after skip this tick
void LvHudResumeWrites(); // no-op
void LvHudWatchGui(); // no-op — no epoch-drop
void LvClearHud(DatapanelGUI* panel); // hide/clear HemolymphBar* only — never LifeBar1
void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap);
