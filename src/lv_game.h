#pragma once

#include "lv_types.h"

// Thin wrappers around official KenshiLib types.
// Implementation lives in lv_game.cpp and is the only file that includes
// Kenshi headers besides the hook file.

#if defined(LIMBVIGOR_IDE)
struct MedicalSystem;
struct Character;
struct Item;
struct DatapanelGUI;
#else
class MedicalSystem;
class Character;
class Item;
class DatapanelGUI;
#endif

void      LvGameInit();
void      LvNoteMedicalPulse(); // medicalUpdate seen — no Character
int       LvWorldInGame(); // 1 after the world is playable — no Character yet
Character* LvCharFromMed(MedicalSystem* med);
MedicalSystem* LvMedFromChar(Character* me);
void      LvReadSnap(MedicalSystem* med, CharSnap* io);
int       LvRestoreLimb(MedicalSystem* med, int limbId); // unused no-op; do not re-hook
int       LvHasSplint(Character* me);
void      LvSay(Character* me, const char* text);
int       LvIsPlayerSquad(Character* me);
int       LvIsSelectedCharacter(Character* me); // squad body the panel is drawing — not isPlayerCharacter()
int       LvPanelIsLeftMedical(DatapanelGUI* panel); // live key "Blood" only — v1.14 does not guess-paint
int       LvPanelHasBlood(DatapanelGUI* panel); // live key "Blood" on a real line
void      LvWalkSelPanel(DatapanelGUI* panel); // hunt MainBar widgets (no tree walk)
void      LvNoteHudProbeSeh(); // drop LifeBar10* cache — never retry _getWidget
void      LvHudCacheDrop(const char* why); // settings-close / GUI teardown — drop dead widgets
int       LvHudCacheAlive(); // 0 if cached LifeBar10 is gone (MyGUI rebuild)
int       LvGrowStumpNub(MedicalSystem* med, int limbId, float progress, float* hpBefore, float* hpAfter);
void      LvClearHud(DatapanelGUI* panel); // hide/clear LifeBar10* only — never touch LifeBar1
void      LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap); // after orig: Datapanel label + numeric Value + Green fill
int       LvReadMsvcString(const void* strObj, char* out, int outsz);
int       LvItemLooksLikeCatalyst(Item* item);
void      LvResolvePluginDirFromSelf();
