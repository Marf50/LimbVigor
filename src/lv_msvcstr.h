#pragma once

// MSVC 2010 std::string layout. Kenshi and KenshiLib were built with that
// CRT. We never pass our own std::string into the game.

#include <cstring>
#include <cstddef>

struct GameStr
{
    union { char sso[16]; char* ptr; } u;
    size_t size;
    size_t cap;
};

inline void GameStrSet(GameStr* s, const char* text)
{
    std::memset(s, 0, sizeof(*s));
    if (!text) text = "";
    size_t n = std::strlen(text);
    if (n > 240) n = 240;
    if (n < 16)
    {
        std::memcpy(s->u.sso, text, n);
        s->size = n;
        s->cap = 15;
    }
    else
    {
        static char pool[32][256];
        static int pi = 0;
        char* slot = pool[pi++ % 32];
        std::memcpy(slot, text, n);
        slot[n] = 0;
        s->u.ptr = slot;
        s->size = n;
        s->cap = 255;
    }
}

inline int GameStrRead(const void* strObj, char* out, int outsz)
{
    if (!strObj || !out || outsz < 2) return 0;
    out[0] = 0;
    const char* p = (const char*)strObj;
    size_t size = 0, cap = 0;
    std::memcpy(&size, p + 16, sizeof(size));
    std::memcpy(&cap,  p + 24, sizeof(cap));
    if (size == 0 || size > 200 || cap > (size_t)1 << 20) return 0;
    const char* src = nullptr;
    if (cap > 15)
        std::memcpy(&src, p, sizeof(src));
    else
        src = p;
    if (!src) return 0;
    int n = (int)size;
    if (n >= outsz) n = outsz - 1;
    std::memcpy(out, src, (size_t)n);
    out[n] = 0;
    return n;
}

inline int GameStrContainsI(const void* strObj, const char* needle)
{
    char hay[128];
    if (!GameStrRead(strObj, hay, (int)sizeof(hay)) || !needle) return 0;
    for (char* h = hay; *h; ++h)
        if (*h >= 'A' && *h <= 'Z') *h = (char)(*h + 32);
    char ndl[64];
    int m = 0;
    for (; needle[m] && m < 63; ++m)
    {
        char c = needle[m];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        ndl[m] = c;
    }
    ndl[m] = 0;
    return std::strstr(hay, ndl) != nullptr ? 1 : 0;
}
