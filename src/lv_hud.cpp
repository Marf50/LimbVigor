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
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Button.h>

// Snapshot is written on the game thread. We never write MyGUI after
// TitleScreen construction — newDelegate / setCaption after create
// has killed 1.8.5–1.8.9 (frame callback, then mouse events).
static CharSnap g_snap;
static volatile int g_have = 0;
static volatile int g_ready = 0;

static MyGUI::Window* g_win = nullptr;
static MyGUI::Button* g_btn = nullptr;

static TitleScreen* (*Title_orig)(TitleScreen*) = nullptr;

// Exact v1.8.8 create path (that reached the menu and the save).
// No eventFrameStart. No eventMouse*. Caption is static.
static TitleScreen* Title_hook(TitleScreen* self)
{
    TitleScreen* ts = Title_orig ? Title_orig(self) : self;
    if (g_ready) return ts;
    if (!LvCfg().enableHud) return ts;

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (!gui)
    {
        LvErr("LimbVigor: no MyGUI at title screen");
        return ts;
    }

    MyGUI::Window* window = gui->createWidgetReal<MyGUI::Window>(
        "Kenshi_WindowCX",
        0.10f, 0.10f, 0.18f, 0.08f,
        MyGUI::Align::Default,
        "Window",
        "LimbVigorWin");
    if (!window)
    {
        LvErr("LimbVigor: title window create failed");
        return ts;
    }
    window->setCaption("Limb Vigor");

    MyGUI::Widget* client = window->getClientWidget();
    if (!client)
    {
        LvErr("LimbVigor: title window has no client widget — static caption only");
        g_win = window;
        g_btn = nullptr;
        g_ready = 1;
        return ts;
    }

    MyGUI::Button* button = client->createWidgetReal<MyGUI::Button>(
        "Kenshi_Button1",
        0.04f, 0.10f, 0.92f, 0.80f,
        MyGUI::Align::Default,
        "LimbVigorBtn");
    if (button)
        button->setCaption("I-key the LV part");

    g_win = window;
    g_btn = button;
    g_ready = 1;
    LvLog("LimbVigor: HUD created at title screen (static, no events)");
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

void LvHudHide() {}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    g_snap = *snap;
    g_have = 1;
}

void LvHudPaint(const CharSnap* snap)
{
    // Game thread — snapshot only. Never touch MyGUI here.
    LvHudNote(snap);
}

#endif
