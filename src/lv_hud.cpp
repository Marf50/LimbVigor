#include "lv_hud.h"
#include "lv_config.h"

// No title-screen MyGUI. Creating a Kenshi_WindowCX in TitleScreen
// hard-kills 1.0.65 + Dark UI. Live numbers are C (after in-game)
// and I-key. This file stays a no-op — do not create widgets here.

void LvHudInstall()
{
    LvLog("LimbVigor: no title MyGUI — C + I-key after in-game");
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
