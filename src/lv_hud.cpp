#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.16 dumps Datapanel keys, MyGUI
// exports, and Goal/State neighbors. No paint. No layout load.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is after-orig dump — no paint, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
