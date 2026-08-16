#pragma once

#include "lv_types.h"

// No TitleScreen hook. No MyGUI widgets.
// I-key tooltip is the HUD (lv_hooks g_hudHave / hook_tip1).
void LvHudInstall();                  // no-op log: I-key only
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap); // no-op if there is no window
