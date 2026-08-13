#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_msvcstr.h"

#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
#define LV_TRY    __try
#define LV_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
#define LV_TRY    if (true)
#define LV_EXCEPT if (false)
#endif

#if defined(LIMBVIGOR_IDE)

void LvHudPaint(const CharSnap* snap) { (void)snap; }
void LvHudHide() {}

#else

#include <Windows.h>
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_ProgressBar.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Delegate.h>
#include <mygui/MyGUI_LayerManager.h>

// MyGUI was built with VS2010. Never pass a real std::string —
// wrap a GameStr and let MyGUI read it as its own string.

static std::string& GS(GameStr* s)
{
    return *reinterpret_cast<std::string*>(s);
}

static void ReadName(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY
    {
        const std::string& raw = w->getName();
        GameStrRead(&raw, out, outsz);
    }
    LV_EXCEPT { out[0] = 0; }
}

static MyGUI::Widget* FindByName(MyGUI::Gui* gui, const char* name)
{
    if (!gui || !name) return nullptr;
    GameStr n;
    GameStrSet(&n, name);
    MyGUI::Widget* w = nullptr;
    LV_TRY { w = gui->findWidgetT(GS(&n), false); }
    LV_EXCEPT { w = nullptr; }
    return w;
}

static MyGUI::Widget* FindNamedWalk(MyGUI::Widget* w, const char* needle, int depth)
{
    if (!w || depth > 14 || !needle) return nullptr;
    char name[80];
    ReadName(w, name, 80);
    if (name[0])
    {
        char low[80];
        int i = 0;
        for (; name[i] && i < 79; ++i)
        {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            low[i] = c;
        }
        low[i] = 0;
        if (std::strstr(low, needle)) return w;
    }
    size_t n = 0;
    LV_TRY { n = w->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = w->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        MyGUI::Widget* hit = FindNamedWalk(ch, needle, depth + 1);
        if (hit) return hit;
    }
    return nullptr;
}

static MyGUI::Widget* FindBlood(MyGUI::Gui* gui)
{
    static const char* kNames[] = {
        "Blood", "blood", "BloodBar", "bloodBar", "blood_bar",
        "LifeBlood", "MedicalBlood", "Bar_Blood", "barBlood",
        "SelectedBlood", "bloodLevel", "BloodLevel", nullptr
    };
    for (int i = 0; kNames[i]; ++i)
    {
        MyGUI::Widget* w = FindByName(gui, kNames[i]);
        if (w) return w;
    }

    static const char* kRoots[] = {
        "Kenshi_MainPanel", "MainPanel", "Main", "HUD",
        "CharacterInfo", "SelectedCharacter", "Medical", nullptr
    };
    for (int i = 0; kRoots[i]; ++i)
    {
        MyGUI::Widget* root = FindByName(gui, kRoots[i]);
        if (!root) continue;
        MyGUI::Widget* w = FindNamedWalk(root, "blood", 0);
        if (w) return w;
    }
    return nullptr;
}

static MyGUI::ProgressBar* g_vigor = nullptr;
static MyGUI::TextBox*     g_vigorTxt = nullptr;
static MyGUI::ProgressBar* g_grow = nullptr;
static MyGUI::TextBox*     g_growTxt = nullptr;
static MyGUI::TextBox*     g_tip = nullptr;
static char                g_tipCopy[256];
static int                 g_dumped = 0;
static int                 g_failed = 0;

static void DumpWalk(MyGUI::Widget* w, int depth, int* left)
{
    if (!w || *left <= 0 || depth > 10) return;
    char name[80];
    ReadName(w, name, 80);
    char typ[48];
    typ[0] = 0;
    LV_TRY
    {
        const std::string& raw = w->getTypeName();
        GameStrRead(&raw, typ, 48);
    }
    LV_EXCEPT {}
    int x = 0, y = 0, ww = 0, hh = 0;
    LV_TRY
    {
        MyGUI::IntCoord c = w->getCoord();
        x = c.left; y = c.top; ww = c.width; hh = c.height;
    }
    LV_EXCEPT {}
    if (name[0] || (ww > 40 && hh > 6 && hh < 40))
        LvLogf("HUD widget d=%d '%s' type=%s %d,%d %dx%d", depth, name, typ, x, y, ww, hh);
    (*left)--;
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

static void DumpOnce(MyGUI::Gui* gui)
{
    if (g_dumped || !gui) return;
    g_dumped = 1;
    LvLog("LimbVigor: dumping HUD widgets (Dark UI / vanilla names)");
    int left = 180;
    static const char* kRoots[] = {
        "Kenshi_MainPanel", "MainPanel", "Main", "HUD", "Statistic", nullptr
    };
    int any = 0;
    for (int i = 0; kRoots[i]; ++i)
    {
        MyGUI::Widget* r = FindByName(gui, kRoots[i]);
        if (!r) continue;
        any = 1;
        DumpWalk(r, 0, &left);
    }
    if (!any) LvLog("LimbVigor: no MainPanel root — will use a fixed HUD slot");
}

static void OnTip(MyGUI::Widget* sender, const MyGUI::ToolTipInfo& info)
{
    (void)sender;
    if (!g_tip) return;
    if (info.type == MyGUI::ToolTipInfo::Hide)
    {
        g_tip->setVisible(false);
        return;
    }
    g_tip->setCaption(MyGUI::UString(g_tipCopy));
    g_tip->setVisible(true);
    if (info.type == MyGUI::ToolTipInfo::Show || info.type == MyGUI::ToolTipInfo::Move)
        g_tip->setPosition(info.point.left + 14, info.point.top + 16);
}

static MyGUI::ProgressBar* MakeBar(MyGUI::Widget* parent, const MyGUI::IntCoord& c, const char* name)
{
    if (!parent) return nullptr;
    static const char* skins[] = {
        "ProgressBar", "Kenshi_ProgressBar", "Kenshi_Bar", "EditProgressBar", nullptr
    };
    GameStr nm;
    GameStrSet(&nm, name);
    for (int i = 0; skins[i]; ++i)
    {
        GameStr sk;
        GameStrSet(&sk, skins[i]);
        MyGUI::ProgressBar* bar = nullptr;
        LV_TRY
        {
            bar = parent->createWidget<MyGUI::ProgressBar>(
                GS(&sk), c, MyGUI::Align::Default, GS(&nm));
        }
        LV_EXCEPT { bar = nullptr; }
        if (bar)
        {
            LvLogf("LimbVigor: HUD bar '%s' skin '%s'", name, skins[i]);
            return bar;
        }
    }
    return nullptr;
}

static MyGUI::TextBox* MakeText(MyGUI::Widget* parent, const MyGUI::IntCoord& c, const char* name)
{
    if (!parent) return nullptr;
    static const char* skins[] = {
        "Kenshi_TextboxStandardText", "Kenshi_TextboxPaintedText", "TextBox", "EditBox", nullptr
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

static int BuildHud()
{
    MyGUI::Gui* gui = nullptr;
    LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
    LV_EXCEPT { gui = nullptr; }
    if (!gui) return 0;

    DumpOnce(gui);

    MyGUI::Widget* blood = FindBlood(gui);
    MyGUI::Widget* parent = nullptr;
    MyGUI::IntCoord slot(12, 420, 168, 16);

    if (blood)
    {
        LV_TRY
        {
            parent = blood->getParent();
            MyGUI::IntCoord bc = blood->getCoord();
            slot = bc;
            slot.top = bc.top + bc.height + 3;
            if (slot.height < 12) slot.height = 14;
        }
        LV_EXCEPT { parent = nullptr; }
        LvLog("LimbVigor: found Blood — cloning a bar under it");
    }

    if (!parent)
    {
        // Dark UI / vanilla left stack. Own widget on the Main layer.
        GameStr layer, wname, skin;
        GameStrSet(&layer, "Main");
        GameStrSet(&wname, "LimbVigorHost");
        GameStrSet(&skin, "PanelEmpty");
        LV_TRY
        {
            parent = gui->createWidgetReal<MyGUI::Widget>(
                GS(&skin), 0.007f, 0.46f, 0.145f, 0.07f,
                MyGUI::Align::Left | MyGUI::Align::Top,
                GS(&layer), GS(&wname));
        }
        LV_EXCEPT { parent = nullptr; }
        if (!parent)
        {
            GameStrSet(&skin, "Kenshi_WindowCX");
            GameStrSet(&layer, "Window");
            LV_TRY
            {
                parent = gui->createWidgetReal<MyGUI::Widget>(
                    GS(&skin), 0.007f, 0.46f, 0.145f, 0.07f,
                    MyGUI::Align::Left | MyGUI::Align::Top,
                    GS(&layer), GS(&wname));
            }
            LV_EXCEPT { parent = nullptr; }
        }
        slot = MyGUI::IntCoord(2, 2, 160, 16);
        if (parent) LvLog("LimbVigor: HUD host on a fixed left slot (Blood name not found)");
    }

    if (!parent) return 0;

    g_vigor = MakeBar(parent, slot, "LimbVigorBar");
    if (!g_vigor) return 0;
    g_vigor->setProgressRange(1000);
    g_vigor->setColour(MyGUI::Colour(0.62f, 0.18f, 0.16f));
    g_vigor->setNeedToolTip(true);
    g_vigor->eventToolTip += MyGUI::newDelegate(OnTip);

    MyGUI::IntCoord tc = slot;
    g_vigorTxt = MakeText(parent, tc, "LimbVigorTxt");

    MyGUI::IntCoord grow = slot;
    grow.top += slot.height + 3;
    g_grow = MakeBar(parent, grow, "LimbVigorGrow");
    if (g_grow)
    {
        g_grow->setProgressRange(1000);
        g_grow->setColour(MyGUI::Colour(0.25f, 0.55f, 0.32f));
        g_grow->setNeedToolTip(true);
        g_grow->eventToolTip += MyGUI::newDelegate(OnTip);
        g_growTxt = MakeText(parent, grow, "LimbVigorGrowTxt");
    }

    GameStr tskin, tlayer, tname;
    GameStrSet(&tskin, "Kenshi_TextboxStandardText");
    GameStrSet(&tlayer, "Popup");
    GameStrSet(&tname, "LimbVigorTip");
    LV_TRY
    {
        g_tip = gui->createWidget<MyGUI::TextBox>(
            GS(&tskin), MyGUI::IntCoord(0, 0, 320, 72),
            MyGUI::Align::Default, GS(&tlayer), GS(&tname));
    }
    LV_EXCEPT { g_tip = nullptr; }
    if (!g_tip)
    {
        GameStrSet(&tskin, "TextBox");
        GameStrSet(&tlayer, "Window");
        LV_TRY
        {
            g_tip = gui->createWidget<MyGUI::TextBox>(
                GS(&tskin), MyGUI::IntCoord(0, 0, 320, 72),
                MyGUI::Align::Default, GS(&tlayer), GS(&tname));
        }
        LV_EXCEPT { g_tip = nullptr; }
    }
    if (g_tip)
    {
        g_tip->setVisible(false);
        g_tip->setNeedToolTip(false);
    }

    std::snprintf(g_tipCopy, sizeof(g_tipCopy), "Limb Vigor");
    return 1;
}

void LvHudHide()
{
    LV_TRY { if (g_vigor) g_vigor->setVisible(false); }
    LV_EXCEPT {}
    LV_TRY { if (g_vigorTxt) g_vigorTxt->setVisible(false); }
    LV_EXCEPT {}
    LV_TRY { if (g_grow) g_grow->setVisible(false); }
    LV_EXCEPT {}
    LV_TRY { if (g_growTxt) g_growTxt->setVisible(false); }
    LV_EXCEPT {}
    LV_TRY { if (g_tip) g_tip->setVisible(false); }
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

    if (!g_vigor)
    {
        int ok = 0;
        LV_TRY { ok = BuildHud(); }
        LV_EXCEPT { ok = 0; }
        if (!ok)
        {
            static int once = 0;
            if (!once) { LvErr("LimbVigor: could not build HUD bar"); once = 1; }
            g_failed = 1;
            return;
        }
    }

    const char* res = LvResourceName(snap->race);
    if (!res || !res[0]) res = "Vigor";
    const float fill = (LvCfg().maxVigor > 0.f) ? (snap->vigor / LvCfg().maxVigor) : 0.f;
    char line[96];
    std::snprintf(line, sizeof(line), "%s   %.0f / %.0f", res, snap->vigor, LvCfg().maxVigor);

    LV_TRY
    {
        g_vigor->setVisible(true);
        g_vigor->setProgressPosition((size_t)(fill * 1000.f));
        if (g_vigorTxt)
        {
            g_vigorTxt->setVisible(true);
            g_vigorTxt->setCaption(MyGUI::UString(line));
        }
    }
    LV_EXCEPT {}

    const int stump = LvFirstStump(snap);
    char eta[96];
    LvEtaText(snap, eta, (int)sizeof(eta));

    if (stump >= 0 && g_grow)
    {
        char why[96];
        const int ok = LvEligible(snap, why, (int)sizeof(why));
        const float p = ok ? (snap->progress[stump] / 100.f) : 0.f;
        if (ok)
            std::snprintf(line, sizeof(line), "%s  %s  %.0f%%",
                LvLimbLabel((LimbId)stump), LvStageName(snap->progress[stump]), snap->progress[stump]);
        else
            std::snprintf(line, sizeof(line), "%s  %s", LvLimbLabel((LimbId)stump), why);
        LV_TRY
        {
            g_grow->setVisible(true);
            g_grow->setProgressPosition((size_t)(p * 1000.f));
            if (g_growTxt)
            {
                g_growTxt->setVisible(true);
                g_growTxt->setCaption(MyGUI::UString(line));
            }
        }
        LV_EXCEPT {}
        std::snprintf(g_tipCopy, sizeof(g_tipCopy), "%s\n%s\n%s", res, eta, LvRaceHint(snap->race));
    }
    else
    {
        LV_TRY { if (g_grow) g_grow->setVisible(false); }
        LV_EXCEPT {}
        LV_TRY { if (g_growTxt) g_growTxt->setVisible(false); }
        LV_EXCEPT {}
        std::snprintf(g_tipCopy, sizeof(g_tipCopy), "%s\n%s", res, LvRaceHint(snap->race));
    }
}

#endif
