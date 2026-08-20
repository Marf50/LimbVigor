#pragma once

#include "lv_types.h"

// HUD module (v1.39). Resolve, paint, and lifetime/invalidate live here —
// not in lv_game.cpp. MyGUI export bind/dump is in lv_hud.cpp.
//
// Lifetime: a cached LifeBar10* is dead if getName SEH, getVisible SEH, or
// the name no longer ends with LifeBar10. Then null every MyGUI pointer,
// drop the prefix, skip writes this tick, re-resolve next tick.
// OptionsPanel / odSettingsOpen / Kenshi_Options name-guess is deleted.

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

void LvHudTickBegin(); // new medical/GUI tick — clear skip so a later death can log
void LvWalkSelPanel(DatapanelGUI* panel); // hunt MainBar widgets (no tree walk)
void LvNoteHudProbeSeh(); // probe SEH — invalidate is dead LifeBar10* only
void LvHudCacheDrop(const char* why); // no-op v1.33 — never drop on ESC/pause/pointer change
int  LvHudCacheAlive(); // 0 after invalidate this tick or no host
int  LvHudWritesOk(); // 0 after invalidate this tick
void LvHudResumeWrites(); // no-op v1.33
void LvHudWatchGui(); // no-op v1.33 — no epoch-drop
void LvClearHud(DatapanelGUI* panel); // hide/clear LifeBar10* only — never touch LifeBar1
void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap);
