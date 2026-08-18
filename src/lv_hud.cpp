#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.24 Widget::setCaption every tick
// on LifeBar10 (tucked after Hunger). setVisible LifeBar10 / Value only.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is Widget::setCaption every tick on LifeBar10 — setVisible LifeBar10/Value only");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
