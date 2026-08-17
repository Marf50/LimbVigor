#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.20 paints Hemolymph on
// LifeBar1Datapanel only if lookup is SEH-safe. No _getWidget
// retry after probe SEH. No tree walk. No createWidget. No ctor.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is SEH-safe LifeBar1Datapanel lookup — no tree walk, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
