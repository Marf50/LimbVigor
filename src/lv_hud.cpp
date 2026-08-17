#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.18 paints Hemolymph on
// LifeBar1Datapanel via findWidget + setLineProgress. No tree walk.
// No layout load. No createWidget.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is findWidget LifeBar1Datapanel — no tree walk, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
