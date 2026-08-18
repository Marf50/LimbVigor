#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.22 setCaption only if getName
// contains LifeBar1. No parent write. No setVisible. No Datapanel.
// No _getWidget retry after SEH. No tree walk. No createWidget.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is name-gated setCaption LifeBar1 only — no parent write, no setVisible");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
