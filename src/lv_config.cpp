#include "lv_config.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>

#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
#include <Debug.h>
#endif

static LvConfig g_cfg = {
    1, 1, 1, 1,
    100.f,
    5.0f, 1.5f, 0.6f,
    6.0f, 14.0f, 8.0f,
    2.0f, 3.5f, 4.0f,
    4.0f, 6.0f, 8.0f,
    1.70f, 1.05f, 0.70f, 2.0f,
    20.f, 40.f, 25.f,
    20.f, 0.22f, 1.5f,
    53.33f
};

static char g_dir[260] = {};

const LvConfig& LvCfg() { return g_cfg; }
const char* LvPluginDir() { return g_dir; }
void LvSetPluginDir(const char* dir)
{
    g_dir[0] = 0;
    if (!dir) return;
    std::snprintf(g_dir, sizeof(g_dir), "%s", dir);
}

void LvLog(const char* msg)
{
#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
    if (g_cfg.debugLog && msg) DebugLog(msg);
#else
    (void)msg;
#endif
}

void LvErr(const char* msg)
{
#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
    if (msg) ErrorLog(msg);
#else
    (void)msg;
#endif
}

void LvLogf(const char* fmt, ...)
{
    if (!g_cfg.debugLog || !fmt) return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LvLog(buf);
}

static int ParseBool(const char* v)
{
    if (!v || !*v) return 0;
    if (v[0]=='1'||v[0]=='y'||v[0]=='Y'||v[0]=='t'||v[0]=='T') return 1;
    if (std::strcmp(v, "on")==0 || std::strcmp(v, "ON")==0) return 1;
    return 0;
}

void LvLoadConfig(const char* pluginDir)
{
    if (pluginDir) LvSetPluginDir(pluginDir);

    char path[300];
    if (g_dir[0])
        std::snprintf(path, sizeof(path), "%s\\LimbVigor.cfg", g_dir);
    else
        std::snprintf(path, sizeof(path), "LimbVigor.cfg");

    FILE* f = std::fopen(path, "r");
    if (!f)
    {
        // also try config.ini next to the DLL (mods/LimbVigor/ layout)
        if (g_dir[0])
            std::snprintf(path, sizeof(path), "%s\\config.ini", g_dir);
        else
            std::snprintf(path, sizeof(path), "config.ini");
        f = std::fopen(path, "r");
    }
    if (!f)
    {
        LvLog("LimbVigor: no config — using defaults");
        return;
    }

    char line[512];
    while (std::fgets(line, sizeof(line), f))
    {
        char* s = line;
        while (*s==' '||*s=='\t') ++s;
        if (*s=='#' || *s==';' || *s=='\n' || *s=='\r' || !*s) continue;
        char* eq = std::strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = s;
        char* val = eq + 1;
        char* ke = key + std::strlen(key);
        while (ke > key && (ke[-1]==' '||ke[-1]=='\t')) *--ke = 0;
        while (*val==' '||*val=='\t') ++val;
        char* ve = val + std::strlen(val);
        while (ve > val && (ve[-1]=='\n'||ve[-1]=='\r'||ve[-1]==' '||ve[-1]=='\t')) *--ve = 0;

        auto fval = [&]() { return (float)std::atof(val); };
        auto bval = [&]() { return ParseBool(val); };

        if (!std::strcmp(key, "EnableHooks")) g_cfg.enableHooks = bval();
        else if (!std::strcmp(key, "EnableHud")) g_cfg.enableHud = bval();
        else if (!std::strcmp(key, "EnableSpeech")) g_cfg.enableSpeech = bval();
        else if (!std::strcmp(key, "DebugLog")) g_cfg.debugLog = bval();
        else if (!std::strcmp(key, "max_vigor") || !std::strcmp(key, "MaxVigor")) g_cfg.maxVigor = fval();
        else if (!std::strcmp(key, "hive_passive")) g_cfg.hivePassive = fval();
        else if (!std::strcmp(key, "shek_passive")) g_cfg.shekPassive = fval();
        else if (!std::strcmp(key, "human_passive")) g_cfg.humanPassive = fval();
        else if (!std::strcmp(key, "hive_combat")) g_cfg.hiveCombat = fval();
        else if (!std::strcmp(key, "shek_combat")) g_cfg.shekCombat = fval();
        else if (!std::strcmp(key, "human_combat")) g_cfg.humanCombat = fval();
        else if (!std::strcmp(key, "fed")) g_cfg.fed = fval();
        else if (!std::strcmp(key, "bed")) g_cfg.bed = fval();
        else if (!std::strcmp(key, "starve_drain")) g_cfg.starveDrain = fval();
        else if (!std::strcmp(key, "hive_growth_drain")) g_cfg.hiveGrowthDrain = fval();
        else if (!std::strcmp(key, "shek_growth_drain")) g_cfg.shekGrowthDrain = fval();
        else if (!std::strcmp(key, "human_growth_drain")) g_cfg.humanGrowthDrain = fval();
        else if (!std::strcmp(key, "hive_growth")) g_cfg.hiveGrowth = fval();
        else if (!std::strcmp(key, "shek_growth")) g_cfg.shekGrowth = fval();
        else if (!std::strcmp(key, "human_growth")) g_cfg.humanGrowth = fval();
        else if (!std::strcmp(key, "bed_growth_mult")) g_cfg.bedGrowthMult = fval();
        else if (!std::strcmp(key, "shek_toughness")) g_cfg.shekToughness = fval();
        else if (!std::strcmp(key, "human_toughness")) g_cfg.humanToughness = fval();
        else if (!std::strcmp(key, "human_medic")) g_cfg.humanMedic = fval();
        else if (!std::strcmp(key, "catalyst_hours")) g_cfg.catalystHours = fval();
        else if (!std::strcmp(key, "restored_flesh")) g_cfg.restoredFlesh = fval();
        else if (!std::strcmp(key, "bleed_pause")) g_cfg.bleedPause = fval();
        else if (!std::strcmp(key, "seconds_per_game_hour")) g_cfg.secondsPerGameHour = fval();
    }
    std::fclose(f);

    if (g_cfg.maxVigor < 10.f) g_cfg.maxVigor = 10.f;
    if (g_cfg.secondsPerGameHour < 1.f) g_cfg.secondsPerGameHour = 53.33f;
    if (g_cfg.restoredFlesh < 0.05f) g_cfg.restoredFlesh = 0.05f;
    if (g_cfg.restoredFlesh > 0.8f) g_cfg.restoredFlesh = 0.8f;
    if (g_cfg.catalystHours < 1.f) g_cfg.catalystHours = 1.f;
    if (g_cfg.bedGrowthMult < 1.f) g_cfg.bedGrowthMult = 1.f;

    LvLog("LimbVigor: config loaded");
}
