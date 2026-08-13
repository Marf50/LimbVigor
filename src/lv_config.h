#pragma once

// Tunables. Defaults match LimbVigor.cfg / the field-manual bench.

struct LvConfig
{
    int   enableHooks;
    int   enableHud;
    int   enableSpeech;
    int   debugLog;

    float maxVigor;

    float hivePassive;
    float shekPassive;
    float humanPassive;

    float hiveCombat;
    float shekCombat;
    float humanCombat;

    float fed;
    float bed;
    float starveDrain;

    float hiveGrowthDrain;
    float shekGrowthDrain;
    float humanGrowthDrain;

    float hiveGrowth;
    float shekGrowth;
    float humanGrowth;
    float bedGrowthMult;

    float shekToughness;
    float humanToughness;
    float humanMedic;

    float catalystHours;
    float restoredFlesh;
    float bleedPause;

    float secondsPerGameHour;
};

void        LvLoadConfig(const char* pluginDir);
const LvConfig& LvCfg();
const char* LvPluginDir();
void        LvSetPluginDir(const char* dir);
void        LvLog(const char* msg);
void        LvLogf(const char* fmt, ...);
void        LvErr(const char* msg);
void        LvDisableHud();
