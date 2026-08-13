#pragma once

#include "lv_types.h"

// Own HUD bar + tooltip, painted on the UI thread only.
// Lives next to Blood on the selected-character HUD (not the C sheet).
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
