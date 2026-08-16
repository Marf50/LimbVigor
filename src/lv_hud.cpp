#include "lv_hud.h"
#include "lv_config.h"

// v1.9.5: any MyGUI widget create is a hard kill on Kenshi 1.0.65 +
// Dark UI — title screen AND after In-game. Do not create
// Kenshi_WindowCX / Button1 / GenericTextBox / FloatingPanelSkin /
// ProgressBar / anything else. Visible path is setLineProgress on
// medicalPanel (how Blood itself appears) plus I-key backup.

void LvHudInstall()
{
    LvLog("LimbVigor: no MyGUI widgets — medicalPanel setLineProgress + I-key");
}

void LvHudEnsureAfterInGame()
{
    // Intentionally empty. Do not create widgets here.
}

void LvHudHide() {}

void LvHudNote(const CharSnap* snap)
{
    // Snapshot lives in lv_hooks (g_hudHave) for the I-key tooltip.
    (void)snap;
}

void LvHudPaint(const CharSnap* snap)
{
    LvHudNote(snap);
}
