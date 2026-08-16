#include "lv_persist.h"
#include "lv_config.h"
#include "lv_parts.h"

#include <cstdio>
#include <cstring>

static CharSnap g_chars[48];
static int      g_nChars = 0;
static int      g_dirty = 0;

#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
#include <Windows.h>
static unsigned g_lastSaveMs = 0;
#endif

static void Path(char* out, int n)
{
    const char* dir = LvPluginDir();
    if (dir && dir[0])
        std::snprintf(out, (size_t)n, "%s\\LimbVigor.progress", dir);
    else
        std::snprintf(out, (size_t)n, "LimbVigor.progress");
}

CharSnap* LvPersistFind(const char* name, int create)
{
    if (!name || !name[0]) name = "?";
    for (int i = 0; i < g_nChars; ++i)
        if (std::strcmp(g_chars[i].name, name) == 0)
            return &g_chars[i];
    if (!create || g_nChars >= 48) return nullptr;
    CharSnap* c = &g_chars[g_nChars++];
    std::memset(c, 0, sizeof(*c));
    std::snprintf(c->name, sizeof(c->name), "%s", name);
    for (int i = 0; i < LIMB_COUNT; ++i) c->lastStage[i] = -1;
    c->vigor = LvCfg().maxVigor * 0.35f;
    g_dirty = 1;
    return c;
}

void LvPersistLoad()
{
    char path[300];
    Path(path, 300);
    FILE* f = std::fopen(path, "r");
    if (!f) return;
    char line[192];
    if (!std::fgets(line, sizeof(line), f) || std::strncmp(line, "LVPROG", 6) != 0)
    {
        std::fclose(f);
        return;
    }
    g_nChars = 0;
    CharSnap* cur = nullptr;
    while (std::fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (line[0] == '@')
        {
            char nm[48] = {};
            std::sscanf(line + 1, " %47[^\r\n]", nm);
            cur = LvPersistFind(nm, 1);
            continue;
        }
        if (!cur) continue;
        if (line[0] == 'V')
        {
            float v = 0.f, cat = 0.f;
            int race = 0;
            if (std::sscanf(line, "V %f %f %d", &v, &cat, &race) >= 2)
            {
                cur->vigor = v;
                cur->catalystHours = cat;
                if (race >= 0 && race <= RACE_ANIMAL) cur->race = (RaceKind)race;
            }
            continue;
        }
        int slot = -1;
        float pr = 0.f;
        int st = -1;
        if (std::sscanf(line, "%d %f %d", &slot, &pr, &st) >= 2 && slot >= 0 && slot < LIMB_COUNT)
        {
            if (pr < 0.f) pr = 0.f;
            if (pr > 100.f) pr = 100.f;
            // Keep 100% / Grown so a -15 empty socket slots LV Grown
            // on load. Do not cap to 99.5 (that left Boop as knitting).
            if (st == LV_PART_GROWN || pr >= 99.5f)
            {
                pr = 100.f;
                if (st < 0) st = LV_PART_GROWN;
            }
            cur->progress[slot] = pr;
            cur->lastStage[slot] = st;
        }
    }
    std::fclose(f);
    g_dirty = 0;
    LvLog("LimbVigor: progress loaded");
}

void LvPersistSave(int force)
{
    if (!g_dirty && !force) return;
#if defined(_WIN32) && !defined(LIMBVIGOR_IDE)
    unsigned now = GetTickCount();
    if (!force && g_lastSaveMs && (now - g_lastSaveMs) < 8000u) return;
    g_lastSaveMs = now;
#endif
    char path[300];
    Path(path, 300);
    FILE* f = std::fopen(path, "w");
    if (!f) return;
    std::fprintf(f, "LVPROG 1\n");
    for (int i = 0; i < g_nChars; ++i)
    {
        CharSnap* c = &g_chars[i];
        std::fprintf(f, "@%s\n", c->name);
        std::fprintf(f, "V %.3f %.3f %d\n", c->vigor, c->catalystHours, (int)c->race);
        for (int s = 0; s < LIMB_COUNT; ++s)
        {
            if (c->progress[s] <= 0.f && c->lastStage[s] < 0) continue;
            std::fprintf(f, "%d %.3f %d\n", s, c->progress[s], c->lastStage[s]);
        }
    }
    std::fclose(f);
    g_dirty = 0;
}

void LvPersistForgetDead()
{
    // Keep the table small: unused names stay. A full wipe is not needed.
    g_dirty = 1;
}

void LvMarkDirty() { g_dirty = 1; }
