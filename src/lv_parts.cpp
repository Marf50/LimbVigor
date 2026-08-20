#include "lv_parts.h"
#include "lv_config.h"
#include "lv_msvcstr.h"
#include "lv_sim.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>

#if defined(LIMBVIGOR_IDE)
#include "stubs/kenshi_ide_stubs.h"
#else
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Debug.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Item.h>
#include <kenshi/Enums.h>
#endif

// C++ try/catch — this file builds GameStr. Access violations are
// caught by the medicalUpdate hook SEH around DriveTick.
#define LV_TRY    try
#define LV_EXCEPT catch (...)

// FCS LimbSlot / RobotLimbs::Limb numbers.
static const int kFcsSlot[LIMB_COUNT] = {
    3, // RIGHT_LEG
    2, // LEFT_LEG
    1, // RIGHT_ARM
    0  // LEFT_ARM
};

#define LV_MESH_RLEG ".\\data\\items\\robotics\\economy leg R.mesh"
#define LV_MESH_LLEG ".\\data\\items\\robotics\\economy leg L.mesh"
#define LV_MESH_RARM ".\\data\\items\\robotics\\economy arm R.mesh"
#define LV_MESH_LARM ".\\data\\items\\robotics\\economy arm L.mesh"
#define LV_ICON_RLEG ".\\data\\items\\robotics\\economy leg R.png"
#define LV_ICON_LLEG ".\\data\\items\\robotics\\economy leg L.png"
#define LV_ICON_RARM ".\\data\\items\\robotics\\economy arm R.png"
#define LV_ICON_LARM ".\\data\\items\\robotics\\economy arm L.png"

// 4 limbs × 5 stages. Names start with "LV " so ReadLimb can tell
// a growing part from a real prosthetic. Grown is the finished limb.
static const LvPartDef kParts[LIMB_COUNT][LV_PART_COUNT] = {
    // RIGHT LEG
    {
        { "LV Stump Right Leg",    "lv-stump-r-leg", "A raw stump. Almost no push-off.",           "Economy Leg (right)", LV_MESH_RLEG, LV_ICON_RLEG, 3,  30.f, 0.15f, 0.20f, 0.10f, 1.f, 1.f, 1.f, 1.f, 0.4f },
        { "LV Budding Right Leg",  "lv-bud-r-leg",   "Flesh is budding on the stump.",             "Economy Leg (right)", LV_MESH_RLEG, LV_ICON_RLEG, 3,  45.f, 0.35f, 0.40f, 0.25f, 1.f, 1.f, 1.f, 1.f, 0.8f },
        { "LV Forming Right Leg",  "lv-form-r-leg",  "Bone and tendon are finding their shape.",   "Economy Leg (right)", LV_MESH_RLEG, LV_ICON_RLEG, 3,  65.f, 0.60f, 0.65f, 0.50f, 1.f, 1.f, 1.f, 1.f, 1.4f },
        { "LV Knitting Right Leg", "lv-knit-r-leg",  "Almost a limb. Soft. Do not test it.",       "Economy Leg (right)", LV_MESH_RLEG, LV_ICON_RLEG, 3,  85.f, 0.85f, 0.85f, 0.75f, 1.f, 1.f, 1.f, 1.f, 2.0f },
        { "LV Grown Right Leg",    "lv-grown-r-leg", "A new limb. Soft. Yours.",                   "Economy Leg (right)", LV_MESH_RLEG, LV_ICON_RLEG, 3, 100.f, 1.00f, 1.00f, 1.00f, 1.f, 1.f, 1.f, 1.f, 2.4f },
    },
    // LEFT LEG
    {
        { "LV Stump Left Leg",    "lv-stump-l-leg", "A raw stump. Almost no push-off.",           "Economy Leg (left)", LV_MESH_LLEG, LV_ICON_LLEG, 2,  30.f, 0.15f, 0.20f, 0.10f, 1.f, 1.f, 1.f, 1.f, 0.4f },
        { "LV Budding Left Leg",  "lv-bud-l-leg",   "Flesh is budding on the stump.",             "Economy Leg (left)", LV_MESH_LLEG, LV_ICON_LLEG, 2,  45.f, 0.35f, 0.40f, 0.25f, 1.f, 1.f, 1.f, 1.f, 0.8f },
        { "LV Forming Left Leg",  "lv-form-l-leg",  "Bone and tendon are finding their shape.",   "Economy Leg (left)", LV_MESH_LLEG, LV_ICON_LLEG, 2,  65.f, 0.60f, 0.65f, 0.50f, 1.f, 1.f, 1.f, 1.f, 1.4f },
        { "LV Knitting Left Leg", "lv-knit-l-leg",  "Almost a limb. Soft. Do not test it.",       "Economy Leg (left)", LV_MESH_LLEG, LV_ICON_LLEG, 2,  85.f, 0.85f, 0.85f, 0.75f, 1.f, 1.f, 1.f, 1.f, 2.0f },
        { "LV Grown Left Leg",    "lv-grown-l-leg", "A new limb. Soft. Yours.",                   "Economy Leg (left)", LV_MESH_LLEG, LV_ICON_LLEG, 2, 100.f, 1.00f, 1.00f, 1.00f, 1.f, 1.f, 1.f, 1.f, 2.4f },
    },
    // RIGHT ARM
    {
        { "LV Stump Right Arm",    "lv-stump-r-arm", "A raw stump. Almost no push-off.",           "Economy Arm (right)", LV_MESH_RARM, LV_ICON_RARM, 1,  30.f, 1.f, 1.f, 0.10f, 0.15f, 0.20f, 0.15f, 0.10f, 0.3f },
        { "LV Budding Right Arm",  "lv-bud-r-arm",   "Flesh is budding on the stump.",             "Economy Arm (right)", LV_MESH_RARM, LV_ICON_RARM, 1,  45.f, 1.f, 1.f, 0.25f, 0.35f, 0.40f, 0.30f, 0.25f, 0.6f },
        { "LV Forming Right Arm",  "lv-form-r-arm",  "Bone and tendon are finding their shape.",   "Economy Arm (right)", LV_MESH_RARM, LV_ICON_RARM, 1,  65.f, 1.f, 1.f, 0.50f, 0.60f, 0.70f, 0.55f, 0.50f, 1.1f },
        { "LV Knitting Right Arm", "lv-knit-r-arm",  "Almost a limb. Soft. Do not test it.",       "Economy Arm (right)", LV_MESH_RARM, LV_ICON_RARM, 1,  85.f, 1.f, 1.f, 0.75f, 0.85f, 0.90f, 0.80f, 0.80f, 1.6f },
        { "LV Grown Right Arm",    "lv-grown-r-arm", "A new limb. Soft. Yours.",                   "Economy Arm (right)", LV_MESH_RARM, LV_ICON_RARM, 1, 100.f, 1.f, 1.f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.9f },
    },
    // LEFT ARM
    {
        { "LV Stump Left Arm",    "lv-stump-l-arm", "A raw stump. Almost no push-off.",           "Economy Arm (left)", LV_MESH_LARM, LV_ICON_LARM, 0,  30.f, 1.f, 1.f, 0.10f, 0.15f, 0.20f, 0.15f, 0.10f, 0.3f },
        { "LV Budding Left Arm",  "lv-bud-l-arm",   "Flesh is budding on the stump.",             "Economy Arm (left)", LV_MESH_LARM, LV_ICON_LARM, 0,  45.f, 1.f, 1.f, 0.25f, 0.35f, 0.40f, 0.30f, 0.25f, 0.6f },
        { "LV Forming Left Arm",  "lv-form-l-arm",  "Bone and tendon are finding their shape.",   "Economy Arm (left)", LV_MESH_LARM, LV_ICON_LARM, 0,  65.f, 1.f, 1.f, 0.50f, 0.60f, 0.70f, 0.55f, 0.50f, 1.1f },
        { "LV Knitting Left Arm", "lv-knit-l-arm",  "Almost a limb. Soft. Do not test it.",       "Economy Arm (left)", LV_MESH_LARM, LV_ICON_LARM, 0,  85.f, 1.f, 1.f, 0.75f, 0.85f, 0.90f, 0.80f, 0.80f, 1.6f },
        { "LV Grown Left Arm",    "lv-grown-l-arm", "A new limb. Soft. Yours.",                   "Economy Arm (left)", LV_MESH_LARM, LV_ICON_LARM, 0, 100.f, 1.f, 1.f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.9f },
    },
};

const LvPartDef* LvPartFor(int limbId, int stage)
{
    if (limbId < 0 || limbId >= LIMB_COUNT) return nullptr;
    if (stage < 0 || stage >= LV_PART_COUNT) return nullptr;
    return &kParts[limbId][stage];
}

const char* LvPartStageName(int stage)
{
    switch (stage)
    {
    case LV_PART_STUMP:    return "stump";
    case LV_PART_BUDDING:  return "budding";
    case LV_PART_FORMING:  return "forming";
    case LV_PART_KNITTING: return "knitting";
    case LV_PART_GROWN:    return "grown";
    default: return "limb";
    }
}

int LvPartStageFromProgress(float progress)
{
    if (progress < 25.f) return LV_PART_STUMP;
    if (progress < 50.f) return LV_PART_BUDDING;
    if (progress < 75.f) return LV_PART_FORMING;
    if (progress < 100.f) return LV_PART_KNITTING;
    return LV_PART_GROWN;
}

int LvPartSlotForLimb(int limbId)
{
    if (limbId < 0 || limbId >= LIMB_COUNT) return -1;
    return kFcsSlot[limbId];
}

#if defined(LIMBVIGOR_IDE)

int  LvIsGrowthPart(Item*) { return 0; }
int  LvGrowthPartStage(Item*) { return -1; }
int  LvEquipGrowthPart(MedicalSystem*, int, int) { return 0; }
void LvClearGrowthPart(MedicalSystem*, int) {}
void LvSyncGrowthParts(MedicalSystem*, const CharSnap*) {}
int  LvSyncOneLimb(MedicalSystem*, const CharSnap*, int) { return 0; }

#else

static const RobotLimbs::Limb kGameLimb[LIMB_COUNT] = {
    RobotLimbs::RIGHT_LEG,
    RobotLimbs::LEFT_LEG,
    RobotLimbs::RIGHT_ARM,
    RobotLimbs::LEFT_ARM
};

// RootObjectFactory.h pulls boost/thread (auto-links a .lib we do not ship).
// createItem is a non-virtual at the documented RVA. this = factory.
static const intptr_t kRvaCreateItem = 0x57FFD0;
static const intptr_t kRvaNullHand   = 0x1E375F8;

typedef Item* (*FnCreateItem)(void* factory, GameData* gd, const void* handle,
    GameData* mesh, GameData* mat, int level, void* faction);

static std::string& GS(GameStr* s)
{
    return *reinterpret_cast<std::string*>(s);
}

static GameData* ItemData(Item* item)
{
    if (!item) return nullptr;
    GameData* data = nullptr;
    LV_TRY { std::memcpy(&data, (const char*)(const void*)item + 0x10, sizeof(data)); }
    LV_EXCEPT { data = nullptr; }
    return data;
}

static int NameLooksLikeOurs(const char* s)
{
    if (!s || !s[0]) return 0;
    if (s[0] == 'L' && s[1] == 'V' && s[2] == ' ') return 1;
    if (s[0] == 'l' && s[1] == 'v' && s[2] == '-') return 1;
    return 0;
}

static int DataLooksLikeOurs(GameData* data)
{
    if (!data) return 0;
    const char* base = (const char*)(const void*)data;
    if (GameStrContainsI(base + 0x28, "lv ")) return 1;
    if (GameStrContainsI(base + 0x58, "lv-")) return 1;
    return 0;
}

int LvIsGrowthPart(Item* item)
{
    return DataLooksLikeOurs(ItemData(item)) ? 1 : 0;
}

int LvGrowthPartStage(Item* item)
{
    GameData* data = ItemData(item);
    if (!data) return -1;
    const char* base = (const char*)(const void*)data;
    char name[80] = {};
    if (!GameStrRead(base + 0x28, name, (int)sizeof(name)))
        GameStrRead(base + 0x58, name, (int)sizeof(name));
    if (!NameLooksLikeOurs(name) && !DataLooksLikeOurs(data)) return -1;
    if (std::strstr(name, "rown") || std::strstr(name, "Grown")) return LV_PART_GROWN;
    if (std::strstr(name, "nit") || std::strstr(name, "knit")) return LV_PART_KNITTING;
    if (std::strstr(name, "orm") || std::strstr(name, "form")) return LV_PART_FORMING;
    if (std::strstr(name, "ud") || std::strstr(name, "bud")) return LV_PART_BUDDING;
    if (std::strstr(name, "tump") || std::strstr(name, "stump")) return LV_PART_STUMP;
    return LV_PART_STUMP;
}

static Item* Equipped(MedicalSystem* med, int limbId)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return nullptr;
    RobotLimbs* robots = nullptr;
    LV_TRY { robots = med->robotLimbs; }
    LV_EXCEPT { robots = nullptr; }
    if (!robots) return nullptr;
    Item* it = nullptr;
    LV_TRY { it = robots->getLimb(kGameLimb[limbId]); }
    LV_EXCEPT { it = nullptr; }
    return it;
}

static void* ExeBase()
{
    HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
    if (!exe) exe = GetModuleHandleA("kenshi_GOG_x64.exe");
    return (void*)exe;
}

static GameData* LookupOurData(const char* stringId, const char* name)
{
    if (!ou) return nullptr;
    GameData* gd = nullptr;
    if (stringId && stringId[0])
    {
        GameStr sid;
        GameStrSet(&sid, stringId);
        LV_TRY { gd = ou->gamedata.getData(GS(&sid)); }
        LV_EXCEPT { gd = nullptr; }
        if (gd) return gd;
        LV_TRY { gd = ou->gamedata.getData(GS(&sid), LIMB_REPLACEMENT); }
        LV_EXCEPT { gd = nullptr; }
        if (gd) return gd;
    }
    if (name && name[0])
    {
        GameStr nm;
        GameStrSet(&nm, name);
        LV_TRY { gd = ou->gamedata.getDataByName(GS(&nm), LIMB_REPLACEMENT); }
        LV_EXCEPT { gd = nullptr; }
    }
    return gd;
}

static int StrHasI(const char* hay, const char* needle)
{
    if (!hay || !needle || !needle[0]) return 0;
    for (const char* h = hay; *h; ++h)
    {
        const char* a = h;
        const char* b = needle;
        while (*a && *b)
        {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (!*b) return 1;
    }
    return 0;
}

// GameData::filesdata lives at 0x1F8 (boost unordered_map<string,string>).
// Do not call find() — that would compile a VS2022 walk into our DLL.
// Walk the map object and one level of heap nodes for GameStr keys/paths.
static int DataHasMeshIcon(GameData* gd)
{
    if (!gd) return 0;
    const char* map = (const char*)(const void*)gd + 0x1F8;
    int sawMesh = 0;
    int sawIcon = 0;

    for (int i = 0; i < 8; ++i)
    {
        void* ptr = nullptr;
        std::memcpy(&ptr, map + i * (int)sizeof(void*), sizeof(ptr));
        if (!ptr) continue;
        const char* bucket = (const char*)ptr;
        for (int b = 0; b < 24; ++b)
        {
            void* node = nullptr;
            std::memcpy(&node, bucket + b * (int)sizeof(void*), sizeof(node));
            if (!node) continue;
            const char* key = (const char*)node + sizeof(void*);
            const char* val = key + 32; // MSVC 2010 std::string
            char k[64] = {};
            char v[160] = {};
            if (GameStrRead(key, k, (int)sizeof(k)))
            {
                if (StrHasI(k, "mesh") && GameStrRead(val, v, (int)sizeof(v)) && v[0])
                    sawMesh = 1;
                if (StrHasI(k, "icon") && GameStrRead(val, v, (int)sizeof(v)) && v[0])
                    sawIcon = 1;
            }
            if (GameStrRead(val, v, (int)sizeof(v)))
            {
                if (StrHasI(v, ".mesh")) sawMesh = 1;
                if (StrHasI(v, ".png") || StrHasI(v, ".dds")) sawIcon = 1;
            }
        }
    }
    return (sawMesh && sawIcon) ? 1 : 0;
}

static int DataLooksEmptyFiles(GameData* gd)
{
    if (!gd) return 1;
    const char* map = (const char*)(const void*)gd + 0x1F8;
    for (int off = 8; off <= 56; off += 8)
    {
        std::uint64_t n = 0;
        std::memcpy(&n, map + off, sizeof(n));
        if (n >= 2 && n <= 16)
            return 0;
    }
    return 1;
}

static int ReadI32(const unsigned char* p, int* off, int size, int* out)
{
    if (!p || !off || !out || *off + 4 > size) return 0;
    std::memcpy(out, p + *off, 4);
    *off += 4;
    return 1;
}

static int ReadModStr(const unsigned char* p, int* off, int size, char* out, int outsz)
{
    int n = 0;
    if (!ReadI32(p, off, size, &n)) return 0;
    if (n < 0 || n > 4000 || *off + n > size) return 0;
    if (out && outsz > 0)
    {
        int c = n < outsz - 1 ? n : outsz - 1;
        std::memcpy(out, p + *off, (size_t)c);
        out[c] = 0;
    }
    *off += n;
    return 1;
}

// On-disk LimbVigor.mod next to the DLL. Fail closed if FileValues are missing.
static int ModRecordHasMeshIcon(const char* stringId)
{
    if (!stringId || !stringId[0]) return 0;
    const char* dir = LvPluginDir();
    char path[MAX_PATH];
    if (dir && dir[0])
        std::snprintf(path, sizeof(path), "%s\\LimbVigor.mod", dir);
    else
        std::snprintf(path, sizeof(path), "LimbVigor.mod");

    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0) f = nullptr;
#else
    f = std::fopen(path, "rb");
#endif
    if (!f) return 0;

    unsigned char* buf = nullptr;
    int size = 0;
    if (std::fseek(f, 0, SEEK_END) == 0)
    {
        long n = std::ftell(f);
        if (n > 32 && n < 2 * 1024 * 1024)
        {
            size = (int)n;
            buf = (unsigned char*)std::malloc((size_t)size);
            if (buf)
            {
                std::rewind(f);
                if ((int)std::fread(buf, 1, (size_t)size, f) != size)
                {
                    std::free(buf);
                    buf = nullptr;
                }
            }
        }
    }
    std::fclose(f);
    if (!buf) return 0;

    int off = 0;
    int type = 0, ver = 0, lastId = 0, count = 0;
    int ok = 0;
    if (ReadI32(buf, &off, size, &type) && type == 16
     && ReadI32(buf, &off, size, &ver)
     && ReadModStr(buf, &off, size, nullptr, 0)
     && ReadModStr(buf, &off, size, nullptr, 0)
     && ReadModStr(buf, &off, size, nullptr, 0)
     && ReadModStr(buf, &off, size, nullptr, 0)
     && ReadI32(buf, &off, size, &lastId)
     && ReadI32(buf, &off, size, &count)
     && count > 0 && count <= 64)
    {
        for (int i = 0; i < count; ++i)
        {
            const int start = off;
            int length = 0, itype = 0, iid = 0;
            char sid[80] = {};
            if (!ReadI32(buf, &off, size, &length)) break;
            if (!ReadI32(buf, &off, size, &itype)) break;
            if (!ReadI32(buf, &off, size, &iid)) break;
            if (!ReadModStr(buf, &off, size, nullptr, 0)) break;
            if (!ReadModStr(buf, &off, size, sid, (int)sizeof(sid))) break;
            int chg = 0;
            if (!ReadI32(buf, &off, size, &chg)) break;

            int nbool = 0;
            if (!ReadI32(buf, &off, size, &nbool) || nbool < 0 || nbool > 32) break;
            for (int b = 0; b < nbool; ++b)
            {
                if (!ReadModStr(buf, &off, size, nullptr, 0)) { nbool = -1; break; }
                if (off >= size) { nbool = -1; break; }
                off += 1;
            }
            if (nbool < 0) break;

            int nfloat = 0;
            if (!ReadI32(buf, &off, size, &nfloat) || nfloat < 0 || nfloat > 64) break;
            for (int fl = 0; fl < nfloat; ++fl)
            {
                if (!ReadModStr(buf, &off, size, nullptr, 0)) { nfloat = -1; break; }
                off += 4;
            }
            if (nfloat < 0) break;

            int nint = 0;
            if (!ReadI32(buf, &off, size, &nint) || nint < 0 || nint > 64) break;
            for (int iv = 0; iv < nint; ++iv)
            {
                if (!ReadModStr(buf, &off, size, nullptr, 0)) { nint = -1; break; }
                off += 4;
            }
            if (nint < 0) break;

            int nvec3 = 0, nvec4 = 0;
            if (!ReadI32(buf, &off, size, &nvec3) || nvec3 != 0) break;
            if (!ReadI32(buf, &off, size, &nvec4) || nvec4 != 0) break;

            int nstr = 0;
            if (!ReadI32(buf, &off, size, &nstr) || nstr < 0 || nstr > 16) break;
            for (int s = 0; s < nstr; ++s)
            {
                if (!ReadModStr(buf, &off, size, nullptr, 0)) { nstr = -1; break; }
                if (!ReadModStr(buf, &off, size, nullptr, 0)) { nstr = -1; break; }
            }
            if (nstr < 0) break;

            int nfile = 0;
            if (!ReadI32(buf, &off, size, &nfile) || nfile < 0 || nfile > 16) break;
            int mesh = 0, icon = 0;
            for (int fv = 0; fv < nfile; ++fv)
            {
                char key[40] = {};
                char val[200] = {};
                if (!ReadModStr(buf, &off, size, key, (int)sizeof(key))) { nfile = -1; break; }
                if (!ReadModStr(buf, &off, size, val, (int)sizeof(val))) { nfile = -1; break; }
                if (StrHasI(key, "mesh") && val[0] && StrHasI(val, ".mesh")) mesh = 1;
                if (StrHasI(key, "icon") && val[0]) icon = 1;
            }
            if (nfile < 0) break;

            if (std::strcmp(sid, stringId) == 0)
            {
                ok = (mesh && icon) ? 1 : 0;
                break;
            }
            off = start + length;
            if (off <= start || off > size) break;
        }
    }
    std::free(buf);
    return ok;
}

static int RecordCanEquip(GameData* gd, const LvPartDef* def)
{
    if (!gd || !def) return 0;
    if (DataHasMeshIcon(gd)) return 1;
    if (DataLooksEmptyFiles(gd))
        return 0;
    // Walk inconclusive — trust the .mod we shipped next to the DLL.
    if (def->mesh && def->mesh[0] && def->icon && def->icon[0]
     && ModRecordHasMeshIcon(def->stringId))
        return 1;
    return 0;
}

static Item* MakeItem(GameData* gd)
{
    if (!gd || !ou || !ou->theFactory) return nullptr;
    void* base = ExeBase();
    if (!base) return nullptr;
    FnCreateItem fn = (FnCreateItem)((unsigned char*)base + kRvaCreateItem);
    const void* nullHand = (const void*)((unsigned char*)base + kRvaNullHand);
    Item* item = nullptr;
    LV_TRY
    {
        item = fn(ou->theFactory, gd, nullHand, nullptr, nullptr, 0, nullptr);
    }
    LV_EXCEPT { item = nullptr; }
    return item;
}

static int SlotPart(MedicalSystem* med, int limbId, Item* item, int midGrowth)
{
    if (!med) return 0;
    const RobotLimbs::Limb limb = kGameLimb[limbId];
    int ok = 0;
    RobotLimbs* robots = nullptr;
    LV_TRY { robots = med->robotLimbs; }
    LV_EXCEPT { robots = nullptr; }
    if (robots)
    {
        LV_TRY
        {
            robots->setLimb(limb, item ? LIMB_REPLACED : LIMB_STUMP, item);
            ok = 1;
        }
        LV_EXCEPT { ok = 0; }
    }
    LV_TRY { med->setRobotLimbItem(limb, item, false); }
    LV_EXCEPT {}
    /* validateHealthValues on a mid-growth nub collapsed 5/75 → 5/5 (v1.29). */
    if (!midGrowth)
    {
        LV_TRY { med->validateHealthValues(); }
        LV_EXCEPT {}
    }
    LV_TRY { med->updateStats(); }
    LV_EXCEPT {}
    return ok;
}

int LvEquipGrowthPart(MedicalSystem* med, int limbId, int stage)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return 0;
    if (stage < 0 || stage >= LV_PART_COUNT) return 0;

    Item* cur = Equipped(med, limbId);
    const int have = cur ? LvGrowthPartStage(cur) : -1;
    /* Never GROWN as the first write. After knitting, GROWN is the next type. */
    if (stage == LV_PART_GROWN && have < LV_PART_KNITTING)
        return 0;

    const LvPartDef* def = LvPartFor(limbId, stage);
    if (!def) return 0;

    if (cur && have == stage)
        return 1;

    static unsigned failUntil[LIMB_COUNT] = {};
#if defined(_WIN32)
    unsigned now = GetTickCount();
    if (failUntil[limbId] && now < failUntil[limbId])
        return 0;
#else
    unsigned now = 0;
#endif

    if (!ou)
    {
        static int once = 0;
        if (!once) { LvErr("LimbVigor: no GameWorld (ou) — cannot create growth parts"); once = 1; }
        return 0;
    }

    GameData* gd = LookupOurData(def->stringId, def->name);
    if (!gd)
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: no LimbVigor.mod GameData for %s — skip (not using Economy)",
            def->name);
        return 0;
    }
    if (!RecordCanEquip(gd, def))
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: mesh-less GameData for %s — skip (not using Economy)",
            def->name);
        return 0;
    }

    Item* item = MakeItem(gd);
    if (!item)
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: createItem failed for %s", def->name);
        return 0;
    }

    if (!SlotPart(med, limbId, item, 1))
    {
        failUntil[limbId] = now + 15000u;
        LvLogf("LimbVigor: setLimb REPLACED failed for %s", def->name);
        return 0;
    }

    failUntil[limbId] = 0;
    const char* from = (have < 0 || have == LV_PART_STUMP)
        ? "STUMP" : LvPartStageName(have);
    float hp = def->hp, mx = def->hp;
    MedicalSystem::HealthPartStatus* part = nullptr;
    LV_TRY { part = med->getPart(kGameLimb[limbId]); }
    LV_EXCEPT { part = nullptr; }
    if (part)
    {
        LV_TRY { hp = part->flesh; mx = part->_maxHealth; }
        LV_EXCEPT {}
    }
    LvLogf("LimbVigor: %s %s → %s nub attached hp=%.1f/%.1f",
        LvLimbLabel((LimbId)limbId), from, LvPartStageName(stage), hp, mx);
    LvLogf("LimbVigor: slotted %s (ours) on %s",
        def->name, LvLimbLabel((LimbId)limbId));
    return 1;
}

void LvClearGrowthPart(MedicalSystem* med, int limbId)
{
    if (!med || limbId < 0 || limbId >= LIMB_COUNT) return;
    Item* cur = Equipped(med, limbId);
    if (cur && !LvIsGrowthPart(cur))
        return;
    SlotPart(med, limbId, nullptr, 1);
}

int LvSyncOneLimb(MedicalSystem* med, const CharSnap* snap, int limbId)
{
    if (!med || !snap) return 0;
    if (limbId < 0 || limbId >= LIMB_COUNT) return 0;
    if (snap->race == RACE_SKELETON || snap->race == RACE_ANIMAL) return 0;

    Item* cur = Equipped(med, limbId);
    if (cur && !LvIsGrowthPart(cur))
        return 0; // real prosthetic — leave it.

    /* 75-HP arms were a false alarm. Intact / injured flesh is not a
     * growth socket. Do not slot an LV part on a whole limb with HP>=10. */
    if (snap->limbs[limbId] == LIMB_KIND_WHOLE && snap->limbHp[limbId] >= 10.f)
        return 0;

    int need = 0;
    int empty15 = 0;
    if (snap->limbs[limbId] == LIMB_KIND_STUMP || snap->limbs[limbId] == LIMB_KIND_CRUSHED)
    {
        need = 1;
        if (!cur)
            empty15 = 1;
    }
    else if (!cur && snap->limbs[limbId] != LIMB_KIND_PROSTHETIC
             && snap->limbHp[limbId] < 10.f
             && (snap->progress[limbId] > 0.f || snap->lastStage[limbId] >= 0
                 || snap->progress[limbId] >= 99.5f))
    {
        // Persist knows this socket; game read it as whole with
        // nothing equipped (the -15 empty-stump case). Not a 75-HP arm.
        need = 1;
        empty15 = 1;
    }
    if (!need) return 0;

    int want = LvPartStageFromProgress(snap->progress[limbId]);
    int have = cur ? LvGrowthPartStage(cur) : -1;
    /* First write is always STUMP nub. Then one type at a time. Never GROWN first. */
    int stage = want;
    if (have < 0)
        stage = LV_PART_STUMP;
    else if (want > have + 1)
        stage = have + 1;
    else if (want < have)
        stage = have;
    if (stage == LV_PART_GROWN && have < LV_PART_KNITTING)
        stage = LV_PART_KNITTING;
    if (LvEquipGrowthPart(med, limbId, stage) && empty15)
    {
        LvLogf("LimbVigor: slotted %s %s (-15 empty socket)",
            "LV part",
            LvLimbLabel((LimbId)limbId));
    }
    return 1;
}

void LvSyncGrowthParts(MedicalSystem* med, const CharSnap* snap)
{
    if (!med || !snap) return;
    if (snap->race == RACE_SKELETON || snap->race == RACE_ANIMAL) return;
    for (int i = 0; i < LIMB_COUNT; ++i)
        LvSyncOneLimb(med, snap, i);
}

#endif
