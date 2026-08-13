#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"

#include <cstdio>
#include <cstring>

#if defined(LIMBVIGOR_IDE)

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }

#else

#include <Windows.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>

// KillButton (official RE_Kenshi example) creates a Kenshi_WindowCX on
// layer "Window" and calls setCaption("...") with a C string. That is the
// only MyGUI pattern proven not to take the game down. v1.8.4 cloned
// skins onto Middle from a frame-start delegate; the log died the instant
// the widgets existed. No delegates, no findWidgetT, no GameStr, no
// ProgressBar, no setUserString.

static MyGUI::Window*  g_win   = nullptr;
static MyGUI::Button*  g_line1 = nullptr;
static MyGUI::Button*  g_line2 = nullptr;
static MyGUI::Button*  g_line3 = nullptr;
static int             g_failed = 0;
static int             g_tries  = 0;
static int             g_skip   = 0;
static int             g_logged = 0;

static int Alive(MyGUI::Widget* w)
{
    if (!w) return 0;
    int ok = 0;
    try { (void)w->getVisible(); ok = 1; }
    catch (...) { ok = 0; }
    return ok;
}

static void Clear()
{
    g_win = nullptr;
    g_line1 = nullptr;
    g_line2 = nullptr;
    g_line3 = nullptr;
}

static int BuildHud()
{
    MyGUI::Gui* gui = nullptr;
    try { gui = MyGUI::Gui::getInstancePtr(); }
    catch (...) { gui = nullptr; }
    if (!gui) return 0;

    MyGUI::Window* win = nullptr;
    try
    {
        win = gui->createWidgetReal<MyGUI::Window>(
            "Kenshi_WindowCX",
            0.012f, 0.30f, 0.20f, 0.16f,
            MyGUI::Align::Left | MyGUI::Align::Top,
            "Window",
            "LimbVigorWin");
    }
    catch (...) { win = nullptr; }
    if (!win) return 0;

    try { win->setCaption("Limb Vigor"); }
    catch (...) {}

    MyGUI::Widget* client = nullptr;
    try { client = win->getClientWidget(); }
    catch (...) { client = nullptr; }
    if (!client) client = win;

    try
    {
        g_line1 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.08f, 0.92f, 0.26f,
            MyGUI::Align::Top | MyGUI::Align::HStretch, "LimbVigorL1");
    }
    catch (...) { g_line1 = nullptr; }
    try
    {
        g_line2 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.38f, 0.92f, 0.26f,
            MyGUI::Align::Top | MyGUI::Align::HStretch, "LimbVigorL2");
    }
    catch (...) { g_line2 = nullptr; }
    try
    {
        g_line3 = client->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1", 0.04f, 0.68f, 0.92f, 0.26f,
            MyGUI::Align::Top | MyGUI::Align::HStretch, "LimbVigorL3");
    }
    catch (...) { g_line3 = nullptr; }

    if (g_line1)
    {
        try { g_line1->setCaption("Limb Vigor"); }
        catch (...) {}
    }
    g_win = win;
    try { g_win->setVisible(true); }
    catch (...) {}
    LvLog("LimbVigor: HUD window up (KillButton skin, layer Window)");
    return 1;
}

static void SetLine(MyGUI::Button* b, const char* s, int show)
{
    if (!b) return;
    try { b->setVisible(show ? true : false); }
    catch (...) {}
    if (!show || !s || !s[0]) return;
    try { b->setCaption(s); }
    catch (...) {}
}

void LvHudHide()
{
    try { if (g_win) g_win->setVisible(false); }
    catch (...) {}
}

void LvHudPaint(const CharSnap* snap)
{
    if (!LvCfg().enableHud || g_failed) return;
    if (!snap || snap->race == RACE_ANIMAL)
    {
        LvHudHide();
        return;
    }

    if (!Alive(g_win))
    {
        Clear();
        if (++g_skip < 8) return;
        g_skip = 0;
        if (g_tries > 20)
        {
            if (!g_failed)
            {
                LvErr("LimbVigor: HUD window gave up — STATS (C) and I-key still work");
                g_failed = 1;
            }
            return;
        }
        int ok = 0;
        try { ok = BuildHud(); }
        catch (...) { ok = 0; }
        g_tries++;
        if (!ok) return;
        g_tries = 0;
    }

    char bar1[96], bar2[96], tip[220];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &f1, bar2, (int)sizeof(bar2), &f2, tip, (int)sizeof(tip));

    try { g_win->setVisible(true); }
    catch (...) {}
    SetLine(g_line1, bar1, 1);
    SetLine(g_line2, bar2[0] ? bar2 : "", bar2[0] ? 1 : 0);
    SetLine(g_line3, tip, tip[0] ? 1 : 0);

    if (!g_logged)
    {
        g_logged = 1;
        LvLogf("LimbVigor: HUD painted  %s", bar1);
        if (bar2[0]) LvLogf("LimbVigor: HUD painted  %s", bar2);
    }
}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    LvHudPaint(snap);
}

#endif
