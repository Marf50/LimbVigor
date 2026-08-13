#pragma once
// Linux / IDE stand-ins. The Windows Actions build uses real KenshiLib headers.

#include <cstdint>
#include <cstring>
#include <string>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

struct HINSTANCE__;
typedef struct HINSTANCE__* HMODULE;
typedef unsigned long DWORD;
typedef char* LPSTR;
typedef const char* LPCSTR;
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
inline int GetModuleHandleExA(DWORD, LPCSTR, HMODULE*) { return 0; }
inline DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD) { return 0; }
inline HMODULE GetModuleHandleA(const char*) { return nullptr; }
inline void DebugLog(const char*) {}
inline void ErrorLog(const char*) {}

namespace KenshiLib {
enum HookStatus { SUCCESS, FAIL };
inline intptr_t GetRealAddress(void*) { return 0; }
template<typename T> inline intptr_t GetRealAddress(T) { return 0; }
inline HookStatus AddHook(void*, void*, void**) { return SUCCESS; }
}

enum StatsEnumerated { STAT_MEDIC = 9, STAT_TOUGHNESS = 21 };
enum itemType { ITEM_MEDRIGGING = 20 };
enum LeftRight { SIDE_LEFT = 1, SIDE_RIGHT = 2 };
enum LimbState { LIMB_ORIGINAL = 0, LIMB_STUMP = 1, LIMB_REPLACED = 2, LIMB_CRUSHED = 3 };

class Character;
class CharStats;
class MedicalSystem;
class RaceData;
class Item;
class GameData;
class DatapanelGUI;
class CharacterAnimal;

class GameData { public: char pad[0x80]; };

class RaceData {
public:
    GameData* data = nullptr;
    float hungerRate = 1.f;
    bool gigantic = false;
    bool robot = false;
    bool noHats = false, noShirts = false, noShoes = false;
};

class RobotLimbs {
public:
    enum Limb { LEFT_ARM, RIGHT_ARM, LEFT_LEG, RIGHT_LEG, NULL_LIMB };
    Character* character = nullptr;
    LimbState states[4] = {};
    Item* items[4] = {};
    LimbState getState(Limb) const { return LIMB_ORIGINAL; }
    Item* getLimb(Limb) const { return nullptr; }
    void setLimb(Limb, LimbState, Item*) {}
};

class CharStats {
public:
    MedicalSystem* medical = nullptr;
    Character* me = nullptr;
    float getStat(StatsEnumerated, bool) const { return 0.f; }
};

class MedicalSystem {
public:
    class HealthPartStatus {
    public:
        enum PartType { PART_TORSO, PART_LEG, PART_ARM, PART_HEAD };
        float flesh = 100.f, fleshStun = 0.f, _maxHealth = 100.f;
        bool isRobotic() { return false; }
        LimbState getRobotLimbState() { return LIMB_ORIGINAL; }
        void updateDerivedHealths() {}
    };
    float hunger = 1.f, fed = 1.f, blood = 100.f;
    float extraBloodLossFromBodyparts = 0.f;
    float currentBleedRate = 0.f;
    HealthPartStatus* leftLeg = nullptr;
    HealthPartStatus* rightLeg = nullptr;
    HealthPartStatus* leftArm = nullptr;
    HealthPartStatus* rightArm = nullptr;
    float knockoutTimer = 0.f;
    float restedState = 0.f;
    RobotLimbs* robotLimbs = nullptr;
    Character* me = nullptr;
    bool dead = false, unconcious = false;
    CharStats* stats = nullptr;
    void medicalUpdate(float) {}
    void getMedicalGUIData(DatapanelGUI*) {}
    bool applyDoctoring(float, Item*, float, Character*) { return false; }
    LimbState getLimbState(RobotLimbs::Limb) const { return LIMB_ORIGINAL; }
    HealthPartStatus* getPart(RobotLimbs::Limb) { return nullptr; }
    void setRobotLimbItem(RobotLimbs::Limb, Item*, bool) {}
    void updateStats() {}
    void validateHealthValues() {}
    float getMaxBlood() const { return 100.f; }
    bool isFed() const { return true; }
    bool isReallyHungry() const { return false; }
    bool isFullyRested() const { return false; }
    bool isDead() const { return dead; }
    bool isUnconcious() const { return unconcious; }
};

class Character {
public:
    RaceData* getRace() const { return nullptr; }
    CharStats* getStats() { return nullptr; }
    CharacterAnimal* isAnimal() { return nullptr; }
    bool hasSimilarItem(itemType) { return false; }
    bool isInCombatMode(bool, bool) const { return false; }
    void say_WithARepeatLimiter(const std::string&) {}
    bool isPlayerCharacter() const { return true; }
    bool isWithThePlayer() { return true; }
};

class Item {};
class DatapanelGUI {};
class CharacterAnimal {};
