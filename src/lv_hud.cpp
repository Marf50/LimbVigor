#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.21 setCaption on LifeBar1 /
// Blood-caption if the hunt finds it. No Datapanel. No setSize.
// No _getWidget retry after SEH. No tree walk. No createWidget.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is setCaption on LifeBar1 if found — no Datapanel, no setSize, no tree walk");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
