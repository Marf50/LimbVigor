#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.23 Widget::setCaption every tick
// on LifeBar10 (after Hunger). Never LifeBar1. No _getWidget. No setVisible.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is Widget::setCaption every tick on LifeBar10 — no LifeBar1 overwrite");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
