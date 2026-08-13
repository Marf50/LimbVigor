#pragma once

#include "lv_types.h"

// Selected-character HUD: own bar under Blood + always-visible caption.
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap); // snapshot + attach UI-thread paint
