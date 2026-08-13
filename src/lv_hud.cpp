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

// Snapshot from the medical tick. Caption is written from getMedicalGUIData
// (already hooked, no extra MyGUI delegate).
static CharSnap g_snap;
static volatile int g_have = 0;
static volatile int g_ready = 0;

static MyGUI::Window* g_win = nullptr;
static MyGUI::Button* g_btn = nullptr;
static char g_last[256];
static int g_painted = 0;

static TitleScreen* (*Title_orig)(TitleScreen*) = nullptr;

// Exact KillButton recipe. No eventFrameStart — that delegate ran
// re-entrant during TitleScreen's own constructor and killed v1.8.7
// before the menu existed.
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
        0.10f, 0.10f, 0.16f, 0.08f,
        MyGUI::Align::Default,
        "Window",
        "LimbVigorWin");
    if (!window)
    {
        LvErr("LimbVigor: title window create failed");
        return ts;
    }
    window->setCaption("Limb Vigor");

    MyGUI::Button* button = window->getClientWidget()->createWidgetReal<MyGUI::Button>(
        "Kenshi_Button1",
        0.05f, 0.10f, 0.90f, 0.80f,
        MyGUI::Align::Default,
        "LimbVigorBtn");
    if (button)
        button->setCaption("waiting...");

    g_win = window;
    g_btn = button;
    g_ready = 1;
    LvLog("LimbVigor: HUD created at title screen (no frame delegate)");
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
    LvHudNote(snap);
    if (!g_ready || !g_btn || !snap) return;

    char bar1[96], bar2[96], tip[160];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &f1,
               bar2, (int)sizeof(bar2), &f2,
               tip, (int)sizeof(tip));

    char text[256];
    if (bar2[0])
        std::snprintf(text, sizeof(text), "%s  |  %s", bar1, bar2);
    else
        std::snprintf(text, sizeof(text), "%s", bar1);

    if (std::strcmp(g_last, text) == 0) return;
    std::snprintf(g_last, sizeof(g_last), "%s", text);

    try { g_btn->setCaption(text); }
    catch (...) { return; }

    if (!g_painted)
    {
        g_painted = 1;
        LvLogf("LimbVigor: HUD painted  %s", text);
    }
}

#endif
