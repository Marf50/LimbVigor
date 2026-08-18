#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// No layout file. Widget::setCaption every tick on LifeBar1. No _getWidget.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
