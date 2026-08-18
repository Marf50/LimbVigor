#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.23 Widget::setCaption every tick
// on LifeBar1. No _getWidget. No setVisible. No Datapanel.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is Widget::setCaption every tick on LifeBar1 — no _getWidget");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
