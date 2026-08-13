#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_msvcstr.h"

#include <cstdio>
#include <cstring>

// C++ try/catch — this file constructs MyGUI objects.
#define LV_TRY    try
#define LV_EXCEPT catch (...)

#if defined(LIMBVIGOR_IDE)

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }

#else

#include <Windows.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Delegate.h>

// MyGUI / Kenshi were built with VS2010. Never pass our std::string.
static std::string& GS(GameStr* s)
{
    return *reinterpret_cast<std::string*>(s);
}

static void ReadStr(const std::string& raw, char* out, int outsz)
{
    out[0] = 0;
    GameStrRead(&raw, out, outsz);
}

static void ReadName(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY
    {
        const std::string& raw = w->getName();
        ReadStr(raw, out, outsz);
    }
    LV_EXCEPT { out[0] = 0; }
}

static int SkinForbidden(const char* skin)
{
    return skin && std::strcmp(skin, "ProgressBar") == 0;
}

static MyGUI::Widget* FindNamed(MyGUI::Gui* gui, const char* name)
{
    if (!gui || !name || !name[0]) return nullptr;
    GameStr n;
    GameStrSet(&n, name);
    MyGUI::Widget* w = nullptr;
    LV_TRY { w = gui->findWidgetT(GS(&n), false); }
    LV_EXCEPT { w = nullptr; }
    return w;
}

// Kenshi prefixes layout widgets ("5_Blood"). findWidgetT lives in the
// MyGUI DLL — do not walk getName() (VS2010 strings come back empty,
// Dark UI dumps crash). Never let findWidgetT throw (second arg false).
static MyGUI::Widget* FindPrefixed(MyGUI::Gui* gui, const char* base)
{
    if (!gui || !base) return nullptr;
    MyGUI::Widget* w = FindNamed(gui, base);
    if (w) return w;
    char buf[48];
    for (int i = 0; i <= 32; ++i)
    {
        std::snprintf(buf, sizeof(buf), "%d_%s", i, base);
        w = FindNamed(gui, buf);
        if (w) return w;
    }
    return nullptr;
}

static MyGUI::Widget* FindBlood(MyGUI::Gui* gui)
{
    static const char* kNames[] = {
        "Blood", "blood", "BloodBar", "bloodBar",
        "LifeBlood", "SelectedBlood", "MedicalBlood",
        "Hunger", "hunger", "HungerBar",
        nullptr
    };
    for (int i = 0; kNames[i]; ++i)
    {
        MyGUI::Widget* w = FindPrefixed(gui, kNames[i]);
        if (w)
        {
            char nm[80];
            ReadName(w, nm, 80);
            LvLogf("LimbVigor: HUD found '%s' via findWidgetT '%s'",
                nm[0] ? nm : kNames[i], kNames[i]);
            return w;
        }
    }
    return nullptr;
}

static MyGUI::Widget* MakeWidget(MyGUI::Widget* parent, const MyGUI::IntCoord& c,
    const char* skin, const char* name)
{
    if (!parent) return nullptr;
    static const char* kFallback[] = {
        "PanelEmpty", "Kenshi_GenericTextBox", "Kenshi_GenericTextBoxFlat",
        "Kenshi_Button1", "", nullptr
    };
    GameStr nm;
    GameStrSet(&nm, name);
    const char* trySkin[8];
    int n = 0;
    if (skin && skin[0] && !SkinForbidden(skin))
        trySkin[n++] = skin;
    for (int i = 0; kFallback[i] != nullptr && n < 7; ++i)
        trySkin[n++] = kFallback[i];
    trySkin[n] = nullptr;
    for (int i = 0; trySkin[i] != nullptr; ++i)
    {
        if (trySkin[i][0] && SkinForbidden(trySkin[i]))
            continue;
        GameStr sk;
        GameStrSet(&sk, trySkin[i]);
        MyGUI::Widget* w = nullptr;
        LV_TRY
        {
            w = parent->createWidget<MyGUI::Widget>(
                GS(&sk), c, MyGUI::Align::Default, GS(&nm));
        }
        LV_EXCEPT { w = nullptr; }
        if (w)
        {
            if (i == 0 && skin && skin[0])
                LvLogf("LimbVigor: widget '%s' skin '%s'", name, trySkin[i]);
            return w;
        }
    }
    return nullptr;
}

static MyGUI::TextBox* MakeText(MyGUI::Widget* parent, const MyGUI::IntCoord& c, const char* name)
{
    if (!parent) return nullptr;
    static const char* skins[] = {
        "Kenshi_TextboxStandardText", "Kenshi_GenericTextBoxFlat",
        "Kenshi_TextboxPaintedText", nullptr
    };
    GameStr nm;
    GameStrSet(&nm, name);
    for (int i = 0; skins[i]; ++i)
    {
        GameStr sk;
        GameStrSet(&sk, skins[i]);
        MyGUI::TextBox* t = nullptr;
        LV_TRY
        {
            t = parent->createWidget<MyGUI::TextBox>(
                GS(&sk), c, MyGUI::Align::Default, GS(&nm));
        }
        LV_EXCEPT { t = nullptr; }
        if (t) return t;
    }
    return nullptr;
}

static MyGUI::Widget*  g_host = nullptr;
static MyGUI::Widget*  g_blood = nullptr;
static MyGUI::Widget*  g_track = nullptr;
static MyGUI::Widget*  g_fill = nullptr;
static MyGUI::TextBox* g_label = nullptr;
static MyGUI::Widget*  g_track2 = nullptr;
static MyGUI::Widget*  g_fill2 = nullptr;
static MyGUI::TextBox* g_label2 = nullptr;
static MyGUI::TextBox* g_tip = nullptr;
static int             g_failed = 0;
static int             g_tries = 0;
static int             g_skip = 0;
static int             g_barW = 176;
static int             g_barH = 16;
static int             g_onLayer = 0;

static int WidgetAlive(MyGUI::Widget* w)
{
    if (!w) return 0;
    int ok = 0;
    LV_TRY { (void)w->getParent(); ok = 1; }
    LV_EXCEPT { ok = 0; }
    return ok;
}

static int HostAlive()
{
    return WidgetAlive(g_host);
}

static void ClearPtrs()
{
    g_host = nullptr;
    g_blood = nullptr;
    g_track = nullptr;
    g_fill = nullptr;
    g_label = nullptr;
    g_track2 = nullptr;
    g_fill2 = nullptr;
    g_label2 = nullptr;
    g_tip = nullptr;
    g_onLayer = 0;
}

static void ArmTooltip(MyGUI::Widget* w)
{
    if (!w) return;
    LV_TRY
    {
        w->setNeedMouseFocus(true);
        w->setNeedToolTip(true);
    }
    LV_EXCEPT {}
}

static void WriteUserTip(MyGUI::Widget* w, const char* text)
{
    if (!w || !text) return;
    GameStr key, val;
    GameStrSet(&key, "toolTip");
    GameStrSet(&val, text);
    LV_TRY { w->setUserString(GS(&key), GS(&val)); }
    LV_EXCEPT {}
    GameStrSet(&key, "ToolTip");
    LV_TRY { w->setUserString(GS(&key), GS(&val)); }
    LV_EXCEPT {}
}

// Kenshi's HUD lives on "Middle" (see Kenshi_MainPanel.layout).
// "Main" is a MyGUI default that Kenshi may not have. Try both.
static MyGUI::Widget* CreateHostOnLayer(MyGUI::Gui* gui, const MyGUI::IntCoord& c)
{
    if (!gui) return nullptr;
    static const char* kLayers[] = {
        "Middle", "Overlapped", "Main", "Popup", nullptr
    };
    static const char* kSkins[] = {
        "Kenshi_FloatingPanelSkin", "PanelEmpty",
        "Kenshi_GenericTextBox", nullptr
    };
    GameStr wname;
    GameStrSet(&wname, "LimbVigorHost");
    for (int li = 0; kLayers[li]; ++li)
    {
        GameStr layer;
        GameStrSet(&layer, kLayers[li]);
        for (int si = 0; kSkins[si]; ++si)
        {
            if (SkinForbidden(kSkins[si])) continue;
            GameStr skin;
            GameStrSet(&skin, kSkins[si]);
            MyGUI::Widget* w = nullptr;
            LV_TRY
            {
                w = gui->createWidget<MyGUI::Widget>(
                    GS(&skin), c, MyGUI::Align::Left | MyGUI::Align::Top,
                    GS(&layer), GS(&wname));
            }
            LV_EXCEPT { w = nullptr; }
            if (w)
            {
                LvLogf("LimbVigor: HUD host layer '%s' skin '%s' at %d,%d %dx%d",
                    kLayers[li], kSkins[si], c.left, c.top, c.width, c.height);
                return w;
            }
        }
    }
    return nullptr;
}

static MyGUI::Widget* CreateHostReal(MyGUI::Gui* gui)
{
    if (!gui) return nullptr;
    static const char* kLayers[] = {
        "Middle", "Overlapped", "Main", "Popup", nullptr
    };
    static const char* kSkins[] = {
        "Kenshi_FloatingPanelSkin", "PanelEmpty",
        "Kenshi_GenericTextBox", nullptr
    };
    GameStr wname;
    GameStrSet(&wname, "LimbVigorHost");
    for (int li = 0; kLayers[li]; ++li)
    {
        GameStr layer;
        GameStrSet(&layer, kLayers[li]);
        for (int si = 0; kSkins[si]; ++si)
        {
            if (SkinForbidden(kSkins[si])) continue;
            GameStr skin;
            GameStrSet(&skin, kSkins[si]);
            MyGUI::Widget* w = nullptr;
            LV_TRY
            {
                w = gui->createWidgetReal<MyGUI::Widget>(
                    GS(&skin), 0.012f, 0.34f, 0.20f, 0.14f,
                    MyGUI::Align::Left | MyGUI::Align::Top,
                    GS(&layer), GS(&wname));
            }
            LV_EXCEPT { w = nullptr; }
            if (w)
            {
                LvLogf("LimbVigor: HUD host (no Blood yet) layer '%s' skin '%s'",
                    kLayers[li], kSkins[si]);
                return w;
            }
        }
    }
    return nullptr;
}

static int BuildHud()
{
    MyGUI::Gui* gui = nullptr;
    LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui) return 0;

    MyGUI::Widget* blood = FindBlood(gui);
    g_blood = blood;
    g_onLayer = 0;

    MyGUI::IntCoord slot(10, 10, 176, 16);

    if (blood)
    {
        LV_TRY
        {
            MyGUI::IntCoord abs = blood->getAbsoluteCoord();
            if (abs.width > 20 && abs.height > 4)
            {
                slot.left = abs.left;
                slot.top = abs.top + abs.height + 3;
                slot.width = abs.width < 80 ? 176 : abs.width;
                slot.height = abs.height < 12 ? 16 : abs.height;
            }
        }
        LV_EXCEPT {}
    }

    MyGUI::IntCoord hostC(slot.left, slot.top, slot.width + 12, slot.height * 3 + 18);
    MyGUI::Widget* host = nullptr;
    if (blood)
        host = CreateHostOnLayer(gui, hostC);
    if (!host)
        host = CreateHostReal(gui);
    if (!host) return 0;

    g_host = host;
    g_onLayer = 1;
    LV_TRY { g_host->setNeedMouseFocus(false); }
    LV_EXCEPT {}

    const int pad = 6;
    MyGUI::IntCoord row(pad, pad, slot.width, slot.height);
    g_barW = slot.width;
    g_barH = slot.height;

    // Text first — numbers must show even if the fill widgets fail.
    g_label = MakeText(g_host, row, "LimbVigorTxt");
    g_track = MakeWidget(g_host, row, "Kenshi_GenericTextBox", "LimbVigorTrack");
    if (!g_label && !g_track) return 0;
    if (g_track) ArmTooltip(g_track);
    if (g_label)
    {
        ArmTooltip(g_label);
        LV_TRY { g_label->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter); }
        LV_EXCEPT {}
    }

    if (g_track)
    {
        MyGUI::IntCoord fillC(1, 1, slot.width - 2, slot.height - 2);
        g_fill = MakeWidget(g_track, fillC, "Kenshi_Button1", "LimbVigorFill");
        if (g_fill)
        {
            LV_TRY { g_fill->setColour(MyGUI::Colour(0.62f, 0.18f, 0.16f)); }
            LV_EXCEPT {}
            LV_TRY { g_fill->setNeedMouseFocus(false); }
            LV_EXCEPT {}
        }
    }

    MyGUI::IntCoord row2 = row;
    row2.top += slot.height + 4;
    g_label2 = MakeText(g_host, row2, "LimbVigorTxt2");
    g_track2 = MakeWidget(g_host, row2, "Kenshi_GenericTextBox", "LimbVigorTrack2");
    if (g_track2)
    {
        ArmTooltip(g_track2);
        MyGUI::IntCoord fillC(1, 1, slot.width - 2, slot.height - 2);
        g_fill2 = MakeWidget(g_track2, fillC, "Kenshi_Button1", "LimbVigorFill2");
        if (g_fill2)
        {
            LV_TRY { g_fill2->setColour(MyGUI::Colour(0.22f, 0.52f, 0.30f)); }
            LV_EXCEPT {}
            LV_TRY { g_fill2->setNeedMouseFocus(false); }
            LV_EXCEPT {}
        }
    }
    if (g_label2)
    {
        ArmTooltip(g_label2);
        LV_TRY { g_label2->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter); }
        LV_EXCEPT {}
    }

    MyGUI::IntCoord tipC = row2;
    tipC.top += slot.height + 4;
    tipC.height = slot.height + 6;
    tipC.width = slot.width + 4;
    g_tip = MakeText(g_host, tipC, "LimbVigorTip");
    if (g_tip)
    {
        ArmTooltip(g_tip);
        LV_TRY { g_tip->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top); }
        LV_EXCEPT {}
    }

    if (!g_label && !g_label2 && !g_tip)
    {
        LvErr("LimbVigor: HUD text widgets failed — STATS (C) and I-key still work");
        return 0;
    }

    LvLog("LimbVigor: HUD bars ready. Numbers are on the bar. Hover for the tooltip.");
    return 1;
}

static void PinToBlood()
{
    if (!g_onLayer || !g_host) return;
    if (!WidgetAlive(g_blood))
    {
        MyGUI::Gui* gui = nullptr;
        LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
        LV_EXCEPT { gui = nullptr; }
        g_blood = FindBlood(gui);
    }
    if (!WidgetAlive(g_blood)) return;
    LV_TRY
    {
        MyGUI::IntCoord abs = g_blood->getAbsoluteCoord();
        if (abs.width > 20 && abs.height > 4)
        {
            g_host->setPosition(abs.left, abs.top + abs.height + 3);
            if (!g_blood->getInheritedVisible())
                g_host->setVisible(false);
        }
    }
    LV_EXCEPT {}
}

static void SetFill(MyGUI::Widget* fill, float t)
{
    if (!fill) return;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    const int w = (int)((g_barW - 2) * t);
    LV_TRY { fill->setSize(w < 1 ? 1 : w, g_barH > 2 ? g_barH - 2 : g_barH); }
    LV_EXCEPT {}
}

static void SetCap(MyGUI::TextBox* t, const char* s)
{
    if (!t || !s) return;
    LV_TRY { t->setCaption(MyGUI::UString(s)); }
    LV_EXCEPT {}
}

void LvHudHide()
{
    LV_TRY { if (g_host) g_host->setVisible(false); }
    LV_EXCEPT {}
}

void LvHudPaint(const CharSnap* snap)
{
    if (!LvCfg().enableHud || g_failed) return;
    if (!snap || snap->race == RACE_ANIMAL)
    {
        LvHudHide();
        return;
    }

    if (!HostAlive())
    {
        ClearPtrs();
        if (++g_skip < 15) return;
        g_skip = 0;
        if (g_tries > 40)
        {
            if (!g_failed)
            {
                LvErr("LimbVigor: HUD bar gave up — STATS (C) and I-key tooltip still work");
                g_failed = 1;
            }
            return;
        }
        int ok = 0;
        LV_TRY { ok = BuildHud(); }
        LV_EXCEPT { ok = 0; }
        g_tries++;
        if (!ok) return;
        g_tries = 0;
    }

    PinToBlood();

    char bar1[96], bar2[96], tip[220];
    float f1 = 0.f, f2 = 0.f;
    LvHudLines(snap, bar1, (int)sizeof(bar1), &f1, bar2, (int)sizeof(bar2), &f2, tip, (int)sizeof(tip));

    char hover[256];
    LvItemTooltipText(snap, hover, (int)sizeof(hover));

    LV_TRY { if (g_host) g_host->setVisible(true); }
    LV_EXCEPT {}
    LV_TRY { if (g_track) g_track->setVisible(true); }
    LV_EXCEPT {}
    SetFill(g_fill, f1);
    SetCap(g_label, bar1);
    WriteUserTip(g_track, hover);
    WriteUserTip(g_label, hover);

    if (bar2[0])
    {
        LV_TRY { if (g_track2) g_track2->setVisible(true); }
        LV_EXCEPT {}
        LV_TRY { if (g_label2) g_label2->setVisible(true); }
        LV_EXCEPT {}
        LV_TRY { if (g_fill2) g_fill2->setVisible(true); }
        LV_EXCEPT {}
        SetFill(g_fill2, f2);
        SetCap(g_label2, bar2);
        WriteUserTip(g_track2, hover);
        WriteUserTip(g_label2, hover);
    }
    else
    {
        LV_TRY { if (g_track2) g_track2->setVisible(false); }
        LV_EXCEPT {}
        LV_TRY { if (g_label2) g_label2->setVisible(false); }
        LV_EXCEPT {}
        LV_TRY { if (g_fill2) g_fill2->setVisible(false); }
        LV_EXCEPT {}
    }

    if (g_tip)
    {
        LV_TRY { g_tip->setVisible(true); }
        LV_EXCEPT {}
        SetCap(g_tip, tip);
        WriteUserTip(g_tip, hover);
    }
}

static CharSnap g_snap;
static int      g_have = 0;
static int      g_frame = 0;

static void OnFrame(float)
{
    if (!g_have || !LvCfg().enableHud) return;
    LvHudPaint(&g_snap);
}

static void EnsureFrame()
{
    if (g_frame) return;
    MyGUI::Gui* gui = nullptr;
    LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui) return;
    LV_TRY
    {
        gui->eventFrameStart += MyGUI::newDelegate(OnFrame);
        g_frame = 1;
        LvLog("LimbVigor: HUD frame hook");
    }
    LV_EXCEPT { g_frame = 0; }
}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    g_snap = *snap;
    g_have = 1;
    EnsureFrame();
}

#endif
