#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// Layout: e062b6d Dark UI exact + LifeBar10 (do not recut). After orig:
// setVisible LifeBar10 / Datapanel / Value / Green. Widget::setCaption
// on LifeBar10 / Datapanel. TextBox or ISubWidgetText on Value only.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
