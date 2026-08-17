#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.14 dumps DatapanelGUI keys; it does
// not guess-paint. No MyGUI create. No layout file.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is after-orig dump — no guess-paint, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
