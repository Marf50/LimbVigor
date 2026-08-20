#include "lv_hud.h"

#include "lv_game.h"
#include "lv_config.h"
#include "lv_sim.h"
#include "lv_msvcstr.h"

#if defined(LIMBVIGOR_IDE)
#include "stubs/kenshi_ide_stubs.h"
#else
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdlib>

#if defined(_MSC_VER)
#define LV_TRY    __try
#define LV_EXCEPT __except (1)
#else
#define LV_TRY    if (true)
#define LV_EXCEPT if (false)
#endif

/* HUD module. Owns resolve, paint, lifetime, and the MyGUI export bind/dump.
 * lv_game.cpp is world / medical / stump only.
 *
 * H1: never keep MyGUI pointers across ticks. Each after-orig paint
 * findWidgetT HemolymphBar* THIS tick (throw=false, prefixed). Write only
 * those returns. Forget yesterday without calling getName/getVisible/set*.
 * If this tick's find is null, skip writes. Options name-guess is dead.
 *
 * H2: layout must not name anything LifeBar10*. Kenshi assignWidget-s that
 * slot; Dark UI leaves it null and ESC lives. HemolymphBar is ours.
 */

static void* g_hookReject = nullptr; /* Goal/State DatapanelGUI* — never paint */
static void* g_capWidget = nullptr;  /* HemolymphBarDatapanel this tick */
static void* g_wHemo = nullptr;
static void* g_wHemoData = nullptr;
static void* g_wHemoValue = nullptr;
static void* g_wHemoGreen = nullptr;
static void* g_wHemoGrey = nullptr;
static void* g_wHemoRed = nullptr;
static void* g_wHemoYellow = nullptr;
static void* g_wHemoWhite = nullptr;
static void* g_wHemoRobot = nullptr;
static void* g_wHemoCrushed = nullptr;
static void* g_wRoot = nullptr;
static void* g_wMedicalPanel = nullptr;
static void* g_wBack = nullptr;
static void* g_wFront = nullptr;
static void* g_wLifeBar9 = nullptr; /* width fallback only — never setCaption / setVisible */
static void* g_wLifeBar9Data = nullptr; /* pixel size source for HemolymphBarDatapanel */
static void* g_wHemoTip = nullptr;
static char  g_prefix[96] = {};
static int   g_prefixLogged = 0;
static int   g_hudSkipTick = 0;   /* skip MyGUI writes this tick after invalidate */
static int   g_hudInvLogged = 0;  /* once per death; reset on successful resolve */
static int   g_paintLogged = 0;
static int   g_paintedOnce = 0;
static int   g_resolveOnce = 0;
static int   g_barProven = 0;

static void lvHudNullAll(void)
{
    g_wHemo = nullptr;
    g_wHemoData = nullptr;
    g_wHemoValue = nullptr;
    g_wHemoGreen = nullptr;
    g_wHemoGrey = nullptr;
    g_wHemoRed = nullptr;
    g_wHemoYellow = nullptr;
    g_wHemoWhite = nullptr;
    g_wHemoRobot = nullptr;
    g_wHemoCrushed = nullptr;
    g_wHemoTip = nullptr;
    g_wLifeBar9 = nullptr;
    g_wLifeBar9Data = nullptr;
    g_wRoot = nullptr;
    g_wMedicalPanel = nullptr;
    g_wFront = nullptr;
    g_wBack = nullptr;
    g_capWidget = nullptr;
    g_resolveOnce = 0;
    g_prefix[0] = 0;
    g_prefixLogged = 0;
}

/* Forget without touching. Log once when a prior tick had a host and this
 * tick's find did not. Never getName a stale pointer. */
static void lvHudInvalidate(void)
{
    lvHudNullAll();
    g_hudSkipTick = 1;
    if (!g_hudInvLogged)
    {
        g_hudInvLogged = 1;
        LvLog("LimbVigor: HUD invalidate (widget died)");
    }
}

static int lvHudMyGuiOk(void)
{
    return g_hudSkipTick ? 0 : 1;
}

static void lvHudSehHit(void* w)
{
    (void)w;
    lvHudInvalidate();
}

void LvHudCacheDrop(const char* why)
{
    /* No epoch-drop. Pointers are forgotten at the start of each paint. */
    (void)why;
}

void LvNoteHudProbeSeh()
{
    /* Find/write SEH already skipped. Do not probe yesterday's pointer. */
}

int LvHudWritesOk(void)
{
    return g_hudSkipTick ? 0 : 1;
}

void LvHudResumeWrites(void)
{
}

void LvHudWatchGui(void)
{
    /* v1.33: no epoch-drop. Do not hook ESC / pause / options. */
}

int LvHudCacheAlive(void)
{
    return (g_hudSkipTick || !g_wHemo) ? 0 : 1;
}

void LvHudTickBegin(void)
{
    g_hudSkipTick = 0;
}

static char  g_capOrig[96] = {};
static char  g_origData[96] = {};
static char  g_origValue[96] = {};
static int   g_capOrigHave = 0;
static int   g_capRank = 0;
static int   g_wroteCaption = 0;
static int   g_wroteValue = 0;

#if !defined(LIMBVIGOR_IDE)
/* Prefixed findWidgetT HemolymphBar* THIS tick. Root child after MedicalPanel.
 * After orig: setVisible HemolymphBar / Datapanel / Value / Green (name-gated).
 * ISub on HemolymphBarDatapanel ONLY. Value is the NUMBER only.
 * Green uses a real int getWidth / getCoord (50–400px). Pointers are not widths.
 * Never 1–9 Green. No HemolymphStrip. No Widget::setCaption hunt. */

typedef void*         (*FnGuiInst)();
typedef void*         (*FnFindW)(void* gui, const GameStr* name, unsigned char throwFlag);
typedef void*         (*FnFindW3)(void* gui, const GameStr* name, const GameStr* prefix, unsigned char throwFlag);
typedef void*         (*FnWFind1)(void* self, const GameStr* name);
typedef void*         (*FnWFind2)(void* self, const GameStr* name, unsigned char throwFlag);
typedef const void*   (*FnGetName)(void* self);
typedef unsigned char (*FnVisible)(void* self);
typedef const void*   (*FnCaption)(void* self);
typedef void          (*FnSetCaption)(void* self, const void* ustr);
typedef void*         (*FnGetSubText)(void* self);
typedef void          (*FnSetVisible)(void* self, unsigned char vis);
typedef void          (*FnSetSizeHH)(void* self, int w, int h);
typedef void          (*FnSetCoordHHHH)(void* self, int l, int t, int w, int h);
typedef int           (*FnGetInt)(void* self);
typedef void*         (*FnGetParent)(void* self);
typedef void          (*FnGetCoordSret)(void* out, void* self);
typedef unsigned long long (*FnGetSizeU64)(void* self);
typedef const int*    (*FnGetCoordRef)(void* self);
typedef void          (*FnSetDepth)(void* self, int depth);

struct LvIntCoord { int left, top, width, height; };
typedef void          (*FnSetCapStr)(void* self, const GameStr* s);
typedef void          (*FnUStrCtorS)(void* self, const GameStr* s);
typedef void          (*FnUStrCtorC)(void* self, const char* s);
typedef void          (*FnUStrDtor)(void* self);

static FnGuiInst  g_guiInst  = nullptr;
static FnFindW    g_findW    = nullptr;
static FnFindW3   g_findW3   = nullptr;
static FnGuiInst  g_wmInst   = nullptr;
static FnFindW    g_wmFind   = nullptr;
static FnFindW3   g_wmFind3  = nullptr;
static FnWFind1   g_wFind1   = nullptr;
static FnWFind2   g_wFind2   = nullptr;
static FnGetName  g_wName    = nullptr;
static FnVisible  g_wVis     = nullptr;
static FnCaption  g_wCaption = nullptr;
static FnSetCaption g_setCapTextBox = nullptr;    /* TextBox::setCaption — HemolymphValue NUMBER only */
static FnSetCaption g_setCapISub = nullptr;       /* ISubWidgetText::setCaption — Datapanel label */
static FnSetCaption g_setCapEdit = nullptr;       /* EditText::setCaption — text child */
static FnGetSubText g_getSubText = nullptr;       /* Widget::getSubWidgetText — Datapanel text child */
static FnSetVisible g_setVis = nullptr;           /* Widget::setVisible — four HemolymphBar* only */
static FnSetSizeHH     g_setSizeHH = nullptr;     /* Widget::setSize — HemolymphBarGreen only */
static FnSetCoordHHHH  g_setCoordHHHH = nullptr;  /* Widget::setCoord — HemolymphBarGreen only */
static FnGetInt        g_getWidth = nullptr;
static FnGetInt        g_getHeight = nullptr;
static FnGetInt        g_getLeft = nullptr;
static FnGetInt        g_getTop = nullptr;
static FnGetParent     g_getParent = nullptr;
static FnGetCoordSret  g_getCoord = nullptr;
static FnGetCoordSret  g_getAbsCoord = nullptr;
static FnGetCoordRef   g_getCoordRef = nullptr;
static FnGetSizeU64    g_getSizeU64 = nullptr;
static FnGetSizeU64    g_getParentSize = nullptr;
static FnCaption       g_getCapISub = nullptr;
static FnSetDepth      g_setDepth = nullptr;
static FnSetCapStr  g_setCapStr  = nullptr;
static FnUStrCtorS  g_ustrCtorS  = nullptr;
static FnUStrCtorC  g_ustrCtorC  = nullptr;
static FnUStrDtor   g_ustrDtor   = nullptr;
static int        g_setCapISubNonVirt = 0;
static int        g_setCapEditNonVirt = 0;
static void*      g_myguiLo = nullptr;
static void*      g_myguiHi = nullptr;
static int        g_exportDumped = 0;
static int        g_lifeBarWhy = 0;
static const char* g_findSym = nullptr;
static const char* g_find3Sym = nullptr;

static const char kGuiGetInstanceExact[] =
    "?getInstancePtr@?$Singleton@VGui@MyGUI@@@MyGUI@@SAPEAVGui@2@XZ";
static const char kFindWidgetTExact[] =
    "?findWidgetT@Gui@MyGUI@@QEAAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z";
static const char kWmGetInstanceExact[] =
    "?getInstancePtr@?$Singleton@VWidgetManager@MyGUI@@@MyGUI@@SAPEAVWidgetManager@2@XZ";

static int lvHasI(const char* hay, const char* needle)
{
    if (!hay || !needle || !needle[0])
        return 0;
    for (const char* h = hay; *h; ++h)
    {
        const char* a = h;
        const char* b = needle;
        while (*a && *b)
        {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (!*b) return 1;
    }
    return 0;
}

static int lvMangledStringArgs(const char* n)
{
    int c = 0;
    if (!n) return 0;
    for (const char* p = n; *p; ++p)
    {
        if (std::strncmp(p, "basic_string", 12) == 0)
        {
            c++;
            p += 11;
        }
    }
    return c;
}

static int lvIsOtherMyGuiSingleton(const char* n)
{
    return (lvHasI(n, "Clipboard") || lvHasI(n, "InputManager")
         || lvHasI(n, "PointerManager") || lvHasI(n, "FontManager")
         || lvHasI(n, "SkinManager") || lvHasI(n, "LanguageManager")
         || lvHasI(n, "ResourceManager") || lvHasI(n, "FactoryManager")
         || lvHasI(n, "PluginManager") || lvHasI(n, "DynLibManager")
         || lvHasI(n, "ControllerManager") || lvHasI(n, "LayoutManager")
         || lvHasI(n, "RenderManager") || lvHasI(n, "ToolTipManager")
         || lvHasI(n, "SubWidgetManager") || lvHasI(n, "LayerManager")
         || lvHasI(n, "WidgetManager") || lvHasI(n, "DataManager")
         || lvHasI(n, "LogManager")) ? 1 : 0;
}

static int lvIsGuiSingletonGetInstance(const char* n)
{
    if (!n || !lvHasI(n, "getInstancePtr"))
        return 0;
    if (lvIsOtherMyGuiSingleton(n))
        return 0;
    if (lvHasI(n, "Singleton@VGui@MyGUI") || lvHasI(n, "$Singleton@VGui@"))
        return 1;
    if (std::strstr(n, "VGui@MyGUI") || std::strstr(n, "@VGui@"))
        return 1;
    return 0;
}

static int lvIsGuiFindWidgetT2(const char* n)
{
    if (!n || lvHasI(n, "createWidget") || lvHasI(n, "destroyWidget"))
        return 0;
    if (!lvHasI(n, "findWidget"))
        return 0;
    if (lvHasI(n, "WidgetManager"))
        return 0;
    if (!lvHasI(n, "@Gui@MyGUI") && !std::strstr(n, "VGui@MyGUI"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return lvMangledStringArgs(n) == 1 ? 1 : 0;
}

static int lvIsWmFindById2(const char* n)
{
    if (!n || !lvHasI(n, "findById") || !lvHasI(n, "WidgetManager"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return lvMangledStringArgs(n) == 1 ? 1 : 0;
}

static int lvIsGuiFindWidgetT3(const char* n)
{
    if (!n || lvHasI(n, "createWidget") || lvHasI(n, "destroyWidget"))
        return 0;
    if (!lvHasI(n, "findWidgetT") && !lvHasI(n, "findWidget"))
        return 0;
    if (lvHasI(n, "WidgetManager"))
        return 0;
    if (!lvHasI(n, "@Gui@MyGUI") && !std::strstr(n, "VGui@MyGUI"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return lvMangledStringArgs(n) == 2 ? 1 : 0;
}

static int lvIsWmFindWidgetT(const char* n)
{
    if (!n || !lvHasI(n, "findWidget") || !lvHasI(n, "WidgetManager"))
        return 0;
    if (lvHasI(n, "createWidget"))
        return 0;
    if (!lvHasI(n, "_N"))
        return 0;
    return 1;
}

static int lvIsWidgetFindWidget(const char* n)
{
    if (!n || lvHasI(n, "createWidget") || lvHasI(n, "destroyWidget"))
        return 0;
    if (!lvHasI(n, "findWidget") || lvHasI(n, "findWidgetT"))
        return 0;
    if (lvHasI(n, "WidgetManager") || lvHasI(n, "@Gui@MyGUI"))
        return 0;
    if (!lvHasI(n, "Widget@MyGUI") && !lvHasI(n, "@Widget@"))
        return 0;
    return 1;
}

static void lvTryBindExport(const char* n, void* addr)
{
    if (!n || !addr)
        return;
    if (!g_guiInst && lvIsGuiSingletonGetInstance(n))
    {
        g_guiInst = (FnGuiInst)addr;
        LvLogf("LimbVigor: mygui bind getInstancePtr '%s'", n);
    }
    if (!g_findW && lvIsGuiFindWidgetT2(n))
    {
        g_findW = (FnFindW)addr;
        g_findSym = n;
        LvLogf("LimbVigor: mygui bind findWidgetT 2-arg '%s'", n);
    }
    if (!g_findW3 && lvIsGuiFindWidgetT3(n))
    {
        g_findW3 = (FnFindW3)addr;
        g_find3Sym = n;
        LvLogf("LimbVigor: mygui bind findWidgetT 3-arg '%s'", n);
    }
    if (!g_wmInst && lvHasI(n, "getInstancePtr") && lvHasI(n, "WidgetManager")
     && lvHasI(n, "Singleton"))
    {
        g_wmInst = (FnGuiInst)addr;
        LvLogf("LimbVigor: mygui bind WidgetManager getInstancePtr '%s'", n);
    }
    if (!g_wmFind && lvIsWmFindById2(n))
    {
        g_wmFind = (FnFindW)addr;
        if (!g_findSym) g_findSym = n;
        LvLogf("LimbVigor: mygui bind WidgetManager findById '%s'", n);
    }
    if (lvIsWmFindWidgetT(n))
    {
        if (lvMangledStringArgs(n) == 2 && !g_wmFind3)
        {
            g_wmFind3 = (FnFindW3)addr;
            LvLogf("LimbVigor: mygui bind WidgetManager findWidgetT 3-arg '%s'", n);
        }
        else if (lvMangledStringArgs(n) == 1 && !g_wmFind)
        {
            g_wmFind = (FnFindW)addr;
            LvLogf("LimbVigor: mygui bind WidgetManager findWidgetT '%s'", n);
        }
    }
    if (lvIsWidgetFindWidget(n))
    {
        if (lvHasI(n, "_N") && !g_wFind2)
        {
            g_wFind2 = (FnWFind2)addr;
            LvLogf("LimbVigor: mygui bind Widget::findWidget 2-arg '%s'", n);
        }
        else if (!lvHasI(n, "_N") && !g_wFind1)
        {
            g_wFind1 = (FnWFind1)addr;
            LvLogf("LimbVigor: mygui bind Widget::findWidget '%s'", n);
        }
    }
    if (!g_wName && lvHasI(n, "getName") && lvHasI(n, "Widget")
     && !lvHasI(n, "getNameAt") && !lvHasI(n, "getNameBy"))
    {
        g_wName = (FnGetName)addr;
        LvLogf("LimbVigor: mygui bind getName '%s'", n);
    }
    /* getVisible. setVisible is bound exactly below — never via a parent name. */
    if (!g_wVis && lvHasI(n, "getVisible") && lvHasI(n, "Widget")
     && !lvHasI(n, "Inherited") && !lvHasI(n, "setVisible"))
    {
        g_wVis = (FnVisible)addr;
        LvLogf("LimbVigor: mygui bind getVisible '%s'", n);
    }
    if (!g_setVis && lvHasI(n, "setVisible") && !lvHasI(n, "getVisible")
     && lvHasI(n, "Widget") && lvHasI(n, "MyGUI") && !lvHasI(n, "Inherited"))
    {
        g_setVis = (FnSetVisible)addr;
        LvLogf("LimbVigor: mygui bind Widget::setVisible '%s'", n);
    }
    if (!g_setSizeHH && lvHasI(n, "setSize") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "IntSize") && (lvHasI(n, "QEAAXHH") || lvHasI(n, "UEAAXHH")))
    {
        g_setSizeHH = (FnSetSizeHH)addr;
        LvLogf("LimbVigor: mygui bind Widget::setSize '%s'", n);
    }
    if (!g_setCoordHHHH && lvHasI(n, "setCoord") && lvHasI(n, "Widget@MyGUI")
     && (lvHasI(n, "QEAAXHHHH") || lvHasI(n, "UEAAXHHHH")))
    {
        g_setCoordHHHH = (FnSetCoordHHHH)addr;
        LvLogf("LimbVigor: mygui bind Widget::setCoord '%s'", n);
    }
    if (!g_getWidth && lvHasI(n, "getWidth") && lvHasI(n, "MyGUI")
     && (lvHasI(n, "Widget@MyGUI") || lvHasI(n, "ICroppedRectangle"))
     && !lvHasI(n, "setWidth") && (lvHasI(n, "QEBAH") || lvHasI(n, "UEBAH")
         || lvHasI(n, "QEAAH") || lvHasI(n, "UEAAH")))
    {
        g_getWidth = (FnGetInt)addr;
        LvLogf("LimbVigor: mygui bind getWidth '%s'", n);
    }
    if (!g_getHeight && lvHasI(n, "getHeight") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "setHeight"))
    {
        g_getHeight = (FnGetInt)addr;
        LvLogf("LimbVigor: mygui bind Widget::getHeight '%s'", n);
    }
    if (!g_getLeft && lvHasI(n, "getLeft") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "setLeft"))
    {
        g_getLeft = (FnGetInt)addr;
        LvLogf("LimbVigor: mygui bind Widget::getLeft '%s'", n);
    }
    if (!g_getTop && lvHasI(n, "getTop") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "setTop") && !lvHasI(n, "getTopLevel"))
    {
        g_getTop = (FnGetInt)addr;
        LvLogf("LimbVigor: mygui bind Widget::getTop '%s'", n);
    }
    if (!g_getParent && lvHasI(n, "getParent") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "getParentSize") && !lvHasI(n, "setParent"))
    {
        g_getParent = (FnGetParent)addr;
        LvLogf("LimbVigor: mygui bind Widget::getParent '%s'", n);
    }
    if (!g_getCoord && lvHasI(n, "getCoord") && lvHasI(n, "MyGUI")
     && !lvHasI(n, "setCoord") && !lvHasI(n, "Absolute") && !lvHasI(n, "Client")
     && lvHasI(n, "IntCoord") && (lvHasI(n, "QEBA?AU") || lvHasI(n, "UEBA?AU")))
    {
        g_getCoord = (FnGetCoordSret)addr;
        LvLogf("LimbVigor: mygui bind getCoord sret '%s'", n);
    }
    if (!g_getCoordRef && lvHasI(n, "getCoord") && lvHasI(n, "MyGUI")
     && !lvHasI(n, "setCoord") && !lvHasI(n, "Absolute")
     && (lvHasI(n, "AEBUIntCoord") || lvHasI(n, "AEBVIntCoord")))
    {
        g_getCoordRef = (FnGetCoordRef)addr;
        LvLogf("LimbVigor: mygui bind getCoord ref '%s'", n);
    }
    if (!g_getAbsCoord && lvHasI(n, "getAbsoluteCoord") && lvHasI(n, "MyGUI")
     && lvHasI(n, "IntCoord"))
    {
        g_getAbsCoord = (FnGetCoordSret)addr;
        LvLogf("LimbVigor: mygui bind getAbsoluteCoord '%s'", n);
    }
    if (!g_getSizeU64 && lvHasI(n, "getSize") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "setSize") && !lvHasI(n, "Parent") && lvHasI(n, "IntSize"))
    {
        g_getSizeU64 = (FnGetSizeU64)addr;
        LvLogf("LimbVigor: mygui bind Widget::getSize '%s'", n);
    }
    if (!g_getParentSize && lvHasI(n, "getParentSize") && lvHasI(n, "Widget@MyGUI"))
    {
        g_getParentSize = (FnGetSizeU64)addr;
        LvLogf("LimbVigor: mygui bind Widget::getParentSize '%s'", n);
    }
    if (!g_setDepth && lvHasI(n, "setDepth") && lvHasI(n, "Widget@MyGUI")
     && !lvHasI(n, "getDepth"))
    {
        g_setDepth = (FnSetDepth)addr;
        LvLogf("LimbVigor: mygui bind Widget::setDepth '%s'", n);
    }
    if (!g_getCapISub && lvHasI(n, "getCaption") && lvHasI(n, "ISubWidgetText")
     && !lvHasI(n, "setCaption"))
    {
        g_getCapISub = (FnCaption)addr;
        LvLogf("LimbVigor: mygui bind ISubWidgetText::getCaption '%s'", n);
    }
    if (!g_wCaption && lvHasI(n, "getCaption") && lvHasI(n, "Widget")
     && !lvHasI(n, "setCaption") && !lvHasI(n, "ISubWidgetText"))
    {
        g_wCaption = (FnCaption)addr;
        LvLogf("LimbVigor: mygui bind getCaption '%s'", n);
    }
    if (!g_getSubText && lvHasI(n, "getSubWidgetText") && lvHasI(n, "Widget")
     && lvHasI(n, "MyGUI") && !lvHasI(n, "setCaption"))
    {
        g_getSubText = (FnGetSubText)addr;
        LvLogf("LimbVigor: mygui bind Widget::getSubWidgetText '%s'", n);
    }
    if (lvHasI(n, "setCaptionWithReplacing") && lvHasI(n, "TextBox")
     && !lvHasI(n, "createWidget") && !g_setCapStr)
    {
        g_setCapStr = (FnSetCapStr)addr;
        LvLogf("LimbVigor: mygui bind setCaptionWithReplacing '%s'", n);
    }
    if (lvHasI(n, "setCaption") && !lvHasI(n, "getCaption")
     && !lvHasI(n, "Replacing") && !lvHasI(n, "createWidget"))
    {
        const int isText = (lvHasI(n, "TextBox@MyGUI") || lvHasI(n, "TextBox"))
                        && !lvHasI(n, "ISubWidgetText") && !lvHasI(n, "EditText");
        const int isISub = lvHasI(n, "ISubWidgetText") ? 1 : 0;
        const int isEdit = lvHasI(n, "EditText") ? 1 : 0;
        const int isNonVirt = lvHasI(n, "QEAAX") ? 1 : 0;
        /* Widget::setCaption is not in MyGUIEngine exports. Do not hunt it. */
        if (isISub)
        {
            if (!g_setCapISub || (isNonVirt && !g_setCapISubNonVirt))
            {
                g_setCapISub = (FnSetCaption)addr;
                g_setCapISubNonVirt = isNonVirt;
                LvLogf("LimbVigor: mygui bind ISubWidgetText::setCaption '%s'", n);
            }
        }
        else if (isEdit)
        {
            if (!g_setCapEdit || (isNonVirt && !g_setCapEditNonVirt))
            {
                g_setCapEdit = (FnSetCaption)addr;
                g_setCapEditNonVirt = isNonVirt;
                LvLogf("LimbVigor: mygui bind EditText::setCaption '%s'", n);
            }
        }
        else if (isText && !g_setCapTextBox)
        {
            g_setCapTextBox = (FnSetCaption)addr;
            LvLogf("LimbVigor: mygui bind TextBox::setCaption (HemolymphValue only) '%s'", n);
        }
    }
    if (!g_ustrCtorC && std::strncmp(n, "??0UString@MyGUI", 16) == 0
     && (lvHasI(n, "PEBD") || lvHasI(n, "PEB_W")))
    {
        if (lvHasI(n, "PEBD"))
        {
            g_ustrCtorC = (FnUStrCtorC)addr;
            LvLogf("LimbVigor: mygui bind UString ctor(char*) '%s'", n);
        }
    }
    if (!g_ustrCtorS && std::strncmp(n, "??0UString@MyGUI", 16) == 0
     && lvHasI(n, "basic_string@D"))
    {
        g_ustrCtorS = (FnUStrCtorS)addr;
        LvLogf("LimbVigor: mygui bind UString ctor(string) '%s'", n);
    }
    if (!g_ustrDtor && std::strncmp(n, "??1UString@MyGUI", 16) == 0)
    {
        g_ustrDtor = (FnUStrDtor)addr;
        LvLogf("LimbVigor: mygui bind UString dtor '%s'", n);
    }
}

static HMODULE lvMyGuiMod()
{
    HMODULE mod = GetModuleHandleA("MyGUIEngine_x64.dll");
    if (!mod)
        mod = GetModuleHandleA("MyGUIEngine.dll");
    return mod;
}

/* GetModuleHandle range — do not hardcode the v1.20 ASLR address. */
static void lvSetMyGuiRange(HMODULE mod)
{
    if (!mod || g_myguiLo)
        return;
    const unsigned char* base = (const unsigned char*)(const void*)mod;
    IMAGE_DOS_HEADER dos;
    std::memcpy(&dos, base, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000)
        return;
    IMAGE_NT_HEADERS64 nt;
    std::memcpy(&nt, base + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE)
        return;
    const DWORD sz = nt.OptionalHeader.SizeOfImage;
    if (!sz)
        return;
    g_myguiLo = (void*)mod;
    g_myguiHi = (char*)(void*)mod + sz;
    LvLogf("LimbVigor: MyGUIEngine range %p-%p size=0x%X", g_myguiLo, g_myguiHi, (unsigned)sz);
}

static FARPROC lvGetProcOne(HMODULE mod, const char* const* names)
{
    if (!mod || !names)
        return nullptr;
    for (int i = 0; names[i]; ++i)
    {
        FARPROC p = GetProcAddress(mod, names[i]);
        if (p)
            return p;
    }
    return nullptr;
}

static const char* lvGetProcName(HMODULE mod, const char* const* names)
{
    if (!mod || !names)
        return nullptr;
    for (int i = 0; names[i]; ++i)
    {
        if (GetProcAddress(mod, names[i]))
            return names[i];
    }
    return nullptr;
}

static void lvLogBindFlags(const char* why)
{
    LvLogf("LimbVigor: setCapW=0 setCapISub=%d setCapEdit=%d setSize=%d setCoord=%d setVis=%d getW=%d getCoord=%d setDepth=%d (%s)",
           g_setCapISub ? 1 : 0, g_setCapEdit ? 1 : 0,
           g_setSizeHH ? 1 : 0, g_setCoordHHHH ? 1 : 0, g_setVis ? 1 : 0,
           g_getWidth ? 1 : 0, (g_getCoord || g_getAbsCoord) ? 1 : 0,
           g_setDepth ? 1 : 0, why ? why : "?");
}

/* ISub is the write path. Widget::setCaption is not exported — do not hunt it. */
static void lvBindISubSetCaption(HMODULE mod)
{
    if (!mod)
        return;
    static const char* kSetCapISubQ[] = {
        "?setCaption@ISubWidgetText@MyGUI@@QEAAXAEBVUString@2@@Z",
        "?setCaption@ISubWidgetText@MyGUI@@QEAAXAEBVUString@MyGUI@@@Z",
        nullptr
    };
    static const char* kSetCapISubU[] = {
        "?setCaption@ISubWidgetText@MyGUI@@UEAAXAEBVUString@2@@Z",
        "?setCaption@ISubWidgetText@MyGUI@@UEAAXAEBVUString@MyGUI@@@Z",
        nullptr
    };
    static const char* kSetCapEditQ[] = {
        "?setCaption@EditText@MyGUI@@QEAAXAEBVUString@2@@Z",
        "?setCaption@EditText@MyGUI@@QEAAXAEBVUString@MyGUI@@@Z",
        nullptr
    };
    static const char* kSetCapEditU[] = {
        "?setCaption@EditText@MyGUI@@UEAAXAEBVUString@2@@Z",
        "?setCaption@EditText@MyGUI@@UEAAXAEBVUString@MyGUI@@@Z",
        nullptr
    };
    static const char* kGetSub[] = {
        "?getSubWidgetText@Widget@MyGUI@@QEAAPEAVISubWidgetText@2@XZ",
        "?getSubWidgetText@Widget@MyGUI@@QEBAPEAVISubWidgetText@2@XZ",
        "?getSubWidgetText@Widget@MyGUI@@QEAAPEAVISubWidgetText@MyGUI@@XZ",
        "?getSubWidgetText@Widget@MyGUI@@QEBAPEAVISubWidgetText@MyGUI@@XZ",
        nullptr
    };
    static const char* kSetVis[] = {
        "?setVisible@Widget@MyGUI@@QEAAX_N@Z",
        "?setVisible@Widget@MyGUI@@UEAAX_N@Z",
        nullptr
    };
    static const char* kSetSize[] = {
        "?setSize@Widget@MyGUI@@QEAAXHH@Z",
        "?setSize@Widget@MyGUI@@UEAAXHH@Z",
        nullptr
    };
    static const char* kSetCoord[] = {
        "?setCoord@Widget@MyGUI@@QEAAXHHHH@Z",
        "?setCoord@Widget@MyGUI@@UEAAXHHHH@Z",
        nullptr
    };
    static const char* kGetWidth[] = {
        "?getWidth@ICroppedRectangle@MyGUI@@QEBAHXZ",
        "?getWidth@ICroppedRectangle@MyGUI@@QEAAHXZ",
        "?getWidth@Widget@MyGUI@@QEBAHXZ",
        "?getWidth@Widget@MyGUI@@QEAAHXZ",
        "?getWidth@Widget@MyGUI@@UEBAHXZ",
        nullptr
    };
    static const char* kGetSize[] = {
        "?getSize@Widget@MyGUI@@QEBA?AUIntSize@2@XZ",
        "?getSize@Widget@MyGUI@@UEBA?AUIntSize@2@XZ",
        nullptr
    };
    static const char* kGetCoordRef[] = {
        "?getCoord@ICroppedRectangle@MyGUI@@QEBAAEBUIntCoord@2@XZ",
        "?getCoord@ICroppedRectangle@MyGUI@@QEBAAEBVIntCoord@2@XZ",
        "?getCoord@Widget@MyGUI@@QEBAAEBUIntCoord@2@XZ",
        nullptr
    };
    static const char* kGetHeight[] = {
        "?getHeight@Widget@MyGUI@@QEBAHXZ",
        "?getHeight@Widget@MyGUI@@UEBAHXZ",
        nullptr
    };
    static const char* kGetLeft[] = {
        "?getLeft@Widget@MyGUI@@QEBAHXZ",
        "?getLeft@Widget@MyGUI@@UEBAHXZ",
        nullptr
    };
    static const char* kGetTop[] = {
        "?getTop@Widget@MyGUI@@QEBAHXZ",
        "?getTop@Widget@MyGUI@@UEBAHXZ",
        nullptr
    };
    static const char* kGetParent[] = {
        "?getParent@Widget@MyGUI@@QEAAPEAV12@XZ",
        "?getParent@Widget@MyGUI@@QEBAPEAV12@XZ",
        nullptr
    };
    static const char* kGetCoord[] = {
        "?getCoord@Widget@MyGUI@@QEBA?AUIntCoord@2@XZ",
        "?getCoord@Widget@MyGUI@@UEBA?AUIntCoord@2@XZ",
        nullptr
    };
    static const char* kGetAbsCoord[] = {
        "?getAbsoluteCoord@Widget@MyGUI@@QEBA?AUIntCoord@2@XZ",
        "?getAbsoluteCoord@Widget@MyGUI@@UEBA?AUIntCoord@2@XZ",
        nullptr
    };
    static const char* kGetParentSize[] = {
        "?getParentSize@Widget@MyGUI@@QEBA?AUIntSize@2@XZ",
        "?getParentSize@Widget@MyGUI@@UEBA?AUIntSize@2@XZ",
        nullptr
    };
    static const char* kSetDepth[] = {
        "?setDepth@Widget@MyGUI@@QEAAXH@Z",
        "?setDepth@Widget@MyGUI@@UEAAXH@Z",
        nullptr
    };
    FARPROC iq = lvGetProcOne(mod, kSetCapISubQ);
    if (iq)
    {
        g_setCapISub = (FnSetCaption)iq;
        g_setCapISubNonVirt = 1;
        LvLogf("LimbVigor: mygui bind ISubWidgetText::setCaption EXACT QEAAX '%s'",
               lvGetProcName(mod, kSetCapISubQ));
    }
    else
    {
        FARPROC iu = lvGetProcOne(mod, kSetCapISubU);
        if (iu && !g_setCapISub)
        {
            g_setCapISub = (FnSetCaption)iu;
            LvLogf("LimbVigor: mygui bind ISubWidgetText::setCaption EXACT UEAAX '%s'",
                   lvGetProcName(mod, kSetCapISubU));
        }
    }
    FARPROC eq = lvGetProcOne(mod, kSetCapEditQ);
    if (eq)
    {
        g_setCapEdit = (FnSetCaption)eq;
        g_setCapEditNonVirt = 1;
        LvLogf("LimbVigor: mygui bind EditText::setCaption EXACT QEAAX '%s'",
               lvGetProcName(mod, kSetCapEditQ));
    }
    else
    {
        FARPROC eu = lvGetProcOne(mod, kSetCapEditU);
        if (eu && !g_setCapEdit)
        {
            g_setCapEdit = (FnSetCaption)eu;
            LvLogf("LimbVigor: mygui bind EditText::setCaption EXACT UEAAX '%s'",
                   lvGetProcName(mod, kSetCapEditU));
        }
    }
    FARPROC st = lvGetProcOne(mod, kGetSub);
    if (st && !g_getSubText)
    {
        g_getSubText = (FnGetSubText)st;
        LvLogf("LimbVigor: mygui bind Widget::getSubWidgetText EXACT '%s'",
               lvGetProcName(mod, kGetSub));
    }
    FARPROC vq = lvGetProcOne(mod, kSetVis);
    if (vq && !g_setVis)
    {
        g_setVis = (FnSetVisible)vq;
        LvLogf("LimbVigor: mygui bind Widget::setVisible EXACT '%s'",
               lvGetProcName(mod, kSetVis));
    }
    FARPROC sz = lvGetProcOne(mod, kSetSize);
    if (sz && !g_setSizeHH)
    {
        g_setSizeHH = (FnSetSizeHH)sz;
        LvLogf("LimbVigor: mygui bind Widget::setSize EXACT '%s'",
               lvGetProcName(mod, kSetSize));
    }
    FARPROC cd = lvGetProcOne(mod, kSetCoord);
    if (cd && !g_setCoordHHHH)
    {
        g_setCoordHHHH = (FnSetCoordHHHH)cd;
        LvLogf("LimbVigor: mygui bind Widget::setCoord EXACT '%s'",
               lvGetProcName(mod, kSetCoord));
    }
    FARPROC gw = lvGetProcOne(mod, kGetWidth);
    if (gw && !g_getWidth)
    {
        g_getWidth = (FnGetInt)gw;
        LvLogf("LimbVigor: mygui bind getWidth EXACT '%s'", lvGetProcName(mod, kGetWidth));
    }
    FARPROC gsz = lvGetProcOne(mod, kGetSize);
    if (gsz && !g_getSizeU64)
    {
        g_getSizeU64 = (FnGetSizeU64)gsz;
        LvLogf("LimbVigor: mygui bind getSize EXACT '%s'", lvGetProcName(mod, kGetSize));
    }
    FARPROC gcr = lvGetProcOne(mod, kGetCoordRef);
    if (gcr && !g_getCoordRef)
    {
        g_getCoordRef = (FnGetCoordRef)gcr;
        LvLogf("LimbVigor: mygui bind getCoord ref EXACT '%s'", lvGetProcName(mod, kGetCoordRef));
    }
    FARPROC gh = lvGetProcOne(mod, kGetHeight);
    if (gh && !g_getHeight) g_getHeight = (FnGetInt)gh;
    FARPROC gl = lvGetProcOne(mod, kGetLeft);
    if (gl && !g_getLeft) g_getLeft = (FnGetInt)gl;
    FARPROC gt = lvGetProcOne(mod, kGetTop);
    if (gt && !g_getTop) g_getTop = (FnGetInt)gt;
    FARPROC gp = lvGetProcOne(mod, kGetParent);
    if (gp && !g_getParent)
    {
        g_getParent = (FnGetParent)gp;
        LvLogf("LimbVigor: mygui bind Widget::getParent EXACT '%s'",
               lvGetProcName(mod, kGetParent));
    }
    FARPROC gc = lvGetProcOne(mod, kGetCoord);
    if (gc && !g_getCoord)
    {
        g_getCoord = (FnGetCoordSret)gc;
        LvLogf("LimbVigor: mygui bind Widget::getCoord EXACT '%s'",
               lvGetProcName(mod, kGetCoord));
    }
    FARPROC ga = lvGetProcOne(mod, kGetAbsCoord);
    if (ga && !g_getAbsCoord)
    {
        g_getAbsCoord = (FnGetCoordSret)ga;
        LvLogf("LimbVigor: mygui bind Widget::getAbsoluteCoord EXACT '%s'",
               lvGetProcName(mod, kGetAbsCoord));
    }
    FARPROC gps = lvGetProcOne(mod, kGetParentSize);
    if (gps && !g_getParentSize)
    {
        g_getParentSize = (FnGetSizeU64)gps;
        LvLogf("LimbVigor: mygui bind Widget::getParentSize EXACT '%s'",
               lvGetProcName(mod, kGetParentSize));
    }
    FARPROC sd = lvGetProcOne(mod, kSetDepth);
    if (sd && !g_setDepth)
    {
        g_setDepth = (FnSetDepth)sd;
        LvLogf("LimbVigor: mygui bind Widget::setDepth EXACT '%s'",
               lvGetProcName(mod, kSetDepth));
    }
    static int bindTalked = 0;
    if (!bindTalked)
    {
        bindTalked = 1;
        lvLogBindFlags(g_setCapISub
            ? "ISub Datapanel + strip + pixel Green, setCapW=0 expected"
            : "ISub MISSING — will retry (setCapW=0 expected)");
    }
}

static void lvDumpMyGuiExports()
{
    HMODULE mod = lvMyGuiMod();
    if (!mod)
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLog("LimbVigor: mygui export list — MyGUIEngine not loaded (will retry)");
        }
        return;
    }
    /* Latch on ISub + setVis. Widget::setCaption is not exported — do not wait for it. */
    if (g_exportDumped && g_setCapISub && g_setVis && (g_setSizeHH || g_setCoordHHHH))
        return;
    static int dumpTalked = 0;
    const int talk = !dumpTalked;
    dumpTalked = 1;
    lvSetMyGuiRange(mod);
    if (talk)
        LvLogf("LimbVigor: mygui module=%p", (void*)mod);

    FARPROC exactGui = GetProcAddress(mod, kGuiGetInstanceExact);
    if (exactGui)
        g_guiInst = (FnGuiInst)exactGui;
    static const char kFindWidgetTConst[] =
        "?findWidgetT@Gui@MyGUI@@QEBAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z";
    FARPROC exactFind = GetProcAddress(mod, kFindWidgetTExact);
    if (!exactFind)
        exactFind = GetProcAddress(mod, kFindWidgetTConst);
    if (exactFind)
    {
        g_findW = (FnFindW)exactFind;
        g_findSym = GetProcAddress(mod, kFindWidgetTExact) ? kFindWidgetTExact : kFindWidgetTConst;
    }
    static const char kFindWidgetT3[] =
        "?findWidgetT@Gui@MyGUI@@QEAAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0_N@Z";
    static const char kFindWidgetT3c[] =
        "?findWidgetT@Gui@MyGUI@@QEBAPEAVWidget@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0_N@Z";
    FARPROC exact3 = GetProcAddress(mod, kFindWidgetT3);
    if (!exact3)
        exact3 = GetProcAddress(mod, kFindWidgetT3c);
    if (exact3)
    {
        g_findW3 = (FnFindW3)exact3;
        g_find3Sym = GetProcAddress(mod, kFindWidgetT3) ? kFindWidgetT3 : kFindWidgetT3c;
    }
    FARPROC exactWm = GetProcAddress(mod, kWmGetInstanceExact);
    if (exactWm)
        g_wmInst = (FnGuiInst)exactWm;
    /* v1.25: Widget::setCaption not in exports. Bind ISub + EditText + getSubWidgetText. */
    lvBindISubSetCaption(mod);
    static const char kSetCapTextQ[] =
        "?setCaption@TextBox@MyGUI@@QEAAXAEBVUString@2@@Z";
    static const char kSetCapTextU[] =
        "?setCaption@TextBox@MyGUI@@UEAAXAEBVUString@2@@Z";
    FARPROC tq = GetProcAddress(mod, kSetCapTextQ);
    if (!tq)
        tq = GetProcAddress(mod, kSetCapTextU);
    if (tq)
    {
        g_setCapTextBox = (FnSetCaption)tq;
    }
    static const char kSetCapRep[] =
        "?setCaptionWithReplacing@TextBox@MyGUI@@QEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z";
    FARPROC rep = GetProcAddress(mod, kSetCapRep);
    if (rep && !g_setCapStr)
    {
        g_setCapStr = (FnSetCapStr)rep;
        LvLogf("LimbVigor: mygui bind setCaptionWithReplacing EXACT");
    }
    static const char kUCtorC[] = "??0UString@MyGUI@@QEAA@PEBD@Z";
    static const char kUCtorS[] =
        "??0UString@MyGUI@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z";
    static const char kUDtor[] = "??1UString@MyGUI@@QEAA@XZ";
    FARPROC uc = GetProcAddress(mod, kUCtorC);
    if (uc) g_ustrCtorC = (FnUStrCtorC)uc;
    FARPROC us = GetProcAddress(mod, kUCtorS);
    if (us) g_ustrCtorS = (FnUStrCtorS)us;
    FARPROC ud = GetProcAddress(mod, kUDtor);
    if (ud) g_ustrDtor = (FnUStrDtor)ud;

    const unsigned char* base = (const unsigned char*)(const void*)mod;
    IMAGE_DOS_HEADER dos;
    std::memcpy(&dos, base, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000)
        return;
    IMAGE_NT_HEADERS64 nt;
    std::memcpy(&nt, base + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE)
        return;
    const IMAGE_DATA_DIRECTORY dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress || dir.Size < sizeof(IMAGE_EXPORT_DIRECTORY))
        return;
    IMAGE_EXPORT_DIRECTORY exp;
    std::memcpy(&exp, base + dir.VirtualAddress, sizeof(exp));
    if (!exp.NumberOfNames || exp.NumberOfNames > 20000)
        return;

    const DWORD* names = (const DWORD*)(base + exp.AddressOfNames);
    const WORD* ords = (const WORD*)(base + exp.AddressOfNameOrdinals);
    const DWORD* funcs = (const DWORD*)(base + exp.AddressOfFunctions);
    int loggedFind = 0;
    int loggedCap = 0;
    for (DWORD i = 0; i < exp.NumberOfNames; ++i)
    {
        DWORD nrva = 0;
        WORD ord = 0;
        DWORD frva = 0;
        LV_TRY
        {
            nrva = names[i];
            ord = ords[i];
            frva = funcs[ord];
        }
        LV_EXCEPT { continue; }
        if (!nrva)
            continue;
        const char* n = (const char*)(base + nrva);
        char en[256];
        en[0] = 0;
        LV_TRY
        {
            int k = 0;
            while (n[k] && k < 255) { en[k] = n[k]; ++k; }
            en[k] = 0;
        }
        LV_EXCEPT { en[0] = 0; }
        if (!en[0])
            continue;
        void* addr = (void*)(base + frva);
        lvTryBindExport(en, addr);
        if (lvHasI(en, "setCaption"))
            loggedCap++;
        if (lvHasI(en, "findWidget") || lvHasI(en, "findById"))
            loggedFind++;
    }
    static const char* kWFindExact[] = {
        "?findWidget@Widget@MyGUI@@QEAAPEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z",
        "?findWidget@Widget@MyGUI@@QEBAPEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z",
        "?findWidget@Widget@MyGUI@@QEAAPEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z",
        "?findWidget@Widget@MyGUI@@QEBAPEAV12@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z",
        nullptr
    };
    for (int i = 0; kWFindExact[i]; ++i)
    {
        FARPROC wf = GetProcAddress(mod, kWFindExact[i]);
        if (!wf)
            continue;
        if (lvHasI(kWFindExact[i], "_N"))
        {
            if (!g_wFind2)
                g_wFind2 = (FnWFind2)wf;
        }
        else if (!g_wFind1)
            g_wFind1 = (FnWFind1)wf;
    }

    (void)loggedFind;
    (void)loggedCap;
    if (g_setCapISub)
        g_exportDumped = 1;
    else
        g_exportDumped = 0;
}

static int lvGameStrLooksName(const char* s)
{
    if (!s || !s[0])
        return 0;
    int n = 0;
    for (; s[n]; ++n)
    {
        unsigned char c = (unsigned char)s[n];
        if (n > 64)
            return 0;
        if (c < 32 || c > 126)
            return 0;
    }
    return 1;
}

static void lvReadCaption(const void* s, char* out, int n)
{
    if (!out || n < 2)
        return;
    out[0] = 0;
    if (!s)
        return;
    if (GameStrRead(s, out, n) && lvGameStrLooksName(out))
        return;
    out[0] = 0;
    size_t size = 0, cap = 0;
    LV_TRY
    {
        std::memcpy(&size, (const char*)s + 16, sizeof(size));
        std::memcpy(&cap, (const char*)s + 24, sizeof(cap));
    }
    LV_EXCEPT { return; }
    if (size == 0 || size > 64 || cap > (size_t)1 << 20)
        return;
    const wchar_t* src = nullptr;
    LV_TRY
    {
        if (cap > 7)
            std::memcpy(&src, s, sizeof(src));
        else
            src = (const wchar_t*)s;
    }
    LV_EXCEPT { src = nullptr; }
    if (!src)
        return;
    int o = 0;
    LV_TRY
    {
        for (size_t i = 0; i < size && o < n - 1; ++i)
        {
            unsigned c = (unsigned)src[i];
            if (c == 0)
                break;
            if (c < 32 || c > 126)
                continue;
            out[o++] = (char)c;
        }
        out[o] = 0;
    }
    LV_EXCEPT { out[0] = 0; }
}

/* v1.21 proved the "ugly" prefix is real: hex(bar+0x30) grouped in threes + '_'.
 * Do not reject commas. Printable std::string of size 2..64 is enough. */
static int lvPrefixUsable(const char* s, size_t sz)
{
    if (!s || !s[0] || sz < 2 || sz > 64)
        return 0;
    for (size_t i = 0; i < sz && s[i]; ++i)
    {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c > 126)
            return 0;
    }
    return 1;
}

/* Real MSVC 2010 std::string: size@+16, cap@+24, SSO if cap<=15. Never a C string. */
static int lvReadMsvcStdStr(const void* obj, char* out, int outsz,
                            size_t* osz, size_t* ocap, int* heap)
{
    if (osz) *osz = 0;
    if (ocap) *ocap = 0;
    if (heap) *heap = 0;
    if (out && outsz > 0) out[0] = 0;
    if (!obj)
        return 0;
    size_t size = 0, cap = 0;
    LV_TRY
    {
        std::memcpy(&size, (const char*)obj + 16, sizeof(size));
        std::memcpy(&cap, (const char*)obj + 24, sizeof(cap));
    }
    LV_EXCEPT { return 0; }
    if (osz) *osz = size;
    if (ocap) *ocap = cap;
    if (size > 200 || cap > (size_t)1 << 20)
        return 0;
    const char* src = nullptr;
    int isHeap = 0;
    LV_TRY
    {
        if (cap > 15)
        {
            std::memcpy(&src, obj, sizeof(src));
            isHeap = 1;
        }
        else
            src = (const char*)obj;
    }
    LV_EXCEPT { return 0; }
    if (heap) *heap = isHeap;
    if (!src || !out || outsz < 2)
        return (size == 0 && cap <= (size_t)1 << 20) ? 1 : 0;
    int n = (int)size;
    if (n >= outsz) n = outsz - 1;
    if (n > 0)
    {
        LV_TRY { std::memcpy(out, src, (size_t)n); }
        LV_EXCEPT { out[0] = 0; return 0; }
    }
    out[n] = 0;
    return 1;
}

static void lvReadBasePrefix(void* bar)
{
    if (g_prefixLogged)
        return;
    g_prefixLogged = 1;
    g_prefix[0] = 0;
    if (!bar)
        return;
    /* v1.21: +0x40 size=22 heap=1 is the real prefix. Try it first. */
    static const int kOff[] = { 0x40, 0x30 };
    for (int i = 0; i < 2; ++i)
    {
        char buf[96];
        size_t sz = 0, cap = 0;
        int heap = 0;
        buf[0] = 0;
        const int ok = lvReadMsvcStdStr((const char*)bar + kOff[i], buf, (int)sizeof(buf),
                                        &sz, &cap, &heap);
        if (ok && lvPrefixUsable(buf, sz))
        {
            std::snprintf(g_prefix, sizeof(g_prefix), "%s", buf);
            return;
        }
    }
}

/* POD-only SEH stubs. MSVC will not mix __try with C++ objects in one fn.
 * C++ catch lives in the caller — MyGUI throw is not an SEH exception. */
static int g_lastFindSeh = 0;
static int g_findSehLogged = 0;

static void lvLogFindSehOnce(const char* path)
{
    if (!g_lastFindSeh)
        return;
    g_lastFindSeh = 0;
    if (g_findSehLogged)
        return;
    g_findSehLogged = 1;
    LvLogf("LimbVigor: find SEH on %s — skip that path, no _getWidget retry",
           path ? path : "?");
}

static void* lvSehGuiInst(FnGuiInst fn)
{
    void* p = nullptr;
    if (!fn) return nullptr;
    LV_TRY { p = fn(); }
    LV_EXCEPT { p = nullptr; g_lastFindSeh = 1; }
    return p;
}

static void* lvSehFind2(FnFindW fn, void* self, const GameStr* name)
{
    void* w = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!fn || !self || !name) return nullptr;
    const unsigned char noThrow = 0;
    LV_TRY { w = fn(self, name, noThrow); }
    LV_EXCEPT { w = nullptr; g_lastFindSeh = 1; lvHudSehHit(nullptr); }
    return w;
}

static void* lvSehFind3(FnFindW3 fn, void* self, const GameStr* name, const GameStr* prefix)
{
    void* w = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!fn || !self || !name || !prefix) return nullptr;
    const unsigned char noThrow = 0;
    LV_TRY { w = fn(self, name, prefix, noThrow); }
    LV_EXCEPT { w = nullptr; g_lastFindSeh = 1; lvHudSehHit(nullptr); }
    return w;
}

static void* lvSehWFind1(void* self, const GameStr* name)
{
    void* w = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!g_wFind1 || !self || !name) return nullptr;
    LV_TRY { w = g_wFind1(self, name); }
    LV_EXCEPT { w = nullptr; g_lastFindSeh = 1; lvHudSehHit(nullptr); }
    return w;
}

static void* lvSehWFind2(void* self, const GameStr* name)
{
    void* w = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!g_wFind2 || !self || !name) return nullptr;
    const unsigned char noThrow = 0;
    LV_TRY { w = g_wFind2(self, name, noThrow); }
    LV_EXCEPT { w = nullptr; g_lastFindSeh = 1; lvHudSehHit(nullptr); }
    return w;
}

static int lvSehVisible(void* w)
{
    int vis = -1;
    if (!lvHudMyGuiOk()) return -1;
    if (!g_wVis || !w) return -1;
    LV_TRY { vis = g_wVis(w) ? 1 : 0; }
    LV_EXCEPT { vis = -1; lvHudSehHit(w); }
    return vis;
}

static const void* lvSehGetName(void* w)
{
    const void* s = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!g_wName || !w) return nullptr;
    LV_TRY { s = g_wName(w); }
    LV_EXCEPT { s = nullptr; lvHudSehHit(w); }
    return s;
}

static int lvProbeOneHud(void* w)
{
    if (!w)
        return 1;
    int vis = -1;
    const void* n = nullptr;
    LV_TRY { vis = lvSehVisible(w); }
    LV_EXCEPT { vis = -1; }
    LV_TRY { n = lvSehGetName(w); }
    LV_EXCEPT { n = nullptr; }
    return (vis >= 0 && n) ? 1 : 0;
}

static void lvResolveLifeBar(void);

/* Forget yesterday (no MyGUI calls), find HemolymphBar* THIS tick, write
 * only those returns. A null find during reload skips writes. */
static int lvHudBeginWrites(void)
{
    const int had = g_wHemo ? 1 : 0;
    if (g_hudSkipTick)
        return 0;
    lvHudNullAll();
    lvResolveLifeBar();
    if (g_wHemo)
        return 1;
    if (had)
        lvHudInvalidate();
    else
        g_hudSkipTick = 1;
    return 0;
}

static void* lvSehGetSubText(void* w);

static const void* lvSehCaption(void* w)
{
    const void* s = nullptr;
    if (!lvHudMyGuiOk()) return nullptr;
    if (!w) return nullptr;
    if (g_getCapISub)
    {
        LV_TRY { s = g_getCapISub(w); }
        LV_EXCEPT { s = nullptr; lvHudSehHit(w); }
        if (s) return s;
    }
    if (!g_wCaption) return nullptr;
    LV_TRY { s = g_wCaption(w); }
    LV_EXCEPT { s = nullptr; lvHudSehHit(w); }
    return s;
}

static void* lvCppFind2(FnFindW fn, void* self, const GameStr* name)
{
    void* w = nullptr;
    try { w = lvSehFind2(fn, self, name); }
    catch (...) { w = nullptr; }
    return w;
}

static void* lvCppFind3(FnFindW3 fn, void* self, const GameStr* name, const GameStr* prefix)
{
    void* w = nullptr;
    try { w = lvSehFind3(fn, self, name, prefix); }
    catch (...) { w = nullptr; }
    return w;
}

static void* lvCppWFind(void* self, const GameStr* name)
{
    void* w = nullptr;
    try
    {
        if (g_wFind2)
            w = lvSehWFind2(self, name);
        if (!w && g_wFind1)
            w = lvSehWFind1(self, name);
    }
    catch (...) { w = nullptr; }
    return w;
}

static void* lvFind3(const char* name)
{
    if (!name || !name[0] || !g_prefix[0] || !g_findW3)
        return nullptr;
    GameStr gn, gp;
    GameStrSet(&gn, name);
    GameStrSet(&gp, g_prefix);
    if (g_guiInst)
    {
        void* gui = nullptr;
        try { gui = lvSehGuiInst(g_guiInst); }
        catch (...) { gui = nullptr; }
        void* w = lvCppFind3(g_findW3, gui, &gn, &gp);
        if (w)
            return w;
    }
    if (g_wmFind3 && g_wmInst)
    {
        void* wm = nullptr;
        try { wm = lvSehGuiInst(g_wmInst); }
        catch (...) { wm = nullptr; }
        return lvCppFind3(g_wmFind3, wm, &gn, &gp);
    }
    return nullptr;
}

static void* lvFind2Full(const char* full)
{
    if (!full || !full[0] || !g_findW)
        return nullptr;
    GameStr gn;
    GameStrSet(&gn, full);
    if (g_guiInst)
    {
        void* gui = nullptr;
        try { gui = lvSehGuiInst(g_guiInst); }
        catch (...) { gui = nullptr; }
        void* w = lvCppFind2(g_findW, gui, &gn);
        if (w)
            return w;
    }
    if (g_wmFind && g_wmInst)
    {
        void* wm = nullptr;
        try { wm = lvSehGuiInst(g_wmInst); }
        catch (...) { wm = nullptr; }
        return lvCppFind2(g_wmFind, wm, &gn);
    }
    return nullptr;
}

static void* lvRootFind(void* root, const char* name)
{
    if (!root || !name || !name[0] || (!g_wFind1 && !g_wFind2))
        return nullptr;
    GameStr gn;
    GameStrSet(&gn, name);
    return lvCppWFind(root, &gn);
}

static int lvHasSub(const char* h, const char* n)
{
    return (h && n && n[0] && std::strstr(h, n)) ? 1 : 0;
}

static int lvCapIsBlood(const char* c)
{
    if (!c || !c[0])
        return 0;
    return (std::strcmp(c, "Blood") == 0 || std::strcmp(c, "Oil") == 0
         || std::strcmp(c, "blood") == 0 || std::strcmp(c, "oil") == 0) ? 1 : 0;
}

static int lvIsFillBarName(const char* n)
{
    return (lvHasSub(n, "HemolymphBarGrey") || lvHasSub(n, "HemolymphBarRed")
         || lvHasSub(n, "HemolymphBarYellow") || lvHasSub(n, "HemolymphBarWhite")
         || lvHasSub(n, "HemolymphBarGreen") || lvHasSub(n, "HemolymphBarRobot")
         || lvHasSub(n, "HemolymphBarCrushed")) ? 1 : 0;
}

static int lvEndsWith(const char* s, const char* suf)
{
    if (!s || !suf || !suf[0])
        return 0;
    const size_t n = std::strlen(s);
    const size_t m = std::strlen(suf);
    if (n < m)
        return 0;
    return std::strcmp(s + (n - m), suf) == 0 ? 1 : 0;
}

static int lvIsForbiddenParent(const char* n)
{
    if (!n || !n[0])
        return 1;
    return (lvHasSub(n, "Root") || lvHasSub(n, "MedicalPanel")
         || lvHasSub(n, "MedicalPanel_Back") || lvHasSub(n, "MedicalPanel_Front")
         || lvHasSub(n, "StatusPanel") || lvHasSub(n, "SquadPanel")
         || lvHasSub(n, "Squad") || lvHasSub(n, "Floor")
         || lvHasSub(n, "NamePanel") || lvEndsWith(n, "Name")
         || lvHasSub(n, "Money") || lvHasSub(n, "Biome")
         || lvHasSub(n, "Paused") || lvHasSub(n, "Loading")
         || lvHasSub(n, "Day") || (lvHasSub(n, "Time") && !lvHasSub(n, "LifeBar"))) ? 1 : 0;
}

/* Write: name ends with HemolymphBarDatapanel only (the label-on-the-bar).
 * Never LifeBar10 itself. Never LifeBar1 / Blood. Never Root / fills. */
static int lvNameIsLifeBar1Strict(const char* name)
{
    if (!name || !name[0])
        return 0;
    if (lvEndsWith(name, "HemolymphBar") || lvEndsWith(name, "HemolymphBarDatapanel")
     || lvEndsWith(name, "HemolymphValue") || lvHasSub(name, "HemolymphBar"))
        return 0;
    return (lvEndsWith(name, "LifeBar1") || lvEndsWith(name, "LifeBar1Datapanel")
         || lvEndsWith(name, "LifeBar1Value")) ? 1 : 0;
}

static int lvNameIsHemoWrite(const char* name)
{
    if (!name || !name[0])
        return 0;
    if (lvNameIsLifeBar1Strict(name) || lvIsForbiddenParent(name) || lvIsFillBarName(name))
        return 0;
    if (lvEndsWith(name, "HemolymphBarDatapanel"))
        return 1;
    return 0;
}

static int lvRankWidget(const char* name, const char* cap)
{
    (void)cap;
    if (lvIsFillBarName(name) || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    if (lvNameIsHemoWrite(name))
        return lvEndsWith(name, "HemolymphBarDatapanel") ? 2 : 3;
    return 0;
}

static void lvReadWidget(void* w, char* name, int nn, char* cap, int nc, int* vis)
{
    if (name && nn > 0) name[0] = 0;
    if (cap && nc > 0) cap[0] = 0;
    if (vis) *vis = -1;
    if (!w)
        return;
    try { if (vis) *vis = lvSehVisible(w); }
    catch (...) { if (vis) *vis = -1; }
    const void* s = nullptr;
    try { s = lvSehGetName(w); }
    catch (...) { s = nullptr; }
    if (s && name)
        GameStrRead(s, name, nn);
    s = nullptr;
    try { s = lvSehCaption(w); }
    catch (...) { s = nullptr; }
    if (s && cap)
        lvReadCaption(s, cap, nc);
    if (cap && !cap[0] && g_getSubText)
    {
        void* sub = lvSehGetSubText(w);
        if (sub)
        {
            s = nullptr;
            try { s = lvSehCaption(sub); }
            catch (...) { s = nullptr; }
            if (s)
                lvReadCaption(s, cap, nc);
        }
    }
}

/* Live getName check — this is the write-path gate, not a comment. */
static int lvWriteDestOk(void* w, char* nameOut, int nsz)
{
    if (nameOut && nsz > 0)
        nameOut[0] = 0;
    if (!w)
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    (void)cap;
    if (nameOut && nsz > 0)
        std::snprintf(nameOut, (size_t)nsz, "%s", name);
    if (lvNameIsLifeBar1Strict(name))
        return 0;
    if (lvNameIsHemoWrite(name))
        return 1;
    return 0;
}

static int lvDestNameGated(void* w)
{
    return lvWriteDestOk(w, nullptr, 0);
}

static void lvConsider(void* w, const char* src)
{
    if (!w)
        return;
    char name[96], cap[96];
    int vis = -1;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    const int rank = lvRankWidget(name, cap);
    if (rank <= g_capRank)
        return;
    g_capWidget = w;
    g_capRank = rank;
    if (cap[0])
    {
        std::snprintf(g_capOrig, sizeof(g_capOrig), "%s", cap);
        g_capOrigHave = 1;
    }
    else
    {
        std::snprintf(g_capOrig, sizeof(g_capOrig), "Blood");
        g_capOrigHave = 1;
    }
}

static int lvVtInMyGui(void* p)
{
    if (!p || !g_myguiLo || !g_myguiHi)
        return 0;
    void* vt = nullptr;
    LV_TRY { std::memcpy(&vt, p, sizeof(vt)); }
    LV_EXCEPT { return 0; }
    if (!vt)
        return 0;
    return (vt >= g_myguiLo && vt < g_myguiHi) ? 1 : 0;
}

/* Member pointers only. Not a Gui tree walk. setSize is HemolymphBarGreen only. */
static void lvScanBarMembers(void* bar)
{
    if (!bar)
        return;
    int n = 0;
    for (int off = 8; off <= 0x400; off += (int)sizeof(void*))
    {
        void* p = nullptr;
        LV_TRY { std::memcpy(&p, (const char*)bar + off, sizeof(p)); }
        LV_EXCEPT { p = nullptr; }
        if (!p || p == bar || p == LvMedicalPanel())
            continue;
        if (!lvVtInMyGui(p))
            continue;
        char tag[32], name[96], cap[96];
        int vis = -1;
        std::snprintf(tag, sizeof(tag), "bar+0x%X", off);
        lvReadWidget(p, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
        LvLogf("LimbVigor: hunt %s widget=%p vis=%d name='%s' caption='%s' (log only, not a write dest)",
               tag, p, vis, name, cap);
        n++;
        if (n >= 40)
            break;
    }
    LvLogf("LimbVigor: MainBar member MyGUI widgets logged=%d (not write dests)", n);
}

static void lvWhyOnce(const char* why)
{
    if (g_lifeBarWhy)
        return;
    g_lifeBarWhy = 1;
    LvLog(why);
}

static int lvSehSetCapFn(FnSetCaption fn, void* w, const void* u)
{
    if (!lvHudMyGuiOk()) return 1;
    if (!fn || !w || !u)
        return 1;
    LV_TRY { fn(w, u); }
    LV_EXCEPT { lvHudSehHit(w); return 1; }
    return 0;
}

static int lvSehSetCapStr(void* w, const GameStr* s)
{
    if (!lvHudMyGuiOk()) return 1;
    if (!g_setCapStr || !w || !s)
        return 1;
    LV_TRY { g_setCapStr(w, s); }
    LV_EXCEPT { lvHudSehHit(w); return 1; }
    return 0;
}

static int lvSehUCtorC(void* u, const char* t)
{
    if (!g_ustrCtorC || !u || !t)
        return 1;
    LV_TRY { g_ustrCtorC(u, t); }
    LV_EXCEPT { return 1; }
    return 0;
}

static int lvSehUCtorS(void* u, const GameStr* s)
{
    if (!g_ustrCtorS || !u || !s)
        return 1;
    LV_TRY { g_ustrCtorS(u, s); }
    LV_EXCEPT { return 1; }
    return 0;
}

static void lvSehUDtor(void* u)
{
    if (!g_ustrDtor || !u)
        return;
    LV_TRY { g_ustrDtor(u); }
    LV_EXCEPT {}
}

static int lvSehSetCapFakeFn(FnSetCaption fn, void* w, void* fake)
{
    if (!lvHudMyGuiOk()) return 1;
    if (!fn || !w || !fake)
        return 1;
    LV_TRY { fn(w, fake); }
    LV_EXCEPT { lvHudSehHit(w); return 1; }
    return 0;
}

/* textBox=1 → TextBox (HemolymphValue only, not painted=1).
 * textBox=2 → ISubWidgetText (Value fallback).
 * textBox=0 → ISub / EditText key write. Never TextBox on LifeBar10 / Datapanel.
 * Never setCaption LifeBar1. Never setCaption Green/fill skins.
 * Widget::setCaption is not exported — do not pick it. */
static FnSetCaption lvPickSetCap(int textBox)
{
    if (textBox == 2)
        return g_setCapISub;
    if (textBox)
    {
        if (g_setCapTextBox)
            return g_setCapTextBox;
        return g_setCapISub;
    }
    if (g_setCapISub)
        return g_setCapISub;
    return g_setCapEdit;
}

static int lvNameIsValueWrite(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name))
        return 0;
    if (lvNameIsLifeBar1Strict(name) || lvEndsWith(name, "LifeBar1Value"))
        return 0;
    return lvEndsWith(name, "HemolymphValue") ? 1 : 0;
}

static int lvWriteDestOkFor(void* w, int textBox, char* nameOut, int nsz)
{
    if (nameOut && nsz > 0)
        nameOut[0] = 0;
    if (!w)
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (nameOut && nsz > 0)
        std::snprintf(nameOut, (size_t)nsz, "%s", name);
    if (textBox)
        return lvNameIsValueWrite(name);
    return lvWriteDestOk(w, nameOut, nsz);
}

static int lvCaptionLooksDigit(const char* c)
{
    if (!c)
        return 0;
    for (; *c; ++c)
    {
        if (*c >= '0' && *c <= '9')
            return 1;
    }
    return 0;
}

static int lvValueIsDigitBox(void* w)
{
    if (!w)
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsValueWrite(name))
        return 0;
    if (lvCapIsBlood(cap))
        return 0;
    return (!cap[0] || lvCaptionLooksDigit(cap)) ? 1 : 0;
}

static int lvTryWriteCaptionOn(void* w, const char* text, int textBox)
{
    if (!w || !text)
        return 1;
    {
        char nm[96];
        nm[0] = 0;
        if (!lvWriteDestOkFor(w, textBox, nm, (int)sizeof(nm)))
        {
            static int once = 0;
            if (!once)
            {
                once = 1;
                LvLogf("LimbVigor: refuse setCaption dest name='%s' textBox=%d",
                       nm, textBox);
            }
            return 1;
        }
    }
    FnSetCaption fn = lvPickSetCap(textBox);
    if (!fn)
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: %s::setCaption not bound — not using Widget",
                   textBox ? "TextBox" : "ISub");
        }
        return 1;
    }
    if ((g_ustrCtorC || g_ustrCtorS) && g_ustrDtor)
    {
        char ustr[512];
        std::memset(ustr, 0, sizeof(ustr));
        int built = 1;
        try
        {
            if (g_ustrCtorC)
                built = lvSehUCtorC(ustr, text);
            else
            {
                GameStr gs;
                GameStrSet(&gs, text);
                built = lvSehUCtorS(ustr, &gs);
            }
        }
        catch (...) { built = 1; }
        if (built)
            return 1;
        int seh = 1;
        try { seh = lvSehSetCapFn(fn, w, ustr); }
        catch (...) { seh = 1; }
        try { lvSehUDtor(ustr); }
        catch (...) {}
        return seh;
    }
    /* TextBox-only fallback. Never setCaptionWithReplacing on LifeBar1. */
    if (textBox && g_setCapStr)
    {
        GameStr gs;
        GameStrSet(&gs, text);
        try { return lvSehSetCapStr(w, &gs); }
        catch (...) { return 1; }
    }
    if (fn)
    {
        struct FakeU
        {
            union { wchar_t sso[8]; wchar_t* ptr; } u;
            size_t size;
            size_t cap;
            char pad[224];
        };
        static wchar_t pool[4][64];
        static int pi = 0;
        FakeU fake;
        std::memset(&fake, 0, sizeof(fake));
        size_t n = std::strlen(text);
        if (n > 48) n = 48;
        wchar_t* slot = pool[pi++ & 3];
        for (size_t i = 0; i < n; ++i)
            slot[i] = (wchar_t)(unsigned char)text[i];
        slot[n] = 0;
        fake.u.ptr = slot;
        fake.size = n;
        fake.cap = 63;
        try { return lvSehSetCapFakeFn(fn, w, &fake); }
        catch (...) { return 1; }
    }
    return 1;
}

static int lvReadBackOk(void* w, const char* want, const char* tag)
{
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    const int match = (want && want[0] && std::strcmp(cap, want) == 0) ? 1 : 0;
    const int stillBlood = lvCapIsBlood(cap);
    (void)tag;
    (void)name;
    (void)vis;
    if (stillBlood)
        return 0;
    return match;
}

static int lvWriteAndReadBack(void* w, const char* text, int textBox, const char* tag)
{
    if (lvTryWriteCaptionOn(w, text, textBox))
        return 0;
    return lvReadBackOk(w, text, tag);
}

static int lvTryWriteCaption(void* w, const char* text)
{
    return lvTryWriteCaptionOn(w, text, 0);
}

static void* lvSehGetSubText(void* w)
{
    if (!lvHudMyGuiOk()) return nullptr;
    if (!g_getSubText || !w)
        return nullptr;
    void* t = nullptr;
    LV_TRY { t = g_getSubText(w); }
    LV_EXCEPT { t = nullptr; lvHudSehHit(w); }
    return t;
}

static int lvCallSetCaption(FnSetCaption fn, void* w, const char* text)
{
    if (!fn || !w || !text)
        return 1;
    if ((g_ustrCtorC || g_ustrCtorS) && g_ustrDtor)
    {
        char ustr[512];
        std::memset(ustr, 0, sizeof(ustr));
        int built = 1;
        try
        {
            if (g_ustrCtorC)
                built = lvSehUCtorC(ustr, text);
            else
            {
                GameStr gs;
                GameStrSet(&gs, text);
                built = lvSehUCtorS(ustr, &gs);
            }
        }
        catch (...) { built = 1; }
        if (built)
            return 1;
        int seh = 1;
        try { seh = lvSehSetCapFn(fn, w, ustr); }
        catch (...) { seh = 1; }
        try { lvSehUDtor(ustr); }
        catch (...) {}
        return seh;
    }
    struct FakeU
    {
        union { wchar_t sso[8]; wchar_t* ptr; } u;
        size_t size;
        size_t cap;
        char pad[224];
    };
    static wchar_t pool[4][64];
    static int pi = 0;
    FakeU fake;
    std::memset(&fake, 0, sizeof(fake));
    size_t n = std::strlen(text);
    if (n > 48) n = 48;
    wchar_t* slot = pool[pi++ & 3];
    for (size_t i = 0; i < n; ++i)
        slot[i] = (wchar_t)(unsigned char)text[i];
    slot[n] = 0;
    fake.u.ptr = slot;
    fake.size = n;
    fake.cap = 63;
    try { return lvSehSetCapFakeFn(fn, w, &fake); }
    catch (...) { return 1; }
}

static int lvTryWriteISubRaw(void* obj, const char* text)
{
    if (!obj || !text)
        return 1;
    if (g_setCapISub && lvCallSetCaption(g_setCapISub, obj, text) == 0)
        return 0;
    if (g_setCapEdit && lvCallSetCaption(g_setCapEdit, obj, text) == 0)
        return 0;
    return 1;
}

/* ISub on Datapanel and/or getSubWidgetText(LifeBar10). Not Widget::setCaption.
 * TextBox-on-Value is a different path and is not painted=1. */
static int lvWriteKeyISub(void* w, const char* text, const char* tag)
{
    char nm[96];
    nm[0] = 0;
    if (!lvWriteDestOk(w, nm, (int)sizeof(nm)))
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: refuse ISub dest name='%s'", nm);
        }
        return 0;
    }
    void* sub = lvSehGetSubText(w);
    if (sub)
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: getSubWidgetText %s parent=%p sub=%p — ISub/EditText on child",
                   tag ? tag : "?", w, sub);
        }
        lvTryWriteISubRaw(sub, text);
    }
    lvTryWriteISubRaw(w, text);
    /* Datapanel is PanelEmpty like LifeBar9. ISub only. Never TextBox here. */
    if (sub && lvReadBackOk(sub, text, tag))
        return 1;
    return lvReadBackOk(w, text, tag);
}

static int lvNameIsHemoGreen(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    return lvEndsWith(name, "HemolymphBarGreen") ? 1 : 0;
}

static int lvSehGetInt(FnGetInt fn, void* w)
{
    if (!lvHudMyGuiOk()) return 0;
    if (!fn || !w)
        return 0;
    int v = 0;
    LV_TRY { v = fn(w); }
    LV_EXCEPT { v = 0; lvHudSehHit(w); }
    return v;
}

static void* lvSehGetParent(void* w)
{
    if (!lvHudMyGuiOk()) return nullptr;
    if (!g_getParent || !w)
        return nullptr;
    void* p = nullptr;
    LV_TRY { p = g_getParent(w); }
    LV_EXCEPT { p = nullptr; lvHudSehHit(w); }
    return p;
}

static int lvSehGetCoord(FnGetCoordSret fn, void* w, LvIntCoord* out)
{
    if (!lvHudMyGuiOk()) return 0;
    if (!fn || !w || !out)
        return 0;
    std::memset(out, 0, sizeof(*out));
    LV_TRY { fn(out, w); }
    LV_EXCEPT { lvHudSehHit(w); return 0; }
    return 1;
}

/* LifeBar pixel W is 50–400. Pointer leftovers are ~1e9. */
static int lvPixelLooksReal(int w)
{
    return (w >= 50 && w <= 400) ? 1 : 0;
}

static int lvTakePackedSize(unsigned long long r, int* ow, int* oh)
{
    const int ww = (int)(r & 0xffffffffu);
    const int hh = (int)(r >> 32);
    if (!lvPixelLooksReal(ww))
        return 0;
    if (ow) *ow = ww;
    if (oh) *oh = (hh > 0 && hh < 200) ? hh : ww / 8;
    return 1;
}

static int lvReadPixelSize(void* w, int* ow, int* oh)
{
    if (ow) *ow = 0;
    if (oh) *oh = 0;
    if (!lvHudMyGuiOk() || !w)
        return 0;
    int ww = lvSehGetInt(g_getWidth, w);
    int hh = lvSehGetInt(g_getHeight, w);
    if (lvPixelLooksReal(ww))
    {
        if (ow) *ow = ww;
        if (oh) *oh = (hh > 0 && hh < 200) ? hh : ww / 8;
        return 1;
    }
    if (g_getSizeU64)
    {
        unsigned long long r = 0;
        LV_TRY { r = g_getSizeU64(w); }
        LV_EXCEPT { r = 0; lvHudSehHit(w); }
        if (lvTakePackedSize(r, ow, oh))
            return 1;
    }
    if (g_getCoordRef)
    {
        const int* p = nullptr;
        LV_TRY { p = g_getCoordRef(w); }
        LV_EXCEPT { p = nullptr; lvHudSehHit(w); }
        if (p)
        {
            int cw = 0, ch = 0;
            LV_TRY { cw = p[2]; ch = p[3]; }
            LV_EXCEPT { cw = 0; ch = 0; }
            if (lvPixelLooksReal(cw))
            {
                if (ow) *ow = cw;
                if (oh) *oh = (ch > 0 && ch < 200) ? ch : cw / 8;
                return 1;
            }
        }
    }
    LvIntCoord c;
    if (lvSehGetCoord(g_getCoord, w, &c) && lvPixelLooksReal(c.width))
    {
        if (ow) *ow = c.width;
        if (oh) *oh = (c.height > 0 && c.height < 200) ? c.height : c.width / 8;
        return 1;
    }
    if (lvSehGetCoord(g_getAbsCoord, w, &c) && lvPixelLooksReal(c.width))
    {
        if (ow) *ow = c.width;
        if (oh) *oh = (c.height > 0 && c.height < 200) ? c.height : c.width / 8;
        return 1;
    }
    if (g_getParentSize)
    {
        unsigned long long r = 0;
        LV_TRY { r = g_getParentSize(w); }
        LV_EXCEPT { r = 0; lvHudSehHit(w); }
        if (lvTakePackedSize(r, ow, oh))
            return 1;
    }
    return 0;
}

static int lvGreenBarOk(int parentW, int greenW, float fill01)
{
    if (parentW > 1000 || parentW < 50 || greenW <= 4)
        return 0;
    if (fill01 >= 0.5f && greenW * 2 < parentW)
        return 0;
    if (fill01 >= 0.9f && greenW * 5 < parentW * 4)
        return 0;
    return 1;
}

static int lvNameIsHemoHost(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    if (lvIsFillBarName(name))
        return 0;
    if (lvEndsWith(name, "HemolymphBarDatapanel") || lvEndsWith(name, "HemolymphValue")
     || lvEndsWith(name, "HemolymphTooltip") || lvEndsWith(name, "HemolymphBarGreen"))
        return 0;
    return lvEndsWith(name, "HemolymphBar") ? 1 : 0;
}

static int lvNameIsHost9(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    if (lvHasSub(name, "HemolymphBar") || lvIsFillBarName(name))
        return 0;
    if (lvEndsWith(name, "LifeBar9Datapanel") || lvEndsWith(name, "LifeBar9Value")
     || lvEndsWith(name, "LifeBar9Tooltip") || lvEndsWith(name, "LifeBar9Green"))
        return 0;
    return lvEndsWith(name, "LifeBar9") ? 1 : 0;
}

static int lvReadHostWh(void* bar, int* ow, int* oh)
{
    if (ow) *ow = 0;
    if (oh) *oh = 0;
    if (!bar)
        return 0;
    int pw = 0, ph = 0;
    if (!lvReadPixelSize(bar, &pw, &ph))
        pw = lvSehGetInt(g_getWidth, bar);
    if (pw < 50 || pw > 400)
        return 0;
    if (ow) *ow = pw;
    if (oh) *oh = ph;
    return 1;
}

/* Hunger px from layout fractions. Do not use getW=2. */
static const float kLayBackW = 0.92976588010787964f;
static const float kLayBar9W = 0.85611510276794434f;
static const float kLayBackH = 0.7469136118888855f;
static const float kLayBar9H = 0.095041319727897644f;
static const float kLayHungerW = 0.12395832622918707f;
static const float kLayHungerH = 0.02129629746523392f;

static int lvLayoutHungerPx(int* ow, int* oh)
{
    int hw = 0, hh = 0;
    const int mw = g_wMedicalPanel ? lvSehGetInt(g_getWidth, g_wMedicalPanel) : 0;
    const int mh = g_wMedicalPanel ? lvSehGetInt(g_getHeight, g_wMedicalPanel) : 0;
    const int bw = g_wBack ? lvSehGetInt(g_getWidth, g_wBack) : 0;
    const int bh = g_wBack ? lvSehGetInt(g_getHeight, g_wBack) : 0;
    const int rw = g_wRoot ? lvSehGetInt(g_getWidth, g_wRoot) : 0;
    const int rh = g_wRoot ? lvSehGetInt(g_getHeight, g_wRoot) : 0;
    if (mw >= 50 && mw <= 2000)
    {
        hw = (int)(mw * kLayBackW * kLayBar9W + 0.5f);
        if (mh >= 8 && mh <= 2000)
            hh = (int)(mh * kLayBackH * kLayBar9H + 0.5f);
    }
    if ((hw < 50 || hw > 400) && bw >= 50 && bw <= 2000)
    {
        hw = (int)(bw * kLayBar9W + 0.5f);
        if (bh >= 8 && bh <= 2000)
            hh = (int)(bh * kLayBar9H + 0.5f);
    }
    if ((hw < 50 || hw > 400) && rw >= 200 && rw <= 7680)
    {
        hw = (int)(rw * kLayHungerW + 0.5f);
        if (rh >= 200 && rh <= 4320)
            hh = (int)(rh * kLayHungerH + 0.5f);
    }
    if (hw < 50 || hw > 400)
        hw = 238; /* 1920 × Hunger screen w */
    if (hh < 8 || hh > 80)
        hh = (hw * 23) / 238;
    if (hh < 8)
        hh = 16;
    if (ow) *ow = hw;
    if (oh) *oh = hh;
    return 1;
}

/* HemolymphBarGreen only. Width = (hemo/max)*LifeBar10 HOST pixel width AFTER show.
 * Never Green / Datapanel as parentW. Never 1–9 Green. Wait if host < 50. */
static int lvFillGreen(float fill01, float hemo, float maxv)
{
    if (!lvHudMyGuiOk()) return 0;
    void* w = g_wHemoGreen;
    if (!w || (!g_setSizeHH && !g_setCoordHHHH))
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsHemoGreen(name))
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: refuse setSize dest name='%s' — HemolymphBarGreen only", name);
        }
        return 0;
    }
    void* bar = g_wHemo;
    char hname[96], hcap[96];
    int hvis = -1;
    hname[0] = 0;
    hcap[0] = 0;
    if (bar)
        lvReadWidget(bar, hname, (int)sizeof(hname), hcap, (int)sizeof(hcap), &hvis);
    if (!bar || !lvNameIsHemoHost(hname))
        return 0;
    int pw = 0, ph = 0;
    const int rawW = lvSehGetInt(g_getWidth, bar);
    int w9 = 0, h9 = 0;
    char n9[96], c9[96];
    int v9 = -1;
    n9[0] = 0;
    c9[0] = 0;
    if (g_wLifeBar9)
    {
        lvReadWidget(g_wLifeBar9, n9, (int)sizeof(n9), c9, (int)sizeof(c9), &v9);
        if (!lvReadPixelSize(g_wLifeBar9, &w9, &h9))
            w9 = lvSehGetInt(g_getWidth, g_wLifeBar9);
    }
    /* Size from a host whose getW is 50–400. Never size from 2. */
    if (rawW > 14 && rawW <= 400 && lvReadHostWh(bar, &pw, &ph))
    {
        /* LifeBar10 host is real */
    }
    else if (g_wLifeBar9 && lvNameIsHost9(n9) && w9 > 14 && w9 <= 400)
    {
        pw = w9;
        ph = h9;
        if (ph < 4)
            lvReadHostWh(g_wLifeBar9, &pw, &ph);
    }
    else
        lvLayoutHungerPx(&pw, &ph);
    if (pw > 1000)
        return 0;
    if (maxv < 1.f) maxv = 100.f;
    float useFill = (hemo > 0.f) ? (hemo / maxv) : fill01;
    if (useFill < 0.f) useFill = 0.f;
    if (useFill > 1.f) useFill = 1.f;
    if (pw >= 50 && pw <= 400 && ph < 8)
    {
        int dummy = 0, hh = 0;
        lvLayoutHungerPx(&dummy, &hh);
        ph = hh;
    }
    const int fillW = (pw >= 50 && pw <= 400) ? (int)(pw * useFill + 0.5f) : 0;
    const int nh = ph > 0 ? ph : 16;
    if (pw < 50 || pw > 400)
        return 0;
    /* Prefer setSize so parentW=272 writes fillW (setCoord left w=0 in 1.36). */
    if (g_setSizeHH)
    {
        LV_TRY { g_setSizeHH(w, fillW, nh); }
        LV_EXCEPT { lvHudSehHit(w); }
    }
    else if (g_setCoordHHHH)
    {
        LV_TRY { g_setCoordHHHH(w, 0, 0, fillW, nh); }
        LV_EXCEPT { lvHudSehHit(w); }
    }
    int got = 0, gh = 0;
    if (!lvReadPixelSize(w, &got, &gh))
        got = lvSehGetInt(g_getWidth, w);
    /* painted=0 only if green w≤4 AFTER the write */
    return (got > 4) ? 1 : 0;
}

static void lvSetVisibleHemo(void* w, int on);

static int lvNameIsData10(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    return lvEndsWith(name, "HemolymphBarDatapanel") ? 1 : 0;
}

/* If caption is Hemolymph but Dylan sees blank: vis/size/z-order, not a missing write.
 * Datapanel XML is 0 0.13 0.996 0.739 — not 0-size. Runtime show/size/depth only. */
static void lvEnsureDatapanelVisible()
{
    if (!lvHudMyGuiOk())
        return;
    void* w = g_wHemoData;
    if (!w)
        return;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsData10(name))
        return;
    int dw = 0, dh = 0;
    LvIntCoord c;
    if (lvSehGetCoord(g_getCoord, w, &c))
    {
        dw = c.width;
        dh = c.height;
    }
    if (!lvPixelLooksReal(dw))
        lvReadPixelSize(w, &dw, &dh);
    lvSetVisibleHemo(w, 1);
    /* Do NOT copy LifeBar9Datapanel if 0x0. Size to real host W × Hunger H. */
    int sw = 0, sh = 0, sl = 0, st = 0;
    if (g_wLifeBar9Data)
    {
        LvIntCoord c9;
        if (lvSehGetCoord(g_getCoord, g_wLifeBar9Data, &c9)
         && c9.width >= 50 && c9.width <= 400 && c9.height >= 4)
        {
            sl = c9.left;
            st = c9.top;
            sw = c9.width;
            sh = c9.height;
        }
        if (sw < 50)
        {
            int dw9 = 0, dh9 = 0;
            if (lvReadPixelSize(g_wLifeBar9Data, &dw9, &dh9) && dw9 >= 50 && dw9 <= 400)
            {
                sw = dw9;
                sh = dh9;
            }
        }
    }
    if (sw < 50 || sh < 4)
    {
        int bw = 0, bh = 0;
        int raw10 = g_wHemo ? lvSehGetInt(g_getWidth, g_wHemo) : 0;
        int raw9 = 0;
        if (g_wHemo && raw10 > 14 && raw10 <= 400)
            lvReadHostWh(g_wHemo, &bw, &bh);
        if (!lvPixelLooksReal(bw) && g_wLifeBar9)
        {
            char hn9[96], hc9[96];
            int hv9 = -1;
            hn9[0] = 0;
            hc9[0] = 0;
            lvReadWidget(g_wLifeBar9, hn9, (int)sizeof(hn9), hc9, (int)sizeof(hc9), &hv9);
            if (!lvReadPixelSize(g_wLifeBar9, &raw9, &bh))
                raw9 = lvSehGetInt(g_getWidth, g_wLifeBar9);
            if (lvNameIsHost9(hn9) && raw9 > 14 && raw9 <= 400)
            {
                bw = raw9;
                if (bh < 4)
                    lvReadHostWh(g_wLifeBar9, &bw, &bh);
            }
        }
        if (!lvPixelLooksReal(bw))
            lvLayoutHungerPx(&bw, &bh);
        if (bw >= 50 && bw <= 400)
        {
            sw = bw;
            sh = (bh >= 8 && bh <= 80) ? bh : (bw * 23) / 238;
            sl = 0;
            st = 0;
        }
    }
    if (sw >= 8 && sh >= 4 && (dw < 8 || dh < 4 || dw != sw || dh != sh)
     && (g_setCoordHHHH || g_setSizeHH))
    {
        if (g_setSizeHH)
        {
            LV_TRY { g_setSizeHH(w, sw, sh); }
            LV_EXCEPT { lvHudSehHit(w); }
        }
        else if (g_setCoordHHHH)
        {
            LV_TRY { g_setCoordHHHH(w, sl, st, sw, sh); }
            LV_EXCEPT { lvHudSehHit(w); }
        }
    }
}

static int lvNameIsDepthOk(const char* name)
{
    if (!name || !name[0] || lvIsForbiddenParent(name) || lvNameIsLifeBar1Strict(name))
        return 0;
    if (lvEndsWith(name, "HemolymphBarDatapanel") || lvEndsWith(name, "HemolymphValue")
     || lvEndsWith(name, "HemolymphTooltip") || lvEndsWith(name, "HemolymphBarGreen")
     || lvEndsWith(name, "HemolymphBar"))
        return 1;
    return 0;
}

/* Front Depth=0 covers last-child-of-Front. Raise HemolymphBar* above that. */
static void lvSetDepthHemo(void* w, int depth)
{
    if (!lvHudMyGuiOk()) return;
    if (!w || !g_setDepth)
        return;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsDepthOk(name))
        return;
    LV_TRY { g_setDepth(w, depth); }
    LV_EXCEPT { lvHudSehHit(w); }
}

static void lvRaiseHemoZ(void)
{
    /* Depth=1 — Root child after MedicalPanel, same as v1.33 look. */
    lvSetDepthHemo(g_wHemo, 1);
    lvSetDepthHemo(g_wHemoValue, 1);
    lvSetDepthHemo(g_wHemoData, 1);
    lvSetDepthHemo(g_wHemoTip, 1);
    lvSetDepthHemo(g_wHemoGreen, 1);
}

/* Show gate: name ends with one of the four exact tokens. Never parents / 1–9. */
static int lvNameIsShowOk(const char* name)
{
    if (!name || !name[0])
        return 0;
    if (lvNameIsLifeBar1Strict(name) || lvIsForbiddenParent(name))
        return 0;
    if (lvEndsWith(name, "HemolymphBarDatapanel"))
        return 1;
    if (lvEndsWith(name, "HemolymphValue"))
        return 1;
    if (lvEndsWith(name, "HemolymphBarGreen"))
        return 1;
    if (lvEndsWith(name, "HemolymphBar"))
        return 1;
    return 0;
}

/* Hide-only: LifeBar10 stun/hurt skins. Never Green. Never 1–9. */
static int lvNameIsStunSkin(const char* name)
{
    if (!name || !name[0] || lvNameIsLifeBar1Strict(name) || lvIsForbiddenParent(name))
        return 0;
    if (lvEndsWith(name, "HemolymphBarGreen"))
        return 0;
    return lvIsFillBarName(name);
}

static void lvSetVisibleHemo(void* w, int on)
{
    if (!lvHudMyGuiOk()) return;
    if (!w || !g_setVis)
        return;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsShowOk(name))
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: refuse setVisible dest name='%s' — LifeBar10 / Datapanel / Value / Green only",
                   name);
        }
        return;
    }
    LV_TRY { g_setVis(w, on ? 1 : 0); }
    LV_EXCEPT { lvHudSehHit(w); }
}

static void lvHideStunSkin(void* w)
{
    if (!lvHudMyGuiOk()) return;
    if (!w || !g_setVis)
        return;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsStunSkin(name))
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: refuse hide-skin dest name='%s' — LifeBar10 Grey/Red/Yellow/White/Robot/Crushed only",
                   name);
        }
        return;
    }
    LV_TRY { g_setVis(w, 0); }
    LV_EXCEPT { lvHudSehHit(w); }
}

static int lvReadVis(void* w)
{
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    if (!w)
        return -1;
    lvReadWidget(w, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    (void)name;
    (void)cap;
    return vis;
}

/* LifeBar10 + Datapanel + Value + Green. Hide Grey/Red/Yellow/White/Robot/Crushed.
 * Never Root / MedicalPanel / Back / Front / LifeBar1-9. */
static void lvShowHemo(int on)
{
    if (g_wHemo)
        lvSetVisibleHemo(g_wHemo, on);
    if (g_wHemoData)
        lvSetVisibleHemo(g_wHemoData, on);
    if (g_wHemoValue)
        lvSetVisibleHemo(g_wHemoValue, on);
    if (g_wHemoGreen)
        lvSetVisibleHemo(g_wHemoGreen, on);
    if (on)
    {
        lvHideStunSkin(g_wHemoGrey);
        lvHideStunSkin(g_wHemoRed);
        lvHideStunSkin(g_wHemoYellow);
        lvHideStunSkin(g_wHemoWhite);
        lvHideStunSkin(g_wHemoRobot);
        lvHideStunSkin(g_wHemoCrushed);
        lvRaiseHemoZ();
    }
}

static void lvLogVisAfterShow()
{
}

static int lvCapIsHudKey(const char* c)
{
    if (!c || !c[0])
        return 0;
    return (std::strcmp(c, "Hemolymph") == 0
         || std::strcmp(c, "Vigor") == 0
         || std::strcmp(c, "Battle-heat") == 0) ? 1 : 0;
}

static int lvHemoVisOk()
{
    if (!g_wHemo)
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(g_wHemo, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    if (!lvNameIsShowOk(name) || !lvEndsWith(name, "HemolymphBar") || lvEndsWith(name, "HemolymphBarGreen"))
        return 0;
    return vis == 1 ? 1 : 0;
}

static int lvHemoCapOk(void* dest)
{
    if (!dest)
        return 0;
    char name[96], cap[96];
    int vis = -1;
    name[0] = 0;
    cap[0] = 0;
    lvReadWidget(dest, name, (int)sizeof(name), cap, (int)sizeof(cap), &vis);
    (void)vis;
    if (lvCapIsBlood(cap) || !cap[0])
        return 0;
    return lvCapIsHudKey(cap);
}

static void lvRestoreCaption()
{
    /* Door / box / chair: hide/clear HemolymphBar* only. Never touch LifeBar1. */
    lvShowHemo(0);
    if (g_wHemoData && lvDestNameGated(g_wHemoData))
        lvWriteKeyISub(g_wHemoData, "", "clear-data");
    if (g_wHemoValue)
        lvTryWriteCaptionOn(g_wHemoValue, "", 1);
    if (g_wHemoGreen)
        lvFillGreen(0.f, 0.f, 100.f);
    g_wroteCaption = 0;
    g_wroteValue = 0;
}

static void lvCacheFound(void* w, const char* name, const char* cap)
{
    if (!w || !name || !name[0])
        return;
    /* Probe-only parents. Never setVisible / setCaption these. */
    if (lvEndsWith(name, "MedicalPanel_Back") && !g_wBack)
    {
        g_wBack = w;
        return;
    }
    if (lvEndsWith(name, "MedicalPanel_Front") && !g_wFront)
    {
        g_wFront = w;
        return;
    }
    if (lvEndsWith(name, "MedicalPanel") && !lvHasSub(name, "MedicalPanel_")
     && !g_wMedicalPanel)
    {
        g_wMedicalPanel = w;
        return;
    }
    if (lvEndsWith(name, "Root") && !g_wRoot)
    {
        g_wRoot = w;
        return;
    }
    if (lvNameIsHost9(name))
    {
        if (!g_wLifeBar9)
            g_wLifeBar9 = w;
        return;
    }
    if (lvEndsWith(name, "LifeBar9Datapanel") && !lvHasSub(name, "HemolymphBar")
     && !g_wLifeBar9Data)
    {
        g_wLifeBar9Data = w;
        return;
    }
    if (lvNameIsLifeBar1Strict(name))
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvLogf("LimbVigor: refuse cache LifeBar1 dest name='%s' — Blood stays Blood", name);
        }
        return;
    }
    if (lvNameIsValueWrite(name))
    {
        if (!g_wHemoValue)
        {
            g_wHemoValue = w;
            if (cap && cap[0])
                std::snprintf(g_origValue, sizeof(g_origValue), "%s", cap);
        }
        return;
    }
    if (lvEndsWith(name, "HemolymphTooltip") && !g_wHemoTip)
    {
        g_wHemoTip = w;
        return;
    }
    if (lvEndsWith(name, "HemolymphBarDatapanel"))
    {
        if (!g_wHemoData)
        {
            g_wHemoData = w;
            if (cap && cap[0])
                std::snprintf(g_origData, sizeof(g_origData), "%s", cap);
        }
        lvConsider(w, "cache-data");
        return;
    }
    if (lvNameIsStunSkin(name))
    {
        if (lvEndsWith(name, "HemolymphBarGrey") && !g_wHemoGrey)
            g_wHemoGrey = w;
        else if (lvEndsWith(name, "HemolymphBarRed") && !g_wHemoRed)
            g_wHemoRed = w;
        else if (lvEndsWith(name, "HemolymphBarYellow") && !g_wHemoYellow)
            g_wHemoYellow = w;
        else if (lvEndsWith(name, "HemolymphBarWhite") && !g_wHemoWhite)
            g_wHemoWhite = w;
        else if (lvEndsWith(name, "HemolymphBarRobot") && !g_wHemoRobot)
            g_wHemoRobot = w;
        else if (lvEndsWith(name, "HemolymphBarCrushed") && !g_wHemoCrushed)
            g_wHemoCrushed = w;
        return;
    }
    if (lvEndsWith(name, "HemolymphBarGreen") && !lvIsForbiddenParent(name))
    {
        if (!g_wHemoGreen)
            g_wHemoGreen = w;
        return;
    }
    if (lvEndsWith(name, "HemolymphBar")
     && !lvIsFillBarName(name) && !lvIsForbiddenParent(name))
    {
        if (!g_wHemo)
        {
            g_wHemo = w;
            g_capWidget = w;
            if (cap && cap[0])
                std::snprintf(g_capOrig, sizeof(g_capOrig), "%s", cap);
            g_capOrigHave = 1;
        }
        lvConsider(w, "cache-bar10");
    }
}

static void lvLogFindHit(const char* src, const char* asked, void* w, int logOnly)
{
    char nm[96], cap[96];
    int vis = -1;
    nm[0] = 0;
    cap[0] = 0;
    if (w)
        lvReadWidget(w, nm, (int)sizeof(nm), cap, (int)sizeof(cap), &vis);
    (void)src;
    (void)asked;
    (void)logOnly;
    if (w)
        lvCacheFound(w, nm, cap);
}

static void lvTryPrefixedFind(const char* shortName, void* root)
{
    if (!shortName || !shortName[0])
        return;
    const int logOnly = lvIsFillBarName(shortName) ? 1 : 0;
    char full[160];
    full[0] = 0;
    if (g_prefix[0])
        std::snprintf(full, sizeof(full), "%s%s", g_prefix, shortName);

    void* w = nullptr;
    if (g_prefix[0])
    {
        g_lastFindSeh = 0;
        try { w = lvFind3(shortName); }
        catch (...) { w = nullptr; g_lastFindSeh = 1; }
        lvLogFindSehOnce("findWidgetT3");
        lvLogFindHit("findWidgetT3", shortName, w, logOnly);

        w = nullptr;
        g_lastFindSeh = 0;
        try { w = lvFind2Full(full); }
        catch (...) { w = nullptr; g_lastFindSeh = 1; }
        lvLogFindSehOnce("findWidgetT2-full");
        lvLogFindHit("findWidgetT2", full, w, logOnly);
    }

    if (root)
    {
        w = nullptr;
        g_lastFindSeh = 0;
        try { w = lvRootFind(root, shortName); }
        catch (...) { w = nullptr; g_lastFindSeh = 1; }
        lvLogFindSehOnce("Root.findWidget");
        lvLogFindHit("Root.findWidget", shortName, w, logOnly);

        if (full[0])
        {
            w = nullptr;
            g_lastFindSeh = 0;
            try { w = lvRootFind(root, full); }
            catch (...) { w = nullptr; g_lastFindSeh = 1; }
            lvLogFindSehOnce("Root.findWidget-full");
            lvLogFindHit("Root.findWidget-full", full, w, logOnly);
        }
    }
}

static void lvResolveLifeBar()
{
    /* Prefixed find THIS tick only. Never _getWidget. Never reuse yesterday. */
    if (g_hudSkipTick)
        return;

    lvDumpMyGuiExports();
    void* bar = LvMainBar();
    void* med = LvMedicalPanel();
    g_barProven = LvMainBarProven(bar, med);
    lvReadBasePrefix(bar);
    (void)med;

    /* HemolymphBar* only. Do not hunt LifeBar1. Do not hunt LifeBar10. */

    void* root = nullptr;
    if (bar)
    {
        LV_TRY { std::memcpy(&root, (const char*)bar + 0x8, sizeof(root)); }
        LV_EXCEPT { root = nullptr; }
        if (root && !lvVtInMyGui(root))
            root = nullptr;
        if (root)
            g_wRoot = root;
    }
    g_resolveOnce = 1;

    lvTryPrefixedFind("MedicalPanel", root);
    lvTryPrefixedFind("MedicalPanel_Back", root);
    lvTryPrefixedFind("MedicalPanel_Front", root);
    lvTryPrefixedFind("LifeBar9", root);
    lvTryPrefixedFind("LifeBar9Datapanel", root);
    lvTryPrefixedFind("HemolymphBar", root);
    lvTryPrefixedFind("HemolymphBarDatapanel", root);
    lvTryPrefixedFind("HemolymphValue", root);
    lvTryPrefixedFind("HemolymphTooltip", root);
    lvTryPrefixedFind("HemolymphBarGreen", root);
    lvTryPrefixedFind("HemolymphBarGrey", root);
    lvTryPrefixedFind("HemolymphBarRed", root);
    lvTryPrefixedFind("HemolymphBarYellow", root);
    lvTryPrefixedFind("HemolymphBarWhite", root);
    lvTryPrefixedFind("HemolymphBarRobot", root);
    lvTryPrefixedFind("HemolymphBarCrushed", root);

    if (!g_capWidget && g_wHemoData)
        g_capWidget = g_wHemoData;
    if (!g_wHemoData || !lvDestNameGated(g_wHemoData))
        lvWhyOnce("LimbVigor: prefixed find found no HemolymphBarDatapanel dest — painted=0, no write");
    if (g_wHemo && g_wHemoData && g_wHemoValue && g_wHemoGreen)
        g_hudInvLogged = 0; /* next death can log once */
}

static void lvDumpMyGuiOnce()
{
    if (!LvWorldInGame() || g_hudSkipTick)
        return;
    lvResolveLifeBar();
}
#else
static int  lvHudBeginWrites(void)
{
    lvHudNullAll();
    return g_hudSkipTick ? 0 : 0;
}
static void lvDumpMyGuiOnce() {}
static void lvResolveLifeBar() {}
static void lvWhyOnce(const char* why) { (void)why; }
static int  lvTryWriteCaption(void* w, const char* t) { (void)w; (void)t; return 1; }
static int  lvWriteAndReadBack(void* w, const char* t, int tb, const char* tag)
{ (void)w; (void)t; (void)tb; (void)tag; return 0; }
static int  lvValueIsDigitBox(void* w) { (void)w; return 0; }
static void lvRestoreCaption() {}
static int  lvDestNameGated(void* w) { (void)w; return 0; }
static void lvShowHemo(int on) { (void)on; }
static void lvDumpMyGuiExports() {}
static void lvLogVisAfterShow() {}
static int  lvReadVis(void* w) { (void)w; return -1; }
static int  lvHemoVisOk() { return 0; }
static int  lvHemoCapOk(void* dest) { (void)dest; return 0; }
static int  lvWriteKeyISub(void* w, const char* t, const char* tag)
{ (void)w; (void)t; (void)tag; return 0; }
static int  lvFillGreen(float f, float h, float m) { (void)f; (void)h; (void)m; return 0; }
static void lvEnsureDatapanelVisible() {}
static int  lvCapIsHudKey(const char* c) { (void)c; return 0; }
static int  lvCaptionLooksDigit(const char* c) { (void)c; return 0; }
static int  lvCapIsBlood(const char* c) { (void)c; return 0; }
static void lvReadWidget(void* w, char* name, int nn, char* cap, int nc, int* vis)
{ (void)w; if (name && nn > 0) name[0] = 0; if (cap && nc > 0) cap[0] = 0; if (vis) *vis = -1; }
#endif

static int KeyEqI(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0 ? 1 : 0;
}

/* Title-Case Kenshi rows only. "left leg" is ours and must not match "Left Leg". */
static int IsReservedKey(const char* key)
{
    static const char* kRes[] = {
        "Blood", "Head", "Hunger",
        "Left Arm", "Right Arm", "Left Leg", "Right Leg",
        nullptr
    };
    if (!key || !key[0]) return 1;
    for (int i = 0; kRes[i]; ++i)
        if (std::strcmp(key, kRes[i]) == 0)
            return 1;
    return 0;
}

static int IsBannedHudKey(const char* key)
{
    static const char* kBan[] = {
        "Time", "Look", "How", "Need", "Regrowth", "Wait", nullptr
    };
    if (!key || !key[0]) return 1;
    for (int i = 0; kBan[i]; ++i)
        if (KeyEqI(key, kBan[i]))
            return 1;
    return 0;
}

void LvWalkSelPanel(DatapanelGUI* panel)
{
    if (!panel || !LvWorldInGame() || !lvHudBeginWrites())
        return;
    try
    {
        if (LvPanelHasGoalKeys(panel) && !g_hookReject)
            g_hookReject = (void*)panel;
        lvDumpMyGuiOnce();
    }
    catch (...)
    {
        LvNoteHudProbeSeh();
        LvErr("LimbVigor: GUI probe C++ throw — skip paint, growth continues");
    }
}

static void* lvCaptionDest()
{
    if (!LvHudWritesOk())
        return nullptr;
    void* dest = g_wHemoData;
    if (!dest)
        return nullptr;
    if (!lvDestNameGated(dest))
    {
        lvWhyOnce("LimbVigor: dest is not HemolymphBarDatapanel — not writing, painted=0 (LifeBar1 untouched)");
        return nullptr;
    }
    return dest;
}

void LvClearHud(DatapanelGUI* panel)
{
    (void)panel;
    /* Door / box / chair: clear HemolymphBar only. Never touch LifeBar1. */
    if (!lvHudBeginWrites())
        return;
    if (g_hudSkipTick || !g_wHemo)
        return;
    lvRestoreCaption();
}

void LvPaintHud(MedicalSystem* med, DatapanelGUI* panel, const CharSnap* snap)
{
    (void)panel;
    if (!snap || !LvCfg().enableHud || !LvWorldInGame())
        return;
    /* Find HemolymphBar* this tick. Do NOT skip because Paused/Options vis=1. */
    if (!lvHudBeginWrites())
        return;
    if (snap->race == RACE_ANIMAL)
    {
        LvClearHud(nullptr);
        return;
    }
    if (med && !LvIsPlayerSquad(LvCharFromMed(med)))
        return;

    /* Retry dump if ISub, setVis, or Green setSize/setCoord missing. setCapW=0 expected. */
#if !defined(LIMBVIGOR_IDE)
    if (!g_setCapISub || !g_setVis || (!g_setSizeHH && !g_setCoordHHHH))
        lvDumpMyGuiExports();
#endif

    /* After orig every selected-person tick. Show the just-found host. */
    if (g_hudSkipTick || !g_wHemo)
        return;
    lvShowHemo(1);
    if (g_hudSkipTick)
        return;
#if !defined(LIMBVIGOR_IDE)
    lvLogVisAfterShow();
#endif
    void* dest = lvCaptionDest();
    if (!dest)
    {
        if (!g_paintedOnce)
        {
            g_paintedOnce = 1;
            LvLog("LimbVigor: painted=0 (prefixed find found no HemolymphBarDatapanel dest)");
        }
        return;
    }

    const char* key1 = LvHudResourceKey(snap);
    if (!key1 || !key1[0] || IsReservedKey(key1) || IsBannedHudKey(key1)
     || !lvCapIsHudKey(key1))
    {
        LvClearHud(nullptr);
        return;
    }

    /* Host W × Hunger H. Do not copy LifeBar9Datapanel if 0x0. */
    lvEnsureDatapanelVisible();
    if (g_hudSkipTick)
        return;

    /* Label on Datapanel only. Value is the NUMBER. Never the word on Value. Never LifeBar1. */
    const int okData = lvWriteKeyISub(dest, key1, "HemolymphBarDatapanel");
    if (g_hudSkipTick)
        return;

    char num[32];
    num[0] = 0;
    float fill1 = 0.f;
    {
        char bar1[96], bar2[8], tip[8];
        float f2 = 0.f;
        bar1[0] = 0;
        LvHudLines(snap, bar1, (int)sizeof(bar1), &fill1, bar2, (int)sizeof(bar2), &f2, tip, (int)sizeof(tip));
        std::snprintf(num, sizeof(num), "%d", (int)snap->vigor);
    }

    int vok = 0;
    if (g_wHemoValue && num[0])
    {
        vok = lvWriteAndReadBack(g_wHemoValue, num, 1, "HemolymphValue");
        if (!vok)
            vok = lvWriteAndReadBack(g_wHemoValue, num, 2, "HemolymphValue");
        if (vok)
            g_wroteValue = 1;
    }

    int dataW = 0, dataH = 0;
#if !defined(LIMBVIGOR_IDE)
    {
        LvIntCoord dc;
        if (lvSehGetCoord(g_getCoord, dest, &dc))
        {
            dataW = dc.width;
            dataH = dc.height;
        }
        if (dataW <= 0 || dataH <= 0)
            lvReadPixelSize(dest, &dataW, &dataH);
    }
#endif
    const int dataOk = (dataW >= 8 && dataH >= 4) ? 1 : 0;

    const float maxv = LvCfg().maxVigor > 0.f ? LvCfg().maxVigor : 100.f;
    const int greenOk = lvFillGreen(fill1, snap->vigor, maxv);

    char vname[96], vcap[96];
    int vvis = -1;
    vname[0] = 0;
    vcap[0] = 0;
    if (g_wHemoValue)
        lvReadWidget(g_wHemoValue, vname, (int)sizeof(vname), vcap, (int)sizeof(vcap), &vvis);
    const int valOk = lvCaptionLooksDigit(vcap) && !lvCapIsHudKey(vcap) && !lvCapIsBlood(vcap);

    char dname[96], dcap[96];
    int dvis = -1;
    dname[0] = 0;
    dcap[0] = 0;
    lvReadWidget(dest, dname, (int)sizeof(dname), dcap, (int)sizeof(dcap), &dvis);
    const int visOk = lvHemoVisOk();
    const int capOk = (okData || lvHemoCapOk(dest)) && dcap[0] && !lvCapIsBlood(dcap);
    const int greyVis = lvReadVis(g_wHemoGrey);
    const int greyOk = (g_wHemoGrey && greyVis != 1) ? 1 : 0;
    if (!visOk || !capOk || !valOk || !greenOk || !dataOk || !greyOk)
    {
        static int once = 0;
        if (!once)
        {
            once = 1;
            LvErr("LimbVigor: painted=0 vis/data/value/green fail — will retry");
        }
        return;
    }
    g_wroteCaption = 1;

    if (!g_paintLogged)
    {
        g_paintLogged = 1;
        g_paintedOnce = 1;
    }
}

void LvHudInstall()
{
    LvLog("LimbVigor: HUD module lv_hud — HemolymphBar, LifeBar10 slot empty, re-find each paint");
}

void LvHudEnsureAfterInGame() {}
void LvHudHide() {}
void LvHudNote(const CharSnap* snap) { (void)snap; }
void LvHudPaint(const CharSnap* snap) { (void)snap; }
