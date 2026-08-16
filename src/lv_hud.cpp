#include "lv_hud.h"
#include "lv_config.h"

// v1.9.2: no title-screen MyGUI at all.
// Creating a Kenshi_WindowCX in TitleScreen's constructor
// (even with no events) hard-kills 1.0.65 + Dark UI after
// "HUD created at title screen". Live numbers are I-key only.

void LvHudInstall()
{
    LvLog("LimbVigor: no title MyGUI — I-key only");
}

void LvHudHide() {}

void LvHudNote(const CharSnap* snap)
{
    // No window. Snapshot lives in lv_hooks (g_hudHave).
    (void)snap;
}

void LvHudPaint(const CharSnap* snap)
{
    LvHudNote(snap);
}
