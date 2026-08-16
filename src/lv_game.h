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
void      LvNoteMainBar(void* mainbar); // MainBarGUI* — no Character
int       LvPanelIsLeftMedical(DatapanelGUI* panel); // same pointer as getMedicalPanel / +0x188, or Blood on this DatapanelGUI*
void      LvNoteSelPanel(DatapanelGUI* panel); // stash selection-info DatapanelGUI* (Blood or medicalPanel)
DatapanelGUI* LvSelPanel();
int       LvPanelHasBlood(DatapanelGUI* panel);
Character* LvCharFromHand(const void* h); // hand::getCharacter — not virtual
void      LvWalkSelPanel(DatapanelGUI* panel); // panel vs medicalPanel, Blood, getNumLines, every key
void      LvLogMedicalPanelOnce(DatapanelGUI* panel);
void      LvClearHud(DatapanelGUI* panel); // removeLine Hemolymph/Vigor/Battle-heat
void      LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap);
int       LvReadMsvcString(const void* strObj, char* out, int outsz);
int       LvItemLooksLikeCatalyst(Item* item);
void      LvResolvePluginDirFromSelf();
