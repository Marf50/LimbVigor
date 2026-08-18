#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// Layout: Dark UI exact + LifeBar10 tucked (Back only). Widget::setCaption
// every tick on LifeBar10. setVisible LifeBar10 / LifeBar10Value only.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
