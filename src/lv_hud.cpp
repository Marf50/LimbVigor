#include "lv_hud.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_msvcstr.h"
#include "lv_game.h"

#include <cstdio>
#include <cstring>
#include <string>

// C++ try/catch — this file touches MyGUI objects (MSVC C2712).
#define LV_TRY    try
#define LV_EXCEPT catch (...)

#if defined(LIMBVIGOR_IDE)

void LvHudInstall()
{
    LvLog("LimbVigor: Blood walk + unused caption (IDE — no MyGUI)");
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
#include <mygui/MyGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Widget.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>

// MyGUI / Kenshi were built with VS2010. Never pass our std::string.
static std::string& GS(GameStr* s)
{
    return *reinterpret_cast<std::string*>(s);
}

static void ReadStr(const void* raw, char* out, int outsz)
{
    out[0] = 0;
    GameStrRead(raw, out, outsz);
}

static void ReadName(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY { ReadStr(&w->getName(), out, outsz); }
    LV_EXCEPT { out[0] = 0; }
}

static void ReadType(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    LV_TRY { ReadStr(&w->getTypeName(), out, outsz); }
    LV_EXCEPT { out[0] = 0; }
}

static void ReadSkin(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w) return;
    /* Kenshi Widget has no getSkinName (v1.8.1). Texture is the stand-in. */
    LV_TRY { ReadStr(&w->_getTextureName(), out, outsz); }
    LV_EXCEPT { out[0] = 0; }
}

static int CaptionIsEmpty(MyGUI::Widget* w)
{
    if (!w) return 0;
    int empty = 0;
    LV_TRY
    {
        MyGUI::TextBox* tb = w->castType<MyGUI::TextBox>(false);
        if (!tb) return 0;
        empty = tb->getCaption().empty() ? 1 : 0;
    }
    LV_EXCEPT { empty = 0; }
    return empty;
}

static void ReadCaption(MyGUI::Widget* w, char* out, int outsz)
{
    out[0] = 0;
    if (!w || !out || outsz < 2) return;
    LV_TRY
    {
        MyGUI::TextBox* tb = w->castType<MyGUI::TextBox>(false);
        if (!tb) return;
        if (tb->getCaption().empty()) return;
        const char* src = tb->getCaption().asUTF8_c_str();
        if (!src) return;
        int n = 0;
        for (; src[n] && n < outsz - 1; ++n)
            out[n] = src[n];
        out[n] = 0;
    }
    LV_EXCEPT { out[0] = 0; }
}

static int NameAfterUnderscore(const char* name, const char* needle)
{
    if (!name || !needle || !name[0]) return 0;
    if (std::strcmp(name, needle) == 0) return 1;
    const char* us = std::strchr(name, '_');
    if (us && us[1] && std::strcmp(us + 1, needle) == 0) return 1;
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

static int IsCaptionWidget(MyGUI::Widget* w)
{
    if (!w) return 0;
    int yes = 0;
    LV_TRY
    {
        if (w->castType<MyGUI::TextBox>(false)
         || w->castType<MyGUI::Button>(false)
         || w->castType<MyGUI::EditBox>(false))
            yes = 1;
    }
    LV_EXCEPT { yes = 0; }
    return yes;
}

static int InSubtree(MyGUI::Widget* w, MyGUI::Widget* root)
{
    while (w)
    {
        if (w == root) return 1;
        MyGUI::Widget* p = nullptr;
        LV_TRY { p = w->getParent(); }
        LV_EXCEPT { p = nullptr; }
        w = p;
    }
    return 0;
}

/* RE_Kenshi FindWidget: name after the first '_'. */
static MyGUI::Widget* FindSuffix(MyGUI::Widget* w, const char* needle, int depth)
{
    if (!w || !needle || depth > 16) return nullptr;
    char name[80];
    ReadName(w, name, 80);
    if (NameAfterUnderscore(name, needle)) return w;
    size_t n = 0;
    LV_TRY { n = w->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = w->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        MyGUI::Widget* hit = FindSuffix(ch, needle, depth + 1);
        if (hit) return hit;
    }
    return nullptr;
}

static MyGUI::Widget* FindSuffixGui(MyGUI::Gui* gui, const char* needle)
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
            if (NameAfterUnderscore(name, needle)) { hit = w; break; }
            hit = FindSuffix(w, needle, 1);
            if (hit) break;
        }
    }
    LV_EXCEPT { hit = nullptr; }
    return hit;
}

static MyGUI::Widget* FindCaptionWalk(MyGUI::Widget* w, const char* caption, int depth)
{
    if (!w || !caption || depth > 16) return nullptr;
    char cap[80];
    ReadCaption(w, cap, 80);
    if (cap[0] && std::strcmp(cap, caption) == 0) return w;
    size_t n = 0;
    LV_TRY { n = w->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = w->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        MyGUI::Widget* hit = FindCaptionWalk(ch, caption, depth + 1);
        if (hit) return hit;
    }
    return nullptr;
}

static MyGUI::Widget* FindBlood(MyGUI::Gui* gui)
{
    if (!gui) return nullptr;
    static const char* kNames[] = { "Blood", "blood", "BloodBar", nullptr };
    for (int i = 0; kNames[i]; ++i)
    {
        MyGUI::Widget* w = FindSuffixGui(gui, kNames[i]);
        if (w) return w;
    }
    MyGUI::Widget* hit = nullptr;
    LV_TRY
    {
        MyGUI::EnumeratorWidgetPtr e = gui->getEnumerator();
        while (e.next())
        {
            hit = FindCaptionWalk(e.current(), "Blood", 0);
            if (hit) break;
        }
    }
    LV_EXCEPT { hit = nullptr; }
    return hit;
}

static int g_loggedBlood = 0;
static int g_loggedUnused = 0;
static int g_loggedNone = 0;
static int g_paintDead = 0;
static CharSnap g_snap;
static int g_have = 0;

static void LogBloodOnce(MyGUI::Widget* blood)
{
    if (g_loggedBlood || !blood) return;
    g_loggedBlood = 1;

    char name[80], pname[80], skin[80], typ[40];
    ReadName(blood, name, 80);
    ReadType(blood, typ, 40);
    ReadSkin(blood, skin, 80);
    MyGUI::Widget* parent = nullptr;
    LV_TRY { parent = blood->getParent(); }
    LV_EXCEPT { parent = nullptr; }
    ReadName(parent, pname, 80);

    size_t n = 0;
    LV_TRY { n = blood->getChildCount(); }
    LV_EXCEPT { n = 0; }
    LvLogf("LimbVigor: Blood HUD name='%s' parent='%s' type=%s skin/tex='%s' children=%d",
           name, pname, typ, skin, (int)n);
    for (size_t i = 0; i < n && i < 16; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = blood->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        if (!ch) continue;
        char cn[80], cc[80], ct[40];
        ReadName(ch, cn, 80);
        ReadCaption(ch, cc, 80);
        ReadType(ch, ct, 40);
        LvLogf("LimbVigor: Blood child[%d] name='%s' type=%s caption='%s'",
               (int)i, cn, ct, cc);
    }
}

static int LooksReserved(const char* name, const char* cap)
{
    if (NameAfterUnderscore(name, "Blood") || (cap && std::strcmp(cap, "Blood") == 0))
        return 1;
    if (ContainsI(name, "blood") || ContainsI(cap, "blood"))
        return 1;
    if (ContainsI(name, "progress") || ContainsI(name, "fill"))
        return 1;
    return 0;
}

static MyGUI::Widget* ScanUnused(MyGUI::Widget* root, MyGUI::Widget* blood, int depth)
{
    if (!root || !blood || depth > 8) return nullptr;
    if (root != blood && !InSubtree(root, blood) && IsCaptionWidget(root) && CaptionIsEmpty(root))
    {
        char name[80], cap[80], typ[40];
        ReadName(root, name, 80);
        ReadCaption(root, cap, 80);
        ReadType(root, typ, 40);
        if (!ContainsI(typ, "Progress") && !LooksReserved(name, cap))
            return root;
    }

    size_t n = 0;
    LV_TRY { n = root->getChildCount(); }
    LV_EXCEPT { n = 0; }
    for (size_t i = 0; i < n; ++i)
    {
        MyGUI::Widget* ch = nullptr;
        LV_TRY { ch = root->getChildAt(i); }
        LV_EXCEPT { ch = nullptr; }
        if (!ch || ch == blood) continue;
        if (InSubtree(ch, blood)) continue;
        MyGUI::Widget* hit = ScanUnused(ch, blood, depth + 1);
        if (hit) return hit;
    }
    return nullptr;
}

static MyGUI::Widget* FindUnused(MyGUI::Gui* gui, MyGUI::Widget* blood)
{
    if (!gui) return nullptr;

    /* Layout the game loader built — find only, never create. */
    MyGUI::Widget* ours = FindSuffixGui(gui, "LimbVigorText");
    if (ours) return ours;

    if (!blood) return nullptr;

    MyGUI::Widget* parent = nullptr;
    LV_TRY { parent = blood->getParent(); }
    LV_EXCEPT { parent = nullptr; }
    if (parent)
    {
        MyGUI::Widget* hit = ScanUnused(parent, blood, 0);
        if (hit) return hit;
        MyGUI::Widget* gp = nullptr;
        LV_TRY { gp = parent->getParent(); }
        LV_EXCEPT { gp = nullptr; }
        if (gp)
        {
            hit = ScanUnused(gp, blood, 0);
            if (hit) return hit;
        }
    }
    return nullptr;
}

static int SetCaptionOnly(MyGUI::Widget* w, const char* text)
{
    if (!w || !text) return 0;
    int ok = 0;
    LV_TRY
    {
        MyGUI::TextBox* tb = w->castType<MyGUI::TextBox>(false);
        if (tb)
        {
            GameStr gs;
            GameStrSet(&gs, text);
            tb->setCaption(GS(&gs));
            ok = 1;
        }
    }
    LV_EXCEPT { ok = 0; }
    return ok;
}

static void MaybeGrow(MyGUI::Widget* w)
{
    if (!w) return;
    LV_TRY
    {
        MyGUI::IntSize sz = w->getSize();
        if (sz.width > 0 && sz.width < 120)
            w->setSize(180, sz.height > 0 ? sz.height : 18);
    }
    LV_EXCEPT {}
}

static MyGUI::Gui* GuiPtr()
{
    MyGUI::Gui* gui = nullptr;
    LV_TRY { gui = MyGUI::Gui::getInstancePtr(); }
    LV_EXCEPT { gui = nullptr; }
    return gui;
}

static void WalkAndPaint(const CharSnap* snap, int hide)
{
    if (g_paintDead) return;
    if (!LvCfg().enableHud) return;

    MyGUI::Gui* gui = GuiPtr();
    if (!gui) return;

    MyGUI::Widget* blood = nullptr;
    LV_TRY { blood = FindBlood(gui); }
    LV_EXCEPT { blood = nullptr; }

    if (blood)
        LogBloodOnce(blood);

    MyGUI::Widget* unused = nullptr;
    LV_TRY { unused = FindUnused(gui, blood); }
    LV_EXCEPT { unused = nullptr; }

    if (!unused)
    {
        /* Blood found, nothing unused — layout is the fallback. Do not
         * log this before Blood exists (MainBar ctor / no selection). */
        if (blood && !g_loggedNone)
        {
            g_loggedNone = 1;
            LvLog("LimbVigor: none exists — no unused caption; LimbVigor.layout shipped (findWidget LimbVigorText)");
        }
        return;
    }

    if (!g_loggedUnused)
    {
        g_loggedUnused = 1;
        char name[80], pname[80], skin[80];
        ReadName(unused, name, 80);
        ReadSkin(unused, skin, 80);
        MyGUI::Widget* p = nullptr;
        LV_TRY { p = unused->getParent(); }
        LV_EXCEPT { p = nullptr; }
        ReadName(p, pname, 80);
        LvLogf("LimbVigor: unused caption name='%s' parent='%s' skin/tex='%s'",
               name, pname, skin);
    }

    if (hide || !snap)
    {
        SetCaptionOnly(unused, "");
        return;
    }

    const char* res = LvResourceName(snap->race);
    if (!res || !res[0]) res = "Vigor";
    const float maxv = LvCfg().maxVigor > 0.f ? LvCfg().maxVigor : 100.f;
    char line[64];
    std::snprintf(line, sizeof(line), "%s  %d / %d", res, (int)snap->vigor, (int)maxv);
    if (!SetCaptionOnly(unused, line))
    {
        g_paintDead = 1;
        LvErr("LimbVigor: setCaption failed — unused row stopped");
        return;
    }
    MaybeGrow(unused);
}

void LvHudInstall()
{
    LvLog("LimbVigor: Blood walk + unused caption — find/paint only");
}

void LvHudEnsureAfterInGame()
{
    if (!LvWorldInGame()) return;
    WalkAndPaint(g_have ? &g_snap : nullptr, !g_have);
}

void LvHudHide()
{
    WalkAndPaint(nullptr, 1);
}

void LvHudNote(const CharSnap* snap)
{
    if (!snap) return;
    g_snap = *snap;
    g_have = 1;
}

void LvHudPaint(const CharSnap* snap)
{
    if (snap) LvHudNote(snap);
    if (!LvWorldInGame()) return;
    if (!g_have)
    {
        WalkAndPaint(nullptr, 1);
        return;
    }
    WalkAndPaint(&g_snap, 0);
}

#endif
