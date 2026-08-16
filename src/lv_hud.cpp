#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_game.h"

#include <cstring>
#include <cstdio>

#if defined(LIMBVIGOR_IDE)

void LvHudInstall()
{
    LvLog("LimbVigor: no title MyGUI — Blood HUD after in-game");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { LvHudNote(snap); }

#else

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Debug.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_TextBox.h>

// KillButton-style create: Kenshi_WindowCX + Kenshi_Button1, C-string
// captions, no MyGUI events. Called from DriveTick after LvWorldInGame
// only. TitleScreen create is a hard kill on 1.0.65 + Dark UI.
//
// The fill is a sized child widget — NOT a ProgressBar skin, NOT a
// clone of Blood, NOT a widget-name walk. Dark UI remaps MainPanel /
// Blood; we pin under the typical left Blood slot once the in-game
// HUD exists.
//
// MyGUI objects need C++ try/catch (MSVC C2712 forbids __try here).
// DriveTick also SEH-wraps the caller.

static MyGUI::Window* g_win = nullptr;
static MyGUI::Button* g_label = nullptr;
static MyGUI::Widget* g_fill = nullptr;
static int g_ready = 0;
static int g_failed = 0;
static int g_tries = 0;
static unsigned g_lastTryMs = 0;
static int g_captionOk = 1;
static int g_fillOk = 1;
static char g_last[160];

static MyGUI::Widget* TryChild(MyGUI::Widget* parent, const char* skin,
    float x, float y, float w, float h, const char* name)
{
    if (!parent || !skin || !skin[0]) return nullptr;
    MyGUI::Widget* wgt = nullptr;
    try
    {
        wgt = parent->createWidgetReal<MyGUI::Widget>(
            skin, x, y, w, h, MyGUI::Align::Left | MyGUI::Align::Top, name);
    }
    catch (...) { wgt = nullptr; }
    return wgt;
}

static MyGUI::Button* TryButton(MyGUI::Widget* parent, const char* skin,
    float x, float y, float w, float h, const char* name)
{
    if (!parent || !skin || !skin[0]) return nullptr;
    MyGUI::Button* b = nullptr;
    try
    {
        b = parent->createWidgetReal<MyGUI::Button>(
            skin, x, y, w, h, MyGUI::Align::Left | MyGUI::Align::Top, name);
    }
    catch (...) { b = nullptr; }
    return b;
}

static int BuildBloodHud()
{
    MyGUI::Gui* gui = nullptr;
    try { gui = MyGUI::Gui::getInstancePtr(); }
    catch (...) { gui = nullptr; }
    if (!gui) return 0;

    // Left stack, under the typical Blood vitals slot. Do not look up
    // "Blood" / MainPanel — Dark UI remaps those names.
    MyGUI::Window* win = nullptr;
    try
    {
        win = gui->createWidgetReal<MyGUI::Window>(
            "Kenshi_WindowCX",
            0.007f, 0.186f, 0.158f, 0.070f,
            MyGUI::Align::Left | MyGUI::Align::Top,
            "Window",
            "LimbVigorBlood");
    }
    catch (...) { win = nullptr; }
    if (!win) return 0;

    try { win->setCaption("Limb Vigor"); }
    catch (...) {}

    MyGUI::Widget* client = nullptr;
    try { client = win->getClientWidget(); }
    catch (...) { client = nullptr; }
    if (!client) client = win;

    MyGUI::Button* label = TryButton(client, "Kenshi_Button1",
        0.04f, 0.08f, 0.92f, 0.40f, "LimbVigorBloodLbl");
    if (!label)
    {
        MyGUI::TextBox* tb = nullptr;
        try
        {
            tb = client->createWidgetReal<MyGUI::TextBox>(
                "Kenshi_GenericTextBox",
                0.04f, 0.08f, 0.92f, 0.40f,
                MyGUI::Align::Left | MyGUI::Align::Top,
                "LimbVigorBloodLbl");
        }
        catch (...) { tb = nullptr; }
        if (tb)
        {
            try { tb->setCaption("Hemolymph  -- / --"); }
            catch (...) {}
            // TextBox is not a Button; keep caption on the window.
            (void)tb;
        }
    }
    if (label)
    {
        try { label->setCaption("Hemolymph  -- / --"); }
        catch (...) {}
    }

    // Fill bar: sized child, not ProgressBar. Start at half width so
    // the bar is visible even before the first snap.
    MyGUI::Widget* fill = TryChild(client, "Kenshi_Button1",
        0.04f, 0.54f, 0.46f, 0.36f, "LimbVigorBloodFill");
    if (!fill)
        fill = TryChild(client, "FloatingPanelSkin",
            0.04f, 0.54f, 0.46f, 0.36f, "LimbVigorBloodFill");
    if (!fill)
        fill = TryChild(client, "Kenshi_WindowCX",
            0.04f, 0.54f, 0.46f, 0.36f, "LimbVigorBloodFill");

    try { win->setVisible(true); }
    catch (...) {}

    g_win = win;
    g_label = label;
    g_fill = fill;
    g_ready = 1;
    g_last[0] = 0;

    if (fill)
        LvLog("LimbVigor: Blood HUD created after in-game");
    else
        LvLog("LimbVigor: Blood HUD created after in-game (caption only — bar widget failed)");
    return 1;
}

static void FormatLine(const CharSnap* snap, char* out, int outsz, float* fill01)
{
    if (out && outsz > 0) out[0] = 0;
    if (fill01) *fill01 = 0.f;
    if (!snap)
    {
        if (out && outsz > 0)
            std::snprintf(out, (size_t)outsz, "Limb Vigor");
        return;
    }

    char bar1[96], bar2[96], tip[160];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &f1,
               bar2, (int)sizeof(bar2), &f2,
               tip, (int)sizeof(tip));
    if (out && outsz > 0)
    {
        if (bar1[0])
            std::snprintf(out, (size_t)outsz, "%s", bar1);
        else
            std::snprintf(out, (size_t)outsz, "Limb Vigor");
    }
    if (fill01)
    {
        float f = f1;
        if (f != f) f = 0.f;
        if (f < 0.f) f = 0.f;
        if (f > 1.f) f = 1.f;
        *fill01 = f;
    }
}

static void ApplySnap(const CharSnap* snap)
{
    if (!g_ready || !g_win) return;

    char line[160];
    float frac = 0.f;
    FormatLine(snap, line, (int)sizeof(line), &frac);

    if (g_captionOk && line[0] && std::strcmp(g_last, line) != 0)
    {
        int excepted = 0;
        if (g_label)
        {
            try { g_label->setCaption(line); }
            catch (...) { excepted = 1; }
        }
        else
        {
            try { g_win->setCaption(line); }
            catch (...) { excepted = 1; }
        }
        if (excepted)
        {
            g_captionOk = 0;
            LvLog("LimbVigor: Blood HUD setCaption excepted — leaving create-time caption");
        }
        else
            std::snprintf(g_last, sizeof(g_last), "%s", line);
    }

    if (g_fillOk && g_fill)
    {
        float w = 0.04f + 0.88f * frac;
        if (w < 0.05f) w = 0.05f;
        if (w > 0.92f) w = 0.92f;
        int excepted = 0;
        try { g_fill->setRealCoord(0.04f, 0.54f, w, 0.36f); }
        catch (...) { excepted = 1; }
        if (excepted)
        {
            g_fillOk = 0;
            LvLog("LimbVigor: Blood HUD fill resize excepted — leaving create-time bar");
        }
    }
}

void LvHudInstall()
{
    LvLog("LimbVigor: no title MyGUI — Blood HUD after in-game");
}

void LvHudEnsureAfterInGame()
{
    if (!LvCfg().enableHud || g_ready || g_failed) return;
    if (!LvWorldInGame()) return;

    if (g_tries >= 8)
    {
        g_failed = 1;
        LvErr("LimbVigor: Blood HUD create failed after in-game — I-key still live");
        return;
    }
    unsigned now = GetTickCount();
    if (g_lastTryMs && now >= g_lastTryMs && (now - g_lastTryMs) < 500u)
        return;
    g_lastTryMs = now ? now : 1;
    g_tries++;

    int ok = 0;
    try { ok = BuildBloodHud(); }
    catch (...) { ok = 0; }
    if (!ok && g_tries >= 8)
    {
        g_failed = 1;
        LvErr("LimbVigor: Blood HUD create failed after in-game — I-key still live");
    }
}

void LvHudHide() {}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    if (!g_ready) LvHudEnsureAfterInGame();
    if (g_ready) ApplySnap(snap);
}

void LvHudPaint(const CharSnap* snap)
{
    LvHudNote(snap);
}

#endif
