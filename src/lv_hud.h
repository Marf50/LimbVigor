#pragma once

#include "lv_types.h"

void LvHudInstall();                  // TitleScreen ctor — UI thread, once
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap); // game thread: snapshot only
