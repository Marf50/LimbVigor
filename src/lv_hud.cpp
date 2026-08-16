#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. Visible HUD is setLineProgress on the
// selection DatapanelGUI (lv_game). No MyGUI walk. No layout file.
// No createWidget.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is selection-panel setLineProgress — no layout, no createWidget");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
