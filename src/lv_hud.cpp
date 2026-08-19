#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.28: 1–9 reverted, Hemolymph on new strip.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is revert 1-9 + Hemolymph strip + pixel Green — setCapW=0 expected");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
