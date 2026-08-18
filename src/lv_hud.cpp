#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.25 Widget::setCaption after orig
// every tick. setVisible LifeBar10 / Datapanel / Value / Green only.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is Widget::setCaption after orig on LifeBar10 — show LifeBar10/Datapanel/Value/Green");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
