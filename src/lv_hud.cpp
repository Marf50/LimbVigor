#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.29: LifeBar10 in MedicalPanel + stump growth.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is LifeBar10 in MedicalPanel + pixel Green — setCapW=0 expected");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
