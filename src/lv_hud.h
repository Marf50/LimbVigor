#pragma once

#include "lv_types.h"

// Intentionally empty. Selected-character overlay bars crashed Kenshi
// (missing ProgressBar skin + Dark UI + MyGUI ABI). GUI is STATS + I-key.
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
