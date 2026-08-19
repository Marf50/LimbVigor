#include "lv_hud.h"

#include "lv_config.h"

// I-key tooltip snapshot only. v1.27 ISub on Datapanel, numeric Value,
// Green setSize after orig. setVisible LifeBar10 / Datapanel / Value / Green only.

void LvHudInstall()
{
    LvLog("LimbVigor: HUD is ISub on Datapanel + numeric Value + Green fill — setCapW=0 expected");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
