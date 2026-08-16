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
int       LvIsSelectedCharacter(Character* me);
int       LvPanelIsLeftMedical(DatapanelGUI* panel); // hook DatapanelGUI* has Blood, or same pointer as gui→mainbar→getMedicalPanel
int       LvPanelHasBlood(DatapanelGUI* panel);
void      LvWalkSelPanel(DatapanelGUI* panel); // panel vs medicalPanel, Blood, getNumLines, every key
void      LvClearHud(DatapanelGUI* panel); // removeLine Hemolymph/Vigor/Battle-heat/Regrowth on after-orig path
void      LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap);
int       LvReadMsvcString(const void* strObj, char* out, int outsz);
int       LvItemLooksLikeCatalyst(Item* item);
void      LvResolvePluginDirFromSelf();
