#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// Layout: MedicalPanel y slid 0.71852→0.68852 only. After orig:
// setVisible LifeBar10 / Datapanel / Value / Green. ISubWidgetText
// setCaption on Datapanel / getSubWidgetText(LifeBar10). Not Widget::setCaption.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
