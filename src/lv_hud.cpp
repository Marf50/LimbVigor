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

static void ReadType(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY
    {
        const std::string& raw = w->getTypeName();
        ReadStr(raw, out, outsz);
    }
    LV_EXCEPT { out[0] = 0; }
}

static void ReadSkin(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY
    {
        const std::string& raw = w->getSkinName();
        ReadStr(raw, out, outsz);
    }
    LV_EXCEPT { out[0] = 0; }
}

static int NameMatches(const char* name, const char* needle)
{
    if (!name || !needle || !name[0]) return 0;
    if (std::strcmp(name, needle) == 0) return 1;
    const char* us = std::strchr(name, '_');
    if (us && us[1] && std::strcmp(us + 1, needle) == 0) return 1;
    const size_t ln = std::strlen(name);
    const size_t ld = std::strlen(needle);
    if (ln > ld && std::strcmp(name + (ln - ld), needle) == 0)
    {
        const char before = name[ln - ld - 1];
        if (before == '_' || before == '-' || before == ' ') return 1;
    }
    return 0;
}

static int ContainsI(const char* hay, const char* ndl)
{
    if (!hay || !ndl) return 0;
    char a[80], b[40];
    int i = 0;
    for (; hay[i] && i < 79; ++i)
    {
        char c = hay[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        a[i] = c;
    }
    a[i] = 0;
    i = 0;
    for (; ndl[i] && i < 39; ++i)
    {
        char c = ndl[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        b[i] = c;
    }
    b[i] = 0;
    return std::strstr(a, b) != nullptr ? 1 : 0;
}

static MyGUI::Widget* WalkFind(MyGUI::Widget* w, const char* needle, int depth)
{
    if (!w || depth > 16 || !needle) return nullptr;
    char name[80];
    ReadName(w, name, 80);
    if (NameMatches(name, needle)) return w;
    size_t n = 0;
    LV_TRY { n = w->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = w->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        MyGUI::Widget* hit = WalkFind(ch, needle, depth + 1);
        if (hit) return hit;
    }
    return nullptr;
}

// Kenshi prefixes layout widgets ("5_Blood"). RE_Kenshi walks the
// enumerator and matches the suffix after the first underscore.
static MyGUI::Widget* FindSuffix(MyGUI::Gui* gui, const char* needle)
{
    if (!gui || !needle) return nullptr;
    MyGUI::Widget* hit = nullptr;
    LV_TRY
    {
        MyGUI::EnumeratorWidgetPtr e = gui->getEnumerator();
        while (e.next())
        {
            MyGUI::Widget* w = e.current();
            if (!w) continue;
            char name[80];
            ReadName(w, name, 80);
            if (NameMatches(name, needle)) { hit = w; break; }
            hit = WalkFind(w, needle, 1);
            if (hit) break;
        }
    }
    LV_EXCEPT { hit = nullptr; }
    return hit;
}

static int Interesting(const char* name)
{
    return ContainsI(name, "blood")
        || ContainsI(name, "life")
        || ContainsI(name, "hunger")
        || ContainsI(name, "medic")
        || ContainsI(name, "health")
        || ContainsI(name, "bar")
        || ContainsI(name, "selected")
        || ContainsI(name, "mainpanel")
        || ContainsI(name, "timemoney");
}

static void DumpWalk(MyGUI::Widget* w, int depth, int* left)
{
    if (!w || *left <= 0 || depth > 12) return;
    char name[80];
    ReadName(w, name, 80);
    if (name[0] && Interesting(name))
    {
        char typ[40], skin[48];
        ReadType(w, typ, 40);
        ReadSkin(w, skin, 48);
        int x = 0, y = 0, ww = 0, hh = 0;
        LV_TRY
        {
            MyGUI::IntCoord c = w->getCoord();
            x = c.left; y = c.top; ww = c.width; hh = c.height;
        }
        LV_EXCEPT {}
        LvLogf("HUD '%s' type=%s skin=%s %d,%d %dx%d", name, typ, skin, x, y, ww, hh);
        (*left)--;
    }
    size_t n = 0;
    LV_TRY { n = w->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n && *left > 0; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = w->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        DumpWalk(ch, depth + 1, left);
    }
}

static int g_dumped = 0;

static void DumpOnce(MyGUI::Gui* gui)
{
    if (g_dumped || !gui) return;
    g_dumped = 1;
    LvLog("LimbVigor: HUD scan (suffix Blood / LifeBar / TimeMoneyPanel)");
    int left = 80;
    LV_TRY
    {
        MyGUI::EnumeratorWidgetPtr e = gui->getEnumerator();
        while (e.next() && left > 0)
            DumpWalk(e.current(), 0, &left);
    }
    LV_EXCEPT {}
}

static MyGUI::Widget* FindBlood(MyGUI::Gui* gui)
{
    static const char* kNames[] = {
        "Blood", "blood", "BloodBar", "bloodBar",
        "LifeBlood", "SelectedBlood", "MedicalBlood",
        nullptr
    };
    for (int i = 0; kNames[i]; ++i)
    {
        MyGUI::Widget* w = FindSuffix(gui, kNames[i]);
        if (w) return w;
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
    if (skin && skin[0]) trySkin[n++] = skin;
    for (int i = 0; kFallback[i] != nullptr && n < 7; ++i)
        trySkin[n++] = kFallback[i];
    trySkin[n] = nullptr;
    for (int i = 0; trySkin[i] != nullptr; ++i)
    {
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
static int             g_barW = 160;
static int             g_barH = 14;

static int HostAlive()
{
    if (!g_host) return 0;
    int ok = 0;
    LV_TRY { ok = g_host->getVisible() || true; (void)g_host->getParent(); ok = 1; }
    LV_EXCEPT { ok = 0; }
    return ok;
}

static void ClearPtrs()
{
    g_host = nullptr;
    g_track = nullptr;
    g_fill = nullptr;
    g_label = nullptr;
    g_track2 = nullptr;
    g_fill2 = nullptr;
    g_label2 = nullptr;
    g_tip = nullptr;
}

static int BuildHud()
{
    MyGUI::Gui* gui = nullptr;
    LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui) return 0;

    DumpOnce(gui);

    MyGUI::Widget* blood = FindBlood(gui);
    MyGUI::Widget* parent = nullptr;
    MyGUI::IntCoord slot(8, 8, 168, 14);
    char bloodSkin[48] = {};

    if (blood)
    {
        LV_TRY
        {
            parent = blood->getParent();
            MyGUI::IntCoord bc = blood->getCoord();
            slot = bc;
            slot.top = bc.top + bc.height + 2;
            if (slot.height < 12) slot.height = 14;
            if (slot.width < 80) slot.width = 160;
        }
        LV_EXCEPT { parent = nullptr; }
        ReadSkin(blood, bloodSkin, 48);
        char bn[80];
        ReadName(blood, bn, 80);
        LvLogf("LimbVigor: Blood is '%s' skin '%s' — bar goes under it", bn, bloodSkin);
    }

    if (!parent)
    {
        MyGUI::Widget* anchor = FindSuffix(gui, "TimeMoneyPanel");
        if (!anchor) anchor = FindSuffix(gui, "LifeBar1");
        if (!anchor) anchor = FindSuffix(gui, "LifeBar");
        if (anchor)
        {
            LV_TRY { parent = anchor->getParent() ? anchor->getParent() : anchor; }
            LV_EXCEPT { parent = nullptr; }
            if (parent) LvLog("LimbVigor: HUD anchored next to TimeMoney / LifeBar");
        }
    }

    if (!parent)
    {
        GameStr layer, wname, skin;
        GameStrSet(&layer, "Main");
        GameStrSet(&wname, "LimbVigorHost");
        GameStrSet(&skin, "PanelEmpty");
        LV_TRY
        {
            parent = gui->createWidgetReal<MyGUI::Widget>(
                GS(&skin), 0.008f, 0.42f, 0.16f, 0.09f,
                MyGUI::Align::Left | MyGUI::Align::Top,
                GS(&layer), GS(&wname));
        }
        LV_EXCEPT { parent = nullptr; }
        slot = MyGUI::IntCoord(2, 2, 168, 14);
        if (parent) LvLog("LimbVigor: HUD host on Main layer (Blood not found yet)");
    }

    if (!parent) return 0;

    MyGUI::IntCoord hostC = slot;
    hostC.height = slot.height * 3 + 10;
    g_host = MakeWidget(parent, hostC, "PanelEmpty", "LimbVigorHost");
    if (!g_host) return 0;
    LV_TRY { g_host->setNeedMouseFocus(false); }
    LV_EXCEPT {}

    MyGUI::IntCoord row(0, 0, slot.width, slot.height);
    g_barW = slot.width;
    g_barH = slot.height;
    g_track = MakeWidget(g_host, row, bloodSkin[0] ? bloodSkin : "Kenshi_GenericTextBox", "LimbVigorTrack");
    if (!g_track) return 0;

    MyGUI::IntCoord fillC(1, 1, slot.width - 2, slot.height - 2);
    g_fill = MakeWidget(g_track, fillC, "Kenshi_Button1", "LimbVigorFill");
    if (g_fill)
    {
        LV_TRY { g_fill->setColour(MyGUI::Colour(0.62f, 0.18f, 0.16f)); }
        LV_EXCEPT {}
    }

    g_label = MakeText(g_host, row, "LimbVigorTxt");
    if (g_label)
    {
        LV_TRY
        {
            g_label->setNeedMouseFocus(false);
            g_label->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
        }
        LV_EXCEPT {}
    }

    MyGUI::IntCoord row2 = row;
    row2.top += slot.height + 3;
    g_track2 = MakeWidget(g_host, row2, bloodSkin[0] ? bloodSkin : "Kenshi_GenericTextBox", "LimbVigorTrack2");
    if (g_track2)
    {
        g_fill2 = MakeWidget(g_track2, fillC, "Kenshi_Button1", "LimbVigorFill2");
        if (g_fill2)
        {
            LV_TRY { g_fill2->setColour(MyGUI::Colour(0.22f, 0.52f, 0.30f)); }
            LV_EXCEPT {}
        }
        g_label2 = MakeText(g_host, row2, "LimbVigorTxt2");
        if (g_label2)
        {
            LV_TRY
            {
                g_label2->setNeedMouseFocus(false);
                g_label2->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
            }
            LV_EXCEPT {}
        }
    }

    MyGUI::IntCoord tipC = row2;
    tipC.top += slot.height + 3;
    tipC.height = slot.height + 4;
    tipC.width = slot.width + 8;
    g_tip = MakeText(g_host, tipC, "LimbVigorTip");
    if (g_tip)
    {
        LV_TRY
        {
            g_tip->setNeedMouseFocus(false);
            g_tip->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
        }
        LV_EXCEPT {}
    }

    LvLog("LimbVigor: HUD bars ready (under Blood / left stack)");
    return 1;
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
        if (++g_skip < 25) return;
        g_skip = 0;
        if (g_tries > 40)
        {
            if (!g_failed)
            {
                LvErr("LimbVigor: HUD bar gave up — STATS (C) still works");
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

    const char* res = LvResourceName(snap->race);
    if (!res || !res[0]) res = "Vigor";
    const float fill = (LvCfg().maxVigor > 0.f) ? (snap->vigor / LvCfg().maxVigor) : 0.f;
    char line[96];
    std::snprintf(line, sizeof(line), "%s  %.0f / %.0f", res, snap->vigor, LvCfg().maxVigor);

    LV_TRY { if (g_host) g_host->setVisible(true); }
    LV_EXCEPT {}
    LV_TRY { if (g_track) g_track->setVisible(true); }
    LV_EXCEPT {}
    SetFill(g_fill, fill);
    SetCap(g_label, line);

    const int stump = LvFirstStump(snap);
    char eta[96];
    LvEtaText(snap, eta, (int)sizeof(eta));

    if (stump >= 0)
    {
        char why[96];
        const int ok = LvEligible(snap, why, (int)sizeof(why));
        const float p = ok ? (snap->progress[stump] / 100.f) : 0.f;
        if (ok)
            std::snprintf(line, sizeof(line), "%s  %s  %.0f%%",
                LvLimbLabel((LimbId)stump), LvStageName(snap->progress[stump]), snap->progress[stump]);
        else
            std::snprintf(line, sizeof(line), "%s  %s", LvLimbLabel((LimbId)stump), why);
        LV_TRY { if (g_track2) g_track2->setVisible(true); }
        LV_EXCEPT {}
        LV_TRY { if (g_label2) g_label2->setVisible(true); }
        LV_EXCEPT {}
        LV_TRY { if (g_fill2) g_fill2->setVisible(true); }
        LV_EXCEPT {}
        SetFill(g_fill2, p);
        SetCap(g_label2, line);
        char tip[192];
        std::snprintf(tip, sizeof(tip), "%s   %s", eta, LvRaceHint(snap->race));
        if (g_tip)
        {
            LV_TRY { g_tip->setVisible(true); }
            LV_EXCEPT {}
            SetCap(g_tip, tip);
        }
    }
    else
    {
        LV_TRY { if (g_track2) g_track2->setVisible(false); }
        LV_EXCEPT {}
        LV_TRY { if (g_label2) g_label2->setVisible(false); }
        LV_EXCEPT {}
        LV_TRY { if (g_fill2) g_fill2->setVisible(false); }
        LV_EXCEPT {}
        if (g_tip)
        {
            LV_TRY { g_tip->setVisible(true); }
            LV_EXCEPT {}
            SetCap(g_tip, LvRaceHint(snap->race));
        }
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

