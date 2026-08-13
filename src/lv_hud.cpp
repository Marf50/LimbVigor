#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"

#include <cstring>
#include <cstdio>

#if defined(LIMBVIGOR_IDE)

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudInstall() {}

#else

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/gui/TitleScreen.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>

// Snapshot written on the game thread, read on the UI thread.
static CharSnap g_snap;
static volatile int g_have = 0;
static volatile int g_ingame = 0;
static volatile int g_ready = 0;

static MyGUI::Window* g_win = nullptr;
static MyGUI::Button* g_btn = nullptr;
static char g_last[256];
static int g_painted = 0;

static TitleScreen* (*Title_orig)(TitleScreen*) = nullptr;

static void OnFrame(float)
{
    if (!g_win || !g_btn) return;
    if (!g_have || !g_ingame || !LvCfg().enableHud)
    {
        try { g_win->setVisible(false); }
        catch (...) {}
        return;
    }

    char bar1[96], bar2[96], tip[160];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(&g_snap, bar1, (int)sizeof(bar1), &f1,
               bar2, (int)sizeof(bar2), &f2,
               tip, (int)sizeof(tip));

    char text[256];
    if (bar2[0])
        std::snprintf(text, sizeof(text), "%s  |  %s", bar1, bar2);
    else
        std::snprintf(text, sizeof(text), "%s", bar1);

    if (std::strcmp(g_last, text) == 0)
    {
        try { g_win->setVisible(true); }
        catch (...) {}
        return;
    }
    std::snprintf(g_last, sizeof(g_last), "%s", text);

    try { g_win->setVisible(true); }
    catch (...) {}
    try { g_btn->setCaption(text); }
    catch (...) {}

    if (!g_painted)
    {
        g_painted = 1;
        LvLogf("LimbVigor: HUD painted  %s", text);
    }
}

// KillButton shape. GetRealAddress — never a raw RVA.
// One window, one button. No getClientWidget children beyond that.
static TitleScreen* Title_hook(TitleScreen* self)
{
    TitleScreen* ts = Title_orig ? Title_orig(self) : self;
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
            0.012f, 0.32f, 0.22f, 0.10f,
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
        g_btn = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1",
            0.04f, 0.10f, 0.92f, 0.80f,
            MyGUI::Align::Stretch,
            "LimbVigorBtn");
        if (g_btn) g_btn->setCaption("Limb Vigor");
    }
    catch (...) { g_btn = nullptr; }

    try { g_win->setVisible(false); }
    catch (...) {}

    try { gui->eventFrameStart += MyGUI::newDelegate(OnFrame); }
    catch (...) { LvErr("LimbVigor: frame delegate failed"); }

    g_ready = 1;
    LvLog("LimbVigor: HUD created at title screen");
    return ts;
}

void LvHudInstall()
{
    intptr_t addr = KenshiLib::GetRealAddress(&TitleScreen::_CONSTRUCTOR);
    if (!addr)
    {
        LvErr("LimbVigor: TitleScreen::_CONSTRUCTOR not in KenshiLib — HUD off");
        return;
    }
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(
            (void*)addr, (void*)Title_hook, (void**)&Title_orig))
        LvErr("LimbVigor: TitleScreen hook failed — HUD off, game continues");
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
