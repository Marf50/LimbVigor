#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// Layout: MedicalPanel grown + 1–10 / Value / Tooltip rescaled.
// After orig: ISub on Datapanel, numeric Value, setSize LifeBar10Green.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
