#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.17 dumps Datapanel keys, Gui's
// Singleton widget tree, and Kenshi_MainPanel.layout names from disk.
// No paint. No layout load.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is after-orig dump — no paint, no load-time GUI hooks");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
