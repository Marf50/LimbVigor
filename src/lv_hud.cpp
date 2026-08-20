#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.30: PanelEmpty Datapanel + host Green.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is LifeBar10 in MedicalPanel + pixel Green host — setCapW=0 expected");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
