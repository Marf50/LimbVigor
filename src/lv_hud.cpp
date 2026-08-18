#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.22 prefixed findWidget LifeBar1
// + setCaption. No _getWidget. No setVisible. No Datapanel.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is prefixed findWidget LifeBar1 + setCaption — no _getWidget");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
