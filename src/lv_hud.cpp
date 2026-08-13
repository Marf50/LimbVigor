#include "lv_hud.h"

// v1.8: do not create MyGUI widgets.
//
// v1.7.1 walked the widget tree, spawned ProgressBar + tooltip delegates
// on the selected-character HUD, and crashed on save load:
//   - Kenshi has no skin named "ProgressBar" (MyGUI replaced it, then died)
//   - Dark UI remaps MainPanel / Blood, so the clone path never found a host
//   - MyGUIEngine is VS2010; UString / newDelegate from a VS2022 plugin is ABI poison
//
// Player-facing info lives on the STATS list (C) via DatapanelGUI::setLineProgress
// — the same call the game uses for Blood — and on the I-key growth-part tooltip.

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}
