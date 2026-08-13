#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"

#include <cstring>

#if defined(LIMBVIGOR_IDE)

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudInstall() {}

#else

#include <Windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>

// TitleScreen::_CONSTRUCTOR  (KenshiLib TitleScreen.h)
// Same address KillButton hooks. UI thread, MyGUI is alive, skins loaded.
static const int kRvaTitleCtor = 0x917740;

static CharSnap g_snap;
static volatile int g_have = 0;
static volatile int g_ingame = 0;
static volatile int g_ready = 0;

static MyGUI::Window* g_win = nullptr;
static MyGUI::Button* g_l1 = nullptr;
static MyGUI::Button* g_l2 = nullptr;
static MyGUI::Button* g_l3 = nullptr;
static int g_painted = 0;

static void SetCap(MyGUI::Button* b, const char* s, int show)
{
    if (!b) return;
    try { b->setVisible(show ? true : false); }
    catch (...) {}
    if (!show || !s) return;
    try { b->setCaption(s); }
    catch (...) {}
}

// Runs on the MyGUI / render thread. Widgets already exist.
static void OnFrame(float)
{
    if (!g_win) return;
    if (!g_have || !g_ingame || !LvCfg().enableHud)
    {
        try { g_win->setVisible(false); }
        catch (...) {}
        return;
    }

    char bar1[96], bar2[96], tip[220];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(&g_snap, bar1, (int)sizeof(bar1), &f1,
               bar2, (int)sizeof(bar2), &f2,
               tip, (int)sizeof(tip));

    try { g_win->setVisible(true); }
    catch (...) {}
    SetCap(g_l1, bar1, 1);
    SetCap(g_l2, bar2[0] ? bar2 : "", bar2[0] ? 1 : 0);
    SetCap(g_l3, tip, tip[0] ? 1 : 0);

    if (!g_painted)
    {
        g_painted = 1;
        LvLogf("LimbVigor: HUD painted  %s", bar1);
    }
}

static void* (*Title_orig)(void*) = nullptr;

// Exact KillButton shape: create the window here, never from a game tick.
static void* Title_hook(void* self)
{
    void* ts = Title_orig ? Title_orig(self) : self;
    if (g_ready) return ts;
    if (!LvCfg().enableHud) return ts;

    MyGUI::Gui* gui = nullptr;
    try { gui = MyGUI::Gui::getInstancePtr(); }
    catch (...) { gui = nullptr; }
    if (!gui)
    {
        LvErr("LimbVigor: no MyGUI at title screen");
        return ts;
    }

    try
    {
        g_win = gui->createWidgetReal<MyGUI::Window>(
            "Kenshi_WindowCX",
            0.012f, 0.30f, 0.20f, 0.16f,
            MyGUI::Align::Left | MyGUI::Align::Top,
            "Window",
            "LimbVigorWin");
    }
    catch (...) { g_win = nullptr; }

    if (!g_win)
    {
        LvErr("LimbVigor: title window create failed");
        return ts;
    }

    try { g_win->setCaption("Limb Vigor"); }
    catch (...) {}

    MyGUI::Widget* client = nullptr;
    try { client = g_win->getClientWidget(); }
    catch (...) { client = nullptr; }
    if (!client) client = g_win;

    try
    {
        g_l1 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.08f, 0.92f, 0.26f,
            MyGUI::Align::HStretch | MyGUI::Align::Top, "LimbVigorL1");
        if (g_l1) g_l1->setCaption("Limb Vigor");
    }
    catch (...) { g_l1 = nullptr; }
    try
    {
        g_l2 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.38f, 0.92f, 0.26f,
            MyGUI::Align::HStretch | MyGUI::Align::Top, "LimbVigorL2");
    }
    catch (...) { g_l2 = nullptr; }
    try
    {
        g_l3 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.68f, 0.92f, 0.26f,
            MyGUI::Align::HStretch | MyGUI::Align::Top, "LimbVigorL3");
    }
    catch (...) { g_l3 = nullptr; }

    try { g_win->setVisible(false); }
    catch (...) {}

    try
    {
        gui->eventFrameStart += MyGUI::newDelegate(OnFrame);
    }
    catch (...)
    {
        LvErr("LimbVigor: frame delegate failed — window stays hidden until I-key");
    }

    g_ready = 1;
    LvLog("LimbVigor: HUD created at title screen (KillButton path)");
    return ts;
}

void LvHudInstall()
{
    void* base = nullptr;
    HMODULE exe = GetModuleHandleA(nullptr);
    if (!exe) exe = GetModuleHandleA("kenshi_x64.exe");
    if (!exe) exe = GetModuleHandleA("kenshi_GOG_x64.exe");
    if (exe) base = (void*)exe;
    if (!base)
    {
        LvErr("LimbVigor: no exe base — HUD skipped");
        return;
    }
    void* addr = (void*)((unsigned char*)base + kRvaTitleCtor);
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(addr, (void*)Title_hook, (void**)&Title_orig))
        LvErr("LimbVigor: TitleScreen hook failed");
    else
        LvLog("LimbVigor: TitleScreen HUD");
}

void LvHudHide()
{
    g_ingame = 0;
}

void LvHudPaint(const CharSnap* snap)
{
    LvHudNote(snap);
}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    g_snap = *snap;
    g_have = 1;
    g_ingame = 1;
}

#endif
