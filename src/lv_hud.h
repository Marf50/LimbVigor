#pragma once

#include "lv_types.h"

// No TitleScreen hook. No MyGUI widgets.
// Visible HUD is C (getMedicalGUIData after in-game) + I-key.
void LvHudInstall();                  // no-op log: no title MyGUI
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap); // no-op if there is no window
