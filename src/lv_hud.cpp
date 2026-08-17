#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.15 dumps Datapanel keys + one MyGUI
// tree. No paint. No MyGUI create. No layout file.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is after-orig dump — no paint, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
