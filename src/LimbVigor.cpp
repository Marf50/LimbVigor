#include "lv_config.h"
#include "lv_game.h"
#include "lv_hooks.h"
#include "lv_persist.h"

#if defined(_MSC_VER)
#define LV_EXPORT __declspec(dllexport)
#else
#define LV_EXPORT __attribute__((visibility("default")))
#endif

// RE_Kenshi entry. Mangled export: ?startPlugin@@YAXXZ
LV_EXPORT void startPlugin()
{
    LvResolvePluginDirFromSelf();
    LvLoadConfig(LvPluginDir());
    LvGameInit();
    LvPersistLoad();
    LvInstallHooks();
    if (LvCfg().enableHud)
        LvLog("LimbVigor: ready v1.8.6 — HUD is created on the title screen (UI thread), like KillButton.");
    else
        LvLog("LimbVigor: ready — HUD off (EnableHud=0)");
}
