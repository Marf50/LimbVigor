#pragma once

#include "lv_types.h"

// HUD module. Resolve, paint, and lifetime live here — not in lv_game.cpp.
//
// OptionsTab belt: while findWidgetT("OptionsTab") succeeds, no Hemolymph*
// find / getName / getVisible / setCaption / setSize / setVisible / setDepth.
// Drop cached pointers without calling into them. medicalUpdate must not
// LvHudTickBegin while the belt is on (that reset is why invalidate died).
// Orig getMedicalGUIData / _NV_getGUIData still run first.
//
// H1: findWidgetT HemolymphBar* THIS tick only, and only when the belt is off.
// Never getName/getVisible/set* on yesterday's pointer.
//
// Do not ship a widget named LifeBar10*. Do not add LifeBar11. Do not
// loadLayout / createWidget. Hemolymph nodes stay in the override file.

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

void LvHudTickBegin(); // new tick — no-op while OptionsTab belt is on
int  LvHudBeltPoll(); // find OptionsTab (proven); 1 = belted, no Hemolymph MyGUI
int  LvHudBeltOn();   // flag only — no MyGUI
void LvWalkSelPanel(DatapanelGUI* panel); // hunt MainBar widgets (no tree walk)
void LvNoteHudProbeSeh(); // find/write SEH — do not probe a stale pointer
void LvHudCacheDrop(const char* why); // no-op — no epoch-drop
int  LvHudCacheAlive(); // 0 after skip this tick or no host
int  LvHudWritesOk(); // 0 after skip this tick
void LvHudResumeWrites(); // no-op
void LvHudWatchGui(); // no-op — no epoch-drop
void LvClearHud(DatapanelGUI* panel); // hide/clear HemolymphBar* only — never LifeBar1
void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap);
