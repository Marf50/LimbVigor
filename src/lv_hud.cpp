#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.26 ISubWidgetText::setCaption after orig
// every tick. setVisible LifeBar10 / Datapanel / Value / Green only.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is ISub setCaption after orig on LifeBar10 — Widget::setCaption not exported");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
