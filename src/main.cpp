








































 

#include "../include/cpptui.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>
#include <memory>
#include <functional>

using namespace cpptui;

#define CI_NAME "CodeInquisitor"
#define CI_AUTHOR "Catusse"
#define CI_VERSION "1.0.0"

 
 
 

static std::string sfmt(const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return std::string(buf);
}

static std::string lower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] >= 'A' && s[i] <= 'Z')
            s[i] = (char)(s[i] + 32);
    return s;
}

static bool endsWith(const std::string &s, const char *suf)
{
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

static std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
        a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
        b--;
    return s.substr(a, b - a);
}

static std::string stripQuotes(std::string s)
{
    s = trim(s);
    if (s.size() >= 2 && s[0] == '"' && s[s.size() - 1] == '"')
        s = s.substr(1, s.size() - 2);
    return s;
}

static std::string baseName(const std::string &p)
{
    size_t k = p.find_last_of("\\/");
    return (k == std::string::npos) ? p : p.substr(k + 1);
}

static bool fileExists(const std::string &p)
{
    if (p.empty())
        return false;
    DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string firstExistingFile(const std::string &text)
{
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '"')
        {
            size_t j = text.find('"', i + 1);
            if (j == std::string::npos)
                break;
            std::string t = stripQuotes(trim(text.substr(i + 1, j - i - 1)));
            if (fileExists(t))
                return t;
            i = j;
        }
    }
    std::string whole = stripQuotes(trim(text));
    if (fileExists(whole))
        return whole;
    std::vector<std::string> toks;
    {
        std::istringstream ss(text);
        std::string tok;
        while (ss >> tok)
            toks.push_back(tok);
    }
    for (size_t i = 0; i < toks.size(); i++)
    {
        std::string acc;
        for (size_t j = i; j < toks.size() && j < i + 12; j++)
        {
            if (j > i)
                acc += " ";
            acc += toks[j];
            std::string t = stripQuotes(acc);
            if (fileExists(t))
                return t;
        }
    }
    return "";
}

static std::string dropCandidate(const std::string &buf)
{
    if (buf.size() < 5 || buf.size() > 600)
        return "";
    std::string t = trim(buf);
    if (t.size() >= 2 && t.front() == '"')
    {
        size_t j = t.find('"', 1);
        if (j == std::string::npos)
            return "";
        std::string inner = t.substr(1, j - 1);
        return fileExists(inner) ? inner : "";
    }
    std::string u = stripQuotes(t);
    return fileExists(u) ? u : "";
}

struct CiFocusProbe : Widget
{
    static Widget *focused() { return focused_widget_; }
};

class CiFileBrowser : public Widget
{
public:
    std::function<void(const std::string &)> on_file_selected;

    CiFileBrowser()
    {
        focusable = true;
        tab_stop = true;
        reload();
    }

    struct Entry
    {
        bool dir;
        std::string name;
        std::string path;
    };

    std::filesystem::path curDir;
    std::vector<Entry> entries;
    int sel = 0;
    int scroll = 0;

    void reload()
    {
        entries.clear();
        sel = 0;
        scroll = 0;
        std::error_code ec;
        if (curDir.empty())
        {
#ifdef _WIN32
            DWORD mask = GetLogicalDrives();
            for (int c = 0; c < 26; c++)
            {
                if (!(mask & (1u << c)))
                    continue;
                std::string root = std::string(1, (char)('A' + c)) + ":\\";
                entries.push_back({true, root, root});
            }
#else
            for (char c = 'A'; c <= 'Z'; c++)
            {
                std::filesystem::path root(std::string(1, c) + ":\\");
                if (std::filesystem::exists(root, ec) && !ec)
                    entries.push_back({true, root.string(), root.string()});
            }
#endif
            return;
        }
        auto parent = curDir.parent_path();
        entries.push_back({true, "..", parent.empty() ? "" : parent.string()});
        std::vector<Entry> dirs, files;
        for (auto &de : std::filesystem::directory_iterator(curDir, ec))
        {
            if (ec)
                break;
            bool isDir = de.is_directory(ec);
            Entry e{isDir, de.path().filename().string(), de.path().string()};
            (isDir ? dirs : files).push_back(e);
        }
        auto byName = [](const Entry &a, const Entry &b)
        { return lower(a.name) < lower(b.name); };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(files.begin(), files.end(), byName);
        entries.insert(entries.end(), dirs.begin(), dirs.end());
        entries.insert(entries.end(), files.begin(), files.end());
    }

    std::string currentLabel() const
    {
        return curDir.empty() ? "This PC" : curDir.string();
    }

    void activate()
    {
        if (entries.empty() || sel < 0 || sel >= (int)entries.size())
            return;
        const Entry &e = entries[sel];
        if (e.dir)
        {
            curDir = e.path;
            reload();
        }
        else if (on_file_selected)
        {
            on_file_selected(e.path);
        }
    }

    void moveSel(int d)
    {
        if (entries.empty())
            return;
        sel += d;
        if (sel < 0)
            sel = 0;
        if (sel >= (int)entries.size())
            sel = (int)entries.size() - 1;
    }

    void pageMove(int d)
    {
        moveSel(d * std::max(1, height - 2));
    }

    void ensureVisible()
    {
        int vis = std::max(1, height - 1);
        if (sel < scroll)
            scroll = sel;
        if (sel >= scroll + vis)
            scroll = sel - vis + 1;
        int maxScroll = std::max(0, (int)entries.size() - vis);
        if (scroll > maxScroll)
            scroll = maxScroll;
        if (scroll < 0)
            scroll = 0;
    }

    static void drawText(Buffer &buffer, int x, int y, const std::string &s,
                         const Color &fg, const Color &bg, int clipW)
    {
        auto chars = TextHelper::prepare_text_for_render(s);
        int vx = 0;
        for (auto &ci : chars)
        {
            if (vx >= clipW)
                break;
            int sx = x + vx;
            if (sx >= 0 && sx < buffer.width() && y >= 0 && y < buffer.height())
            {
                Cell c;
                c.content = ci.content;
                c.fg_color = fg;
                c.bg_color = bg;
                buffer.set(sx, y, c);
                if (ci.display_width == 2 && sx + 1 < buffer.width())
                    buffer.set(sx + 1, y, Cell{"", fg, bg});
            }
            vx += ci.display_width;
        }
    }

    void render(Buffer &buffer) override
    {
        const Theme &t = Theme::current();
        Color bg = t.input_bg.is_default ? t.background : t.input_bg;
        buffer.push_clip({x, y, width, height});
        for (int j = 0; j < width; j++)
            for (int i = 0; i < height; i++)
            {
                Cell c;
                c.content = " ";
                c.bg_color = bg;
                buffer.set(x + j, y + i, c);
            }

        drawText(buffer, x, y, "  " + currentLabel(), t.secondary, bg, width);

        int vis = std::max(0, height - 1);
        for (int r = 0; r < vis; r++)
        {
            int idx = scroll + r;
            if (idx >= (int)entries.size())
                break;
            const Entry &e = entries[idx];
            bool isSel = (idx == sel);
            Color rowBg = isSel ? t.selection : bg;
            Color rowFg = isSel ? Color::contrast_color(rowBg) : t.foreground;
            for (int j = 0; j < width; j++)
            {
                Cell c;
                c.content = " ";
                c.bg_color = rowBg;
                buffer.set(x + j, y + 1 + r, c);
            }
            std::string label = e.name;
            if (e.dir && e.name != ".." && !endsWith(label, "\\") &&
                !endsWith(label, "/"))
                label += "/";
            Color fc = isSel ? rowFg : (e.dir ? t.primary : t.foreground);
            drawText(buffer, x + 2, y + 1 + r, label, fc, rowBg, width - 4);
        }

        if (!entries.empty() && (int)entries.size() > vis)
        {
            std::string ind = sfmt("%d/%d", sel + 1, (int)entries.size());
            drawText(buffer, x + width - (int)ind.size() - 1, y + height - 1,
                     ind, t.secondary, bg, (int)ind.size() + 1);
        }
        buffer.pop_clip();
    }

    bool on_event(const Event &event) override
    {
        if (event.is_key_event() && has_focus())
        {
            if (event.is_nav_up())
            {
                moveSel(-1);
            }
            else if (event.is_nav_down())
            {
                moveSel(1);
            }
            else if (event.is_nav_left())
            {
                if (!curDir.empty())
                {
                    curDir = curDir.parent_path();
                    reload();
                }
            }
            else if (event.is_nav_right() || event.is_activate())
            {
                activate();
            }
            else if (event.is_nav_pgup())
            {
                pageMove(-1);
            }
            else if (event.is_nav_pgdn())
            {
                pageMove(1);
            }
            else if (event.is_nav_home())
            {
                sel = 0;
            }
            else if (event.is_nav_end())
            {
                sel = (int)entries.size() - 1;
            }
            else
            {
                return false;
            }
            ensureVisible();
            return true;
        }
        if (event.is_mouse_event() && has_focus())
        {
            if (event.mouse_left() && event.click_count > 0)
            {
                int row = event.y - y - 1 + scroll;
                if (row >= 0 && row < (int)entries.size())
                {
                    sel = row;
                    ensureVisible();
                    if (event.click_count >= 2)
                        activate();
                    return true;
                }
            }
        }
        return false;
    }
};

class SplashWidget : public Widget
{
public:
    float progress = 0.0f;

    SplashWidget()
    {
        focusable = false;
    }

    static void text(Buffer &buffer, int x, int y, const std::string &s,
                     const Color &fg, const Color &bg)
    {
        auto chars = TextHelper::prepare_text_for_render(s);
        int vx = 0;
        for (auto &ci : chars)
        {
            if (x + vx < 0 || x + vx >= buffer.width() || y < 0 ||
                y >= buffer.height())
            {
                vx += ci.display_width;
                continue;
            }
            Cell c;
            c.content = ci.content;
            c.fg_color = fg;
            c.bg_color = bg;
            buffer.set(x + vx, y, c);
            if (ci.display_width == 2 && x + vx + 1 < buffer.width())
                buffer.set(x + vx + 1, y, Cell{"", fg, bg});
            vx += ci.display_width;
        }
    }

    static void styled(Buffer &buffer, int x, int y, const StyledText &st,
                       const Color &bg)
    {
        auto plain = st.plain_text();
        auto chars = TextHelper::prepare_text_for_render(plain);
        int vx = 0;
        size_t charIdx = 0;
        for (const auto &span : st.spans())
        {
            auto spanChars = TextHelper::prepare_text_for_render(span.text);
            Color fc = span.color.is_default ? Theme::current().foreground
                                             : span.color;
            for (size_t k = 0; k < spanChars.size(); k++)
            {
                if (charIdx >= chars.size())
                    break;
                if (x + vx >= 0 && x + vx < buffer.width() && y >= 0 &&
                    y < buffer.height())
                {
                    Cell c;
                    c.content = chars[charIdx].content;
                    c.fg_color = fc;
                    c.bg_color = bg;
                    c.bold = span.bold;
                    buffer.set(x + vx, y, c);
                    if (chars[charIdx].display_width == 2 &&
                        x + vx + 1 < buffer.width())
                        buffer.set(x + vx + 1, y, Cell{"", fc, bg});
                }
                vx += chars[charIdx].display_width;
                charIdx++;
            }
        }
    }

    void render(Buffer &buffer) override
    {
        const Theme &t = Theme::current();
        Color bg = t.background;
        for (int j = 0; j < buffer.width(); j++)
            for (int i = 0; i < buffer.height(); i++)
            {
                Cell c;
                c.content = " ";
                c.bg_color = bg;
                buffer.set(j, i, c);
            }

        int bw = std::min(52, buffer.width() - 4);
        int bh = 13;
        int bx = (buffer.width() - bw) / 2;
        int by = (buffer.height() - bh) / 2;
        if (bx < 0)
            bx = 0;
        if (by < 0)
            by = 0;
        if (bw < 10 || bh < 8)
            return;

        Color border = t.secondary;
        Color panel = t.panel_bg.is_default ? bg : t.panel_bg;
        for (int i = 0; i < bh; i++)
            for (int j = 0; j < bw; j++)
            {
                Cell c;
                c.content = " ";
                c.bg_color = panel;
                buffer.set(bx + j, by + i, c);
            }

        auto put = [&](int dx, int dy, const std::string &ch)
        {
            if (bx + dx >= 0 && bx + dx < buffer.width() && by + dy >= 0 &&
                by + dy < buffer.height())
            {
                Cell c;
                c.content = ch;
                c.fg_color = border;
                c.bg_color = panel;
                buffer.set(bx + dx, by + dy, c);
            }
        };
        for (int j = 1; j < bw - 1; j++)
        {
            put(j, 0, "\xE2\x95\x90");
            put(j, bh - 1, "\xE2\x95\x90");
        }
        for (int i = 1; i < bh - 1; i++)
        {
            put(0, i, "\xE2\x95\x91");
            put(bw - 1, i, "\xE2\x95\x91");
        }
        put(0, 0, "\xE2\x95\x94");
        put(bw - 1, 0, "\xE2\x95\x97");
        put(0, bh - 1, "\xE2\x95\x9A");
        put(bw - 1, bh - 1, "\xE2\x95\x9D");

        int cx = bx + bw / 2;
        styled(buffer, cx - 15, by + 2,
               StyledText().colored_bold("C O D E   I N Q U I S I T O R", t.primary),
               panel);
        text(buffer, cx - 24, by + 4,
             "v" CI_VERSION "  -  Windows PE static-analysis", t.secondary,
             panel);
        text(buffer, cx - 12, by + 5, "Developed by " CI_AUTHOR, t.secondary,
             panel);

        int barW = 28;
        int bx0 = cx - barW / 2 - 1;
        int filled = (int)(progress * barW + 0.5f);
        if (filled > barW)
            filled = barW;
        if (filled < 0)
            filled = 0;
        text(buffer, bx0, by + 7, "[", t.secondary, panel);
        for (int j = 0; j < barW; j++)
        {
            bool on = j < filled;
            text(buffer, bx0 + 1 + j, by + 7, on ? "\xE2\x96\x88" : "\xE2\x96\x91",
                 on ? t.primary : t.secondary, panel);
        }
        text(buffer, bx0 + 1 + barW, by + 7, "]", t.secondary, panel);
        char pct[8];
        snprintf(pct, sizeof(pct), "%3d%%", (int)(progress * 100.0f));
        text(buffer, bx0 + barW + 3, by + 7, pct, t.foreground, panel);

        text(buffer, cx - 15, by + 9, "Press any key to skip", t.secondary,
             panel);
        text(buffer, cx - 14, by + 11, "static analysis  -  never executed",
             t.secondary, panel);
    }
};

static std::string H32(uint32_t v) { return sfmt("0x%08X", (unsigned)v); }

static std::string H64(uint64_t v)
{
    unsigned hi = (unsigned)(v >> 32), lo = (unsigned)(v & 0xFFFFFFFFu);
    if (hi)
        return sfmt("0x%X%08X", hi, lo);
    return sfmt("0x%08X", lo);
}

static std::string sizeStr(uint64_t n)
{
    double v = (double)n;
    if (n >= 1024ull * 1024ull)
        return sfmt("%lu bytes (%.2f MB)", (unsigned long)n, v / (1024.0 * 1024.0));
    if (n >= 1024ull)
        return sfmt("%lu bytes (%.2f KB)", (unsigned long)n, v / 1024.0);
    return sfmt("%lu bytes", (unsigned long)n);
}

static std::string tsStr(uint32_t t)
{
    time_t tt = (time_t)t;
    struct tm g;
#ifdef _WIN32
    if (gmtime_s(&g, &tt) != 0)
        return "n/d";
#else
    if (!gmtime_r(&tt, &g))
        return "n/d";
#endif
    char b[64];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S UTC", &g);
    return std::string(b);
}

static bool parseHex(const std::string &in, uint64_t &out)
{
    std::string s = trim(in);
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s = s.substr(2);
    if (!s.empty() && (s[s.size() - 1] == 'h' || s[s.size() - 1] == 'H'))
        s = s.substr(0, s.size() - 1);
    if (s.empty() || s.size() > 16)
        return false;
    uint64_t v = 0;
    for (size_t i = 0; i < s.size(); i++)
    {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            return false;
        v = (v << 4) | (uint64_t)d;
    }
    out = v;
    return true;
}

 
 
 

struct Md5
{
    uint32_t a, b, c, d;
    uint64_t len;
    uint8_t buf[64];
    size_t bl;
    uint32_t K[64];

    static uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    void init()
    {
        a = 0x67452301u;
        b = 0xefcdab89u;
        c = 0x98badcfeu;
        d = 0x10325476u;
        len = 0;
        bl = 0;
        for (int i = 0; i < 64; i++)
            K[i] = (uint32_t)(fabs(sin((double)(i + 1))) * 4294967296.0);
    }

    void block(const uint8_t *p)
    {
        static const int S[64] = {
            7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
            5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = (uint32_t)p[4 * i] | ((uint32_t)p[4 * i + 1] << 8) |
                   ((uint32_t)p[4 * i + 2] << 16) | ((uint32_t)p[4 * i + 3] << 24);
        uint32_t A = a, B = b, Cc = c, D = d;
        for (int i = 0; i < 64; i++)
        {
            uint32_t F;
            int g;
            if (i < 16)
            {
                F = (B & Cc) | (~B & D);
                g = i;
            }
            else if (i < 32)
            {
                F = (D & B) | (~D & Cc);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                F = B ^ Cc ^ D;
                g = (3 * i + 5) % 16;
            }
            else
            {
                F = Cc ^ (B | ~D);
                g = (7 * i) % 16;
            }
            F = F + A + K[i] + M[g];
            A = D;
            D = Cc;
            Cc = B;
            B = B + rotl(F, S[i]);
        }
        a += A;
        b += B;
        c += Cc;
        d += D;
    }

    void update(const uint8_t *p, size_t n)
    {
        len += n;
        while (n)
        {
            size_t t = 64 - bl;
            if (t > n)
                t = n;
            memcpy(buf + bl, p, t);
            bl += t;
            p += t;
            n -= t;
            if (bl == 64)
            {
                block(buf);
                bl = 0;
            }
        }
    }

    std::string final()
    {
        uint64_t bits = len * 8;
        uint8_t z = 0x80;
        update(&z, 1);
        z = 0;
        while (bl != 56)
            update(&z, 1);
        uint8_t lb[8];
        for (int i = 0; i < 8; i++)
            lb[i] = (uint8_t)((bits >> (8 * i)) & 0xFF);
        update(lb, 8);
        uint32_t r[4] = {a, b, c, d};
        std::string s;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                s += sfmt("%02x", (unsigned)((r[i] >> (8 * j)) & 0xFF));
        return s;
    }
};

struct Sha256
{
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t bl;

    static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void init()
    {
        static const uint32_t iv[8] = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
        memcpy(h, iv, sizeof h);
        len = 0;
        bl = 0;
    }

    void block(const uint8_t *p)
    {
        static const uint32_t K[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
                   ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
        for (int i = 16; i < 64; i++)
        {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++)
        {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    void update(const uint8_t *p, size_t n)
    {
        len += n;
        while (n)
        {
            size_t t = 64 - bl;
            if (t > n)
                t = n;
            memcpy(buf + bl, p, t);
            bl += t;
            p += t;
            n -= t;
            if (bl == 64)
            {
                block(buf);
                bl = 0;
            }
        }
    }

    std::string final()
    {
        uint64_t bits = len * 8;
        uint8_t z = 0x80;
        update(&z, 1);
        z = 0;
        while (bl != 56)
            update(&z, 1);
        uint8_t lb[8];
        for (int i = 0; i < 8; i++)
            lb[i] = (uint8_t)((bits >> (56 - 8 * i)) & 0xFF);
        update(lb, 8);
        std::string s;
        for (int i = 0; i < 8; i++)
            s += sfmt("%08x", (unsigned)h[i]);
        return s;
    }
};

static std::string md5Of(const uint8_t *p, size_t n)
{
    Md5 m;
    m.init();
    m.update(p, n);
    return m.final();
}

static std::string sha256Of(const uint8_t *p, size_t n)
{
    Sha256 s;
    s.init();
    s.update(p, n);
    return s.final();
}

 
 
 

namespace pe
{

    enum
    {
        SEV_INFO = 0,
        SEV_LOW = 1,
        SEV_MED = 2,
        SEV_HIGH = 3
    };

    struct Section
    {
        char name[9];
        uint32_t vsize, vaddr, rawsize, rawptr, chars;
        double entropy;
        uint32_t chunkHigh, chunkTotal, maxChunkOff;
        double maxChunkEntropy;
    };

    struct ImportFn
    {
        std::string name;
        uint16_t hint;
        uint32_t ordinal;
        bool byOrd;
    };
    struct ImportDll
    {
        std::string dll;
        std::vector<ImportFn> fns;
        bool delay;
    };
    struct ExportFn
    {
        uint32_t ordinal, rva;
        std::string name, forward;
    };
    struct Dir
    {
        uint32_t rva, size;
    };
    struct Finding
    {
        int sev;
        std::string title, detail;
    };
    struct ApiHit
    {
        std::string dll, fn;
        int cat;
        int w;
    };

    enum
    {
        CAT_INJ,
        CAT_MEM,
        CAT_ANTIDBG,
        CAT_INPUT,
        CAT_PERS,
        CAT_NET,
        CAT_CRYPT,
        CAT_PRIV,
        CAT_EXEC,
        CAT_DYN,
        CAT_ENUM,
        CAT_COUNT
    };

    struct Info
    {
        std::string path, err;
        std::vector<uint8_t> d;
        bool loaded;

        uint32_t lfanew, optOff, csumOff;
        uint16_t machine, nsec, sizeOpt, chars, magic, subsystem, dllChars;
        bool is64;
        uint32_t timestamp, symPtr, nSyms, entry, sizeCode, sizeImage, sizeHdr, secAlign, fileAlign;
        uint32_t checksum, csumCalc, nDirs;
        uint64_t imageBase;
        uint8_t linkMaj, linkMin;
        uint16_t osMaj, osMin, subMaj, subMin;
        Dir dir[16];
        std::vector<Section> sec;

        std::vector<ImportDll> imports;
        uint32_t importFnCount, ordinalCount;
        std::string expName;
        uint32_t expBase, expFuncs, expNames;
        std::vector<ExportFn> exports;

        std::string md5, sha256, imphash, pdb;
        double fileEntropy, overlayEntropy;
        uint64_t overlayOff, overlaySize;
        int tlsCallbacks;
        bool hasSig, isNet, hasRich;
        int epSec;
        std::vector<Finding> findings;
        std::vector<ApiHit> apiHits;
        int score;

        Info() { reset(); }

        void reset()
        {
            path.clear();
            err.clear();
            d.clear();
            loaded = false;
            lfanew = optOff = csumOff = 0;
            machine = nsec = sizeOpt = chars = magic = subsystem = dllChars = 0;
            is64 = false;
            timestamp = symPtr = nSyms = entry = sizeCode = sizeImage = 0;
            sizeHdr = secAlign = fileAlign = 0;
            checksum = csumCalc = nDirs = 0;
            imageBase = 0;
            linkMaj = linkMin = 0;
            osMaj = osMin = subMaj = subMin = 0;
            memset(dir, 0, sizeof dir);
            sec.clear();
            imports.clear();
            importFnCount = ordinalCount = 0;
            expName.clear();
            expBase = expFuncs = expNames = 0;
            exports.clear();
            md5.clear();
            sha256.clear();
            imphash.clear();
            pdb.clear();
            fileEntropy = overlayEntropy = 0.0;
            overlayOff = overlaySize = 0;
            tlsCallbacks = 0;
            hasSig = isNet = hasRich = false;
            epSec = -1;
            findings.clear();
            apiHits.clear();
            score = 0;
        }
    };

    static Info P_global;
    static thread_local Info *activeP = &P_global;
#define P (*pe::activeP)

    static bool inb(uint64_t o, uint64_t l)
    {
        uint64_t n = P.d.size();
        return o <= n && l <= n - o;
    }

    static uint16_t u16(uint64_t o)
    {
        return inb(o, 2) ? (uint16_t)(P.d[(size_t)o] | (P.d[(size_t)o + 1] << 8)) : 0;
    }

    static uint32_t u32(uint64_t o)
    {
        if (!inb(o, 4))
            return 0;
        return (uint32_t)P.d[(size_t)o] |
               ((uint32_t)P.d[(size_t)o + 1] << 8) |
               ((uint32_t)P.d[(size_t)o + 2] << 16) |
               ((uint32_t)P.d[(size_t)o + 3] << 24);
    }

    static uint64_t u64(uint64_t o) { return (uint64_t)u32(o) | ((uint64_t)u32(o + 4) << 32); }

    static std::string cstr(uint64_t o, size_t maxl = 260)
    {
        std::string s;
        while (o < P.d.size() && s.size() < maxl)
        {
            uint8_t c = P.d[(size_t)o++];
            if (!c)
                break;
            s += (c >= 32 && c < 127) ? (char)c : '?';
        }
        return s;
    }

    static int secByRva(uint32_t rva)
    {
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            uint32_t span = s.vsize > s.rawsize ? s.vsize : s.rawsize;
            if (rva >= s.vaddr && (uint64_t)rva < (uint64_t)s.vaddr + span)
                return (int)i;
        }
        return -1;
    }

    static int secByOff(uint64_t off)
    {
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            if (s.rawsize && off >= s.rawptr && off < (uint64_t)s.rawptr + s.rawsize)
                return (int)i;
        }
        return -1;
    }

    static long long rva2off(uint32_t rva)
    {
        int i = secByRva(rva);
        if (i >= 0)
        {
            const Section &s = P.sec[i];
            uint32_t delta = rva - s.vaddr;
            if (delta < s.rawsize)
            {
                uint64_t o = (uint64_t)s.rawptr + delta;
                if (o < P.d.size())
                    return (long long)o;
            }
            return -1;
        }
        if (rva < P.sizeHdr && rva < P.d.size())
            return (long long)rva;
        return -1;
    }

    static long long off2rva(uint64_t off)
    {
        int i = secByOff(off);
        if (i >= 0)
            return (long long)(P.sec[i].vaddr + (off - P.sec[i].rawptr));
        if (off < P.sizeHdr)
            return (long long)off;
        return -1;
    }

    static double entropyOf(const uint8_t *p, size_t n)
    {
        if (!n)
            return 0.0;
        uint32_t cnt[256];
        memset(cnt, 0, sizeof cnt);
        for (size_t i = 0; i < n; i++)
            cnt[p[i]]++;
        double h = 0.0;
        for (int i = 0; i < 256; i++)
            if (cnt[i])
            {
                double q = (double)cnt[i] / (double)n;
                h -= q * log(q) * 1.4426950408889634;
            }
        return h;
    }

    static const char *entropyVerdict(double e)
    {
        if (e < 1.0)
            return "empty / repetitive";
        if (e < 5.0)
            return "low (data / text)";
        if (e < 6.8)
            return "normal (code / data)";
        if (e < 7.2)
            return "high (suspicious)";
        return "very high (packed / encrypted)";
    }

    static const char *machineName(uint16_t m)
    {
        switch (m)
        {
        case 0x014c:
            return "Intel 386 (x86)";
        case 0x8664:
            return "AMD64 (x64)";
        case 0x01c0:
            return "ARM";
        case 0x01c4:
            return "ARM Thumb-2 (ARMNT)";
        case 0xaa64:
            return "ARM64";
        case 0x0200:
            return "Intel Itanium (IA-64)";
        case 0x0ebc:
            return "EFI Byte Code";
        default:
            return "Unknown";
        }
    }

    static const char *subsysName(uint16_t s)
    {
        switch (s)
        {
        case 1:
            return "Native (driver)";
        case 2:
            return "Windows GUI";
        case 3:
            return "Windows Console (CUI)";
        case 5:
            return "OS/2 Console";
        case 7:
            return "POSIX Console";
        case 9:
            return "Windows CE GUI";
        case 10:
            return "EFI Application";
        case 11:
            return "EFI Boot Service Driver";
        case 12:
            return "EFI Runtime Driver";
        case 13:
            return "EFI ROM";
        case 14:
            return "Xbox";
        case 16:
            return "Windows Boot Application";
        default:
            return "Unknown";
        }
    }

    static const char *dirName(int i)
    {
        static const char *n[16] = {
            "Export", "Import", "Resource", "Exception", "Security (signature)", "Base Relocation",
            "Debug", "Architecture", "Global Ptr", "TLS", "Load Config", "Bound Import",
            "IAT", "Delay Import", "CLR (.NET)", "Reserved"};
        return n[i & 15];
    }

    static std::string permStr(uint32_t c)
    {
        std::string s;
        s += (c & 0x40000000u) ? 'R' : '-';
        s += (c & 0x80000000u) ? 'W' : '-';
        s += (c & 0x20000000u) ? 'X' : '-';
        return s;
    }

    static std::string charsStr(uint16_t c)
    {
        std::string s;
        if (c & 0x0001)
            s += "RELOCS_STRIPPED ";
        if (c & 0x0002)
            s += "EXECUTABLE_IMAGE ";
        if (c & 0x0004)
            s += "LINE_NUMS_STRIPPED ";
        if (c & 0x0008)
            s += "LOCAL_SYMS_STRIPPED ";
        if (c & 0x0020)
            s += "LARGE_ADDRESS_AWARE ";
        if (c & 0x0100)
            s += "32BIT_MACHINE ";
        if (c & 0x0200)
            s += "DEBUG_STRIPPED ";
        if (c & 0x1000)
            s += "SYSTEM ";
        if (c & 0x2000)
            s += "DLL ";
        return trim(s);
    }

    static std::string dllCharsStr(uint16_t c)
    {
        std::string s;
        if (c & 0x0020)
            s += "HIGH_ENTROPY_VA ";
        if (c & 0x0040)
            s += "DYNAMIC_BASE(ASLR) ";
        if (c & 0x0080)
            s += "FORCE_INTEGRITY ";
        if (c & 0x0100)
            s += "NX_COMPAT(DEP) ";
        if (c & 0x0200)
            s += "NO_ISOLATION ";
        if (c & 0x0400)
            s += "NO_SEH ";
        if (c & 0x0800)
            s += "NO_BIND ";
        if (c & 0x1000)
            s += "APPCONTAINER ";
        if (c & 0x2000)
            s += "WDM_DRIVER ";
        if (c & 0x4000)
            s += "GUARD_CF ";
        if (c & 0x8000)
            s += "TERMINAL_SERVER_AWARE ";
        return trim(s);
    }

    static const char *CAT_NAME[CAT_COUNT] = {
        "Process injection / manipulation", "Dynamic memory management", "Anti-debug / anti-analysis",
        "Input / keyboard-mouse hooks", "Persistence / services", "Network", "Cryptography",
        "Privileges / driver", "Process execution", "Dynamic API resolution", "Process enumeration"};

    struct ApiDef
    {
        const char *name;
        int cat;
        int w;
    };

    static const ApiDef API_TABLE[] = {
        {"VirtualAllocEx", CAT_INJ, 3}, {"WriteProcessMemory", CAT_INJ, 3}, {"CreateRemoteThread", CAT_INJ, 3}, {"CreateRemoteThreadEx", CAT_INJ, 3}, {"NtCreateThreadEx", CAT_INJ, 3}, {"RtlCreateUserThread", CAT_INJ, 3}, {"QueueUserAPC", CAT_INJ, 2}, {"NtQueueApcThread", CAT_INJ, 3}, {"SetThreadContext", CAT_INJ, 3}, {"NtMapViewOfSection", CAT_INJ, 2}, {"NtUnmapViewOfSection", CAT_INJ, 2}, {"ZwUnmapViewOfSection", CAT_INJ, 2}, {"NtWriteVirtualMemory", CAT_INJ, 3}, {"NtAllocateVirtualMemory", CAT_INJ, 2}, {"VirtualProtectEx", CAT_INJ, 2}, {"OpenProcess", CAT_INJ, 1}, {"ReadProcessMemory", CAT_INJ, 1}, {"NtReadVirtualMemory", CAT_INJ, 1}, {"VirtualAlloc", CAT_MEM, 1}, {"VirtualProtect", CAT_MEM, 1}, {"IsDebuggerPresent", CAT_ANTIDBG, 2}, {"CheckRemoteDebuggerPresent", CAT_ANTIDBG, 2}, {"NtQueryInformationProcess", CAT_ANTIDBG, 2}, {"ZwQueryInformationProcess", CAT_ANTIDBG, 2}, {"NtSetInformationThread", CAT_ANTIDBG, 2}, {"OutputDebugString", CAT_ANTIDBG, 1}, {"DebugActiveProcess", CAT_ANTIDBG, 2}, {"NtQuerySystemInformation", CAT_ANTIDBG, 1}, {"SetWindowsHookEx", CAT_INPUT, 2}, {"GetAsyncKeyState", CAT_INPUT, 1}, {"GetKeyboardState", CAT_INPUT, 1}, {"RegisterRawInputDevices", CAT_INPUT, 1}, {"GetRawInputData", CAT_INPUT, 1}, {"keybd_event", CAT_INPUT, 1}, {"mouse_event", CAT_INPUT, 1}, {"SendInput", CAT_INPUT, 1}, {"BlockInput", CAT_INPUT, 2}, {"RegSetValueEx", CAT_PERS, 1}, {"RegCreateKeyEx", CAT_PERS, 1}, {"CreateService", CAT_PERS, 2}, {"StartService", CAT_PERS, 1}, {"OpenSCManager", CAT_PERS, 1}, {"ChangeServiceConfig", CAT_PERS, 1}, {"InternetOpen", CAT_NET, 1}, {"InternetOpenUrl", CAT_NET, 1}, {"InternetConnect", CAT_NET, 1}, {"InternetReadFile", CAT_NET, 1}, {"URLDownloadToFile", CAT_NET, 2}, {"WinHttpOpen", CAT_NET, 1}, {"WinHttpConnect", CAT_NET, 1}, {"HttpSendRequest", CAT_NET, 1}, {"WSAStartup", CAT_NET, 1}, {"socket", CAT_NET, 1}, {"connect", CAT_NET, 1}, {"send", CAT_NET, 1}, {"recv", CAT_NET, 1}, {"gethostbyname", CAT_NET, 1}, {"CryptEncrypt", CAT_CRYPT, 1}, {"CryptDecrypt", CAT_CRYPT, 1}, {"CryptAcquireContext", CAT_CRYPT, 1}, {"CryptGenKey", CAT_CRYPT, 1}, {"BCryptEncrypt", CAT_CRYPT, 1}, {"BCryptDecrypt", CAT_CRYPT, 1}, {"AdjustTokenPrivileges", CAT_PRIV, 2}, {"NtLoadDriver", CAT_PRIV, 3}, {"NtUnloadDriver", CAT_PRIV, 3}, {"ZwLoadDriver", CAT_PRIV, 3}, {"NtSetSystemInformation", CAT_PRIV, 3}, {"DeviceIoControl", CAT_PRIV, 1}, {"LookupPrivilegeValue", CAT_PRIV, 1}, {"OpenProcessToken", CAT_PRIV, 1}, {"WinExec", CAT_EXEC, 2}, {"ShellExecute", CAT_EXEC, 1}, {"ShellExecuteEx", CAT_EXEC, 1}, {"CreateProcess", CAT_EXEC, 1}, {"CreateProcessAsUser", CAT_EXEC, 2}, {"system", CAT_EXEC, 1}, {"LoadLibrary", CAT_DYN, 1}, {"LoadLibraryEx", CAT_DYN, 1}, {"GetProcAddress", CAT_DYN, 1}, {"LdrLoadDll", CAT_DYN, 2}, {"LdrGetProcedureAddress", CAT_DYN, 2}, {"CreateToolhelp32Snapshot", CAT_ENUM, 1}, {"Process32First", CAT_ENUM, 1}, {"Process32Next", CAT_ENUM, 1}, {"Module32First", CAT_ENUM, 1}, {"EnumProcesses", CAT_ENUM, 1}};

    static const ApiDef *lookupApi(const std::string &name)
    {
        const size_t N = sizeof(API_TABLE) / sizeof(API_TABLE[0]);
        for (size_t i = 0; i < N; i++)
            if (name == API_TABLE[i].name)
                return &API_TABLE[i];
        if (name.size() > 2 && (name[name.size() - 1] == 'A' || name[name.size() - 1] == 'W'))
        {
            std::string b = name.substr(0, name.size() - 1);
            for (size_t i = 0; i < N; i++)
                if (b == API_TABLE[i].name)
                    return &API_TABLE[i];
        }
        return nullptr;
    }

    static const char *packerOf(const char *name)
    {
        struct PK
        {
            const char *n;
            const char *p;
        };
        static const PK T[] = {
            {"UPX0", "UPX"}, {"UPX1", "UPX"}, {"UPX2", "UPX"}, {"UPX!", "UPX"}, {".aspack", "ASPack"}, {".adata", "ASPack"}, {"ASPack", "ASPack"}, {".petite", "Petite"}, {".themida", "Themida/WinLicense"}, {".winlice", "WinLicense"}, {".vmp0", "VMProtect"}, {".vmp1", "VMProtect"}, {".vmp2", "VMProtect"}, {"MPRESS1", "MPRESS"}, {"MPRESS2", "MPRESS"}, {".nsp0", "NsPack"}, {".nsp1", "NsPack"}, {".nsp2", "NsPack"}, {"PEC2", "PECompact"}, {"pec1", "PECompact"}, {".enigma1", "Enigma Protector"}, {".enigma2", "Enigma Protector"}, {".boom", "The Boomerang"}, {".taz", "PESpin"}, {".y0da", "yoda's Protector"}, {"FSG!", "FSG"}, {".perplex", "Perplex PE-Protector"}, {".packed", "generic packer"}, {".RLPack", "RLPack"}, {"kkrunchy", "kkrunchy"}};
        std::string ln = lower(name);
        for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
            if (ln == lower(T[i].n))
                return T[i].p;
        return nullptr;
    }

    static bool isStdSection(const char *name)
    {
        static const char *S[] = {
            ".text", ".data", ".rdata", ".rsrc", ".reloc", ".idata", ".edata", ".pdata", ".xdata",
            ".bss", ".tls", ".crt", ".didat", ".gfids", ".00cfg", ".debug", ".drectve", ".sdata",
            ".voltbl", ".detourc", ".detourd", "code", "data", "bss", ".itext", ".eh_fram",
            ".rodata", "init", "page", "pagelk", "pagecons", "paged", ".retplne", ".gehcont",
            "fothk", ".cormeta", ".nep", ".edata"};
        std::string ln = lower(name);
        for (size_t i = 0; i < sizeof(S) / sizeof(S[0]); i++)
            if (ln == S[i])
                return true;
        return false;
    }

    static void addF(int sev, const std::string &t, const std::string &d)
    {
        Finding f;
        f.sev = sev;
        f.title = t;
        f.detail = d;
        P.findings.push_back(f);
    }

    static bool findingLess(const Finding &a, const Finding &b) { return a.sev > b.sev; }

    static uint32_t calcChecksum()
    {
        const std::vector<uint8_t> &d = P.d;
        size_t n = d.size();
        if (!n || (P.csumOff & 1))
            return 0;
        if ((size_t)P.csumOff + 4 > n)
            return 0;
        uint64_t sum = 0;
        for (size_t i = 0; i + 1 < n; i += 2)
        {
            if (i == (size_t)P.csumOff || i == (size_t)P.csumOff + 2)
                continue;
            sum += (uint32_t)(d[i] | (d[i + 1] << 8));
            if (sum > 0xFFFFFFFFull)
                sum = (sum & 0xFFFFFFFFull) + (sum >> 32);
        }
        if (n & 1)
            sum += d[n - 1];
        sum = (sum & 0xFFFF) + (sum >> 16);
        sum += (sum >> 16);
        sum &= 0xFFFF;
        return (uint32_t)(sum + (uint32_t)n);
    }

    static void readThunks(uint32_t thunkRva, uint32_t base, ImportDll &dll)
    {
        long long to = rva2off(thunkRva - base);
        if (to < 0)
            return;
        const uint32_t esz = P.is64 ? 8u : 4u;
        for (int j = 0; j < 30000; j++)
        {
            uint64_t eo = (uint64_t)to + (uint64_t)j * esz;
            if (!inb(eo, esz))
                break;
            uint64_t v = P.is64 ? u64(eo) : u32(eo);
            if (!v)
                break;
            ImportFn fn;
            fn.hint = 0;
            fn.ordinal = 0;
            bool ord = P.is64 ? (((v >> 63) & 1) != 0) : (((v >> 31) & 1) != 0);
            fn.byOrd = ord;
            if (ord)
            {
                fn.ordinal = (uint32_t)(v & 0xFFFF);
            }
            else
            {
                uint64_t addr = P.is64 ? (v & 0x7FFFFFFFFFFFFFFFull) : (uint64_t)(v & 0x7FFFFFFFu);
            uint32_t nameRva = (uint32_t)(addr - base);
                long long fo = rva2off(nameRva);
                if (fo >= 0)
                {
                    fn.hint = u16((uint64_t)fo);
                    fn.name = cstr((uint64_t)fo + 2, 256);
                }
                else
                    fn.name = "<invalid RVA>";
            }
            dll.fns.push_back(fn);
        }
    }

    static void parseImports()
    {
        long long off = P.dir[1].rva ? rva2off(P.dir[1].rva) : -1;
        if (off >= 0)
        {
            for (int i = 0; i < 2000; i++)
            {
                uint64_t o = (uint64_t)off + (uint64_t)i * 20;
                if (!inb(o, 20))
                    break;
                uint32_t oft = u32(o), nameRva = u32(o + 12), ft = u32(o + 16);
                if (!oft && !nameRva && !ft)
                    break;
                long long no = rva2off(nameRva);
                ImportDll dll;
                dll.delay = false;
                dll.dll = (no >= 0) ? cstr((uint64_t)no, 128) : std::string("<invalid name>");
                readThunks(oft ? oft : ft, 0, dll);
                P.importFnCount += (uint32_t)dll.fns.size();
                P.imports.push_back(dll);
            }
        }
        long long doff = P.dir[13].rva ? rva2off(P.dir[13].rva) : -1;
        if (doff >= 0)
        {
            for (int i = 0; i < 500; i++)
            {
                uint64_t o = (uint64_t)doff + (uint64_t)i * 32;
                if (!inb(o, 32))
                    break;
                uint32_t attrs = u32(o), nameV = u32(o + 4), intV = u32(o + 16);
                if (!attrs && !nameV && !intV)
                    break;
                uint32_t base = (attrs & 1) ? 0u : (uint32_t)P.imageBase;
                long long no = rva2off(nameV - base);
                ImportDll dll;
                dll.delay = true;
                dll.dll = (no >= 0) ? cstr((uint64_t)no, 128) : std::string("<invalid name>");
                readThunks(intV, base, dll);
                P.importFnCount += (uint32_t)dll.fns.size();
                P.imports.push_back(dll);
            }
        }
        for (size_t i = 0; i < P.imports.size(); i++)
            for (size_t j = 0; j < P.imports[i].fns.size(); j++)
                if (P.imports[i].fns[j].byOrd)
                    P.ordinalCount++;

        std::string joined;
        for (size_t i = 0; i < P.imports.size(); i++)
        {
            if (P.imports[i].delay)
                continue;
            std::string dn = lower(P.imports[i].dll);
            if (endsWith(dn, ".dll") || endsWith(dn, ".ocx") || endsWith(dn, ".sys"))
                dn = dn.substr(0, dn.size() - 4);
            for (size_t j = 0; j < P.imports[i].fns.size(); j++)
            {
                const ImportFn &f = P.imports[i].fns[j];
                if (!joined.empty())
                    joined += ",";
                joined += dn + "." + (f.byOrd ? sfmt("ord%u", (unsigned)f.ordinal) : lower(f.name));
            }
        }
        if (!joined.empty())
            P.imphash = md5Of((const uint8_t *)joined.data(), joined.size());
    }

    static bool exportLess(const ExportFn &a, const ExportFn &b) { return a.ordinal < b.ordinal; }

    static void parseExports()
    {
        long long off = P.dir[0].rva ? rva2off(P.dir[0].rva) : -1;
        if (off < 0 || !inb((uint64_t)off, 40))
            return;
        uint64_t o = (uint64_t)off;
        uint32_t nameRva = u32(o + 12);
        P.expBase = u32(o + 16);
        P.expFuncs = u32(o + 20);
        P.expNames = u32(o + 24);
        uint32_t fRva = u32(o + 28), nRva = u32(o + 32), oRva = u32(o + 36);
        long long no = rva2off(nameRva);
        if (no >= 0)
            P.expName = cstr((uint64_t)no, 128);
        if (P.expFuncs > 65536)
            P.expFuncs = 65536;
        if (P.expNames > 65536)
            P.expNames = 65536;
        long long fOff = rva2off(fRva), nOff = rva2off(nRva), oOff = rva2off(oRva);
        if (fOff < 0)
            return;

        uint32_t dStart = P.dir[0].rva, dEnd = P.dir[0].rva + P.dir[0].size;
        std::vector<bool> named(P.expFuncs, false);

        if (nOff >= 0 && oOff >= 0)
        {
            for (uint32_t i = 0; i < P.expNames; i++)
            {
                uint32_t nr = u32((uint64_t)nOff + 4ull * i);
                uint16_t ord = u16((uint64_t)oOff + 2ull * i);
                if (ord >= P.expFuncs)
                    continue;
                ExportFn e;
                e.ordinal = ord + P.expBase;
                e.rva = u32((uint64_t)fOff + 4ull * ord);
                long long sn = rva2off(nr);
                e.name = (sn >= 0) ? cstr((uint64_t)sn, 256) : std::string("<invalid name>");
                if (e.rva >= dStart && e.rva < dEnd)
                {
                    long long fo = rva2off(e.rva);
                    if (fo >= 0)
                        e.forward = cstr((uint64_t)fo, 256);
                }
                named[ord] = true;
                P.exports.push_back(e);
            }
        }
        for (uint32_t i = 0; i < P.expFuncs; i++)
        {
            uint32_t r = u32((uint64_t)fOff + 4ull * i);
            if (!r || named[i])
                continue;
            ExportFn e;
            e.ordinal = i + P.expBase;
            e.rva = r;
            if (r >= dStart && r < dEnd)
            {
                long long fo = rva2off(r);
                if (fo >= 0)
                    e.forward = cstr((uint64_t)fo, 256);
            }
            P.exports.push_back(e);
        }
        std::sort(P.exports.begin(), P.exports.end(), exportLess);
    }

    static void parseExtras()
    {
        long long toff = P.dir[9].rva ? rva2off(P.dir[9].rva) : -1;
        if (toff >= 0)
        {
            uint64_t cbVa = P.is64 ? u64((uint64_t)toff + 24) : u32((uint64_t)toff + 12);
            if (cbVa > P.imageBase)
            {
                long long co = rva2off((uint32_t)(cbVa - P.imageBase));
                if (co >= 0)
                {
                    const uint32_t esz = P.is64 ? 8u : 4u;
                    for (int i = 0; i < 64; i++)
                    {
                        uint64_t eo = (uint64_t)co + (uint64_t)i * esz;
                        if (!inb(eo, esz))
                            break;
                        uint64_t v = P.is64 ? u64(eo) : u32(eo);
                        if (!v)
                            break;
                        P.tlsCallbacks++;
                    }
                }
            }
        }

        long long doff = P.dir[6].rva ? rva2off(P.dir[6].rva) : -1;
        if (doff >= 0)
        {
            uint32_t cnt = P.dir[6].size / 28;
            if (cnt > 16)
                cnt = 16;
            for (uint32_t i = 0; i < cnt; i++)
            {
                uint64_t o = (uint64_t)doff + 28ull * i;
                if (!inb(o, 28))
                    break;
                uint32_t type = u32(o + 12), ptr = u32(o + 24);
                if (type == 2 && inb(ptr, 24) &&
                    P.d[ptr] == 'R' && P.d[ptr + 1] == 'S' && P.d[ptr + 2] == 'D' && P.d[ptr + 3] == 'S')
                {
                    P.pdb = cstr((uint64_t)ptr + 24, 260);
                    break;
                }
            }
        }

        P.hasSig = (P.dir[4].rva != 0 && P.dir[4].size != 0);
        P.isNet = (P.dir[14].rva != 0 && P.dir[14].size != 0);

        for (uint64_t i = 0x40; i + 4 <= (uint64_t)P.lfanew && i + 4 <= P.d.size(); i++)
            if (P.d[(size_t)i] == 'R' && P.d[(size_t)i + 1] == 'i' &&
                P.d[(size_t)i + 2] == 'c' && P.d[(size_t)i + 3] == 'h')
            {
                P.hasRich = true;
                break;
            }

        uint64_t maxEnd = P.sizeHdr;
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            if (!P.sec[i].rawsize)
                continue;
            uint64_t e = (uint64_t)P.sec[i].rawptr + P.sec[i].rawsize;
            if (e > maxEnd)
                maxEnd = e;
        }
        if (maxEnd < P.d.size())
        {
            P.overlayOff = maxEnd;
            P.overlaySize = P.d.size() - maxEnd;
            if (P.hasSig && (uint64_t)P.dir[4].rva >= maxEnd &&
                (uint64_t)P.dir[4].rva + P.dir[4].size <= P.d.size())
                P.overlaySize = (uint64_t)P.dir[4].rva - maxEnd;
            if (P.overlaySize)
                P.overlayEntropy = entropyOf(&P.d[(size_t)P.overlayOff], (size_t)P.overlaySize);
        }
    }

    static bool parsePE()
    {
        size_t n = P.d.size();
        if (n < 64)
        {
            P.err = "File too small to be a PE.";
            return false;
        }
        if (P.d[0] != 'M' || P.d[1] != 'Z')
        {
            P.err = "Missing 'MZ' signature: not a Windows executable (PE).";
            return false;
        }
        P.lfanew = u32(0x3C);
        if (!inb(P.lfanew, 24) || u32(P.lfanew) != 0x00004550u)
        {
            P.err = "'PE\\0\\0' signature not found: DOS/NE/LE file or corrupted header.";
            return false;
        }
        uint64_t c = (uint64_t)P.lfanew + 4;
        P.machine = u16(c);
        P.nsec = u16(c + 2);
        P.timestamp = u32(c + 4);
        P.symPtr = u32(c + 8);
        P.nSyms = u32(c + 12);
        P.sizeOpt = u16(c + 16);
        P.chars = u16(c + 18);

        P.optOff = P.lfanew + 24;
        if (P.sizeOpt < 96 || !inb(P.optOff, P.sizeOpt))
        {
            P.err = "Missing or truncated optional header.";
            return false;
        }
        uint64_t o = P.optOff;
        P.magic = u16(o);
        if (P.magic == 0x10B)
            P.is64 = false;
        else if (P.magic == 0x20B)
            P.is64 = true;
        else
        {
            P.err = sfmt("Unknown optional header magic (0x%04X)", (unsigned)P.magic);
            return false;
        }

        P.linkMaj = P.d[(size_t)o + 2];
        P.linkMin = P.d[(size_t)o + 3];
        P.sizeCode = u32(o + 4);
        P.entry = u32(o + 16);
        P.imageBase = P.is64 ? u64(o + 24) : (uint64_t)u32(o + 28);
        P.secAlign = u32(o + 32);
        P.fileAlign = u32(o + 36);
        P.osMaj = u16(o + 40);
        P.osMin = u16(o + 42);
        P.subMaj = u16(o + 48);
        P.subMin = u16(o + 50);
        P.sizeImage = u32(o + 56);
        P.sizeHdr = u32(o + 60);
        P.csumOff = P.optOff + 64;
        P.checksum = u32(o + 64);
        P.subsystem = u16(o + 68);
        P.dllChars = u16(o + 70);

        uint64_t ndOff = o + (P.is64 ? 108 : 92), dirOff = o + (P.is64 ? 112 : 96);
        uint32_t nd = u32(ndOff);
        uint32_t avail = (P.sizeOpt > (dirOff - o)) ? (uint32_t)((P.sizeOpt - (dirOff - o)) / 8) : 0;
        if (nd > 16)
            nd = 16;
        if (nd > avail)
            nd = avail;
        P.nDirs = nd;
        for (uint32_t i = 0; i < nd; i++)
        {
            P.dir[i].rva = u32(dirOff + 8ull * i);
            P.dir[i].size = u32(dirOff + 8ull * i + 4);
        }

        uint32_t ns = P.nsec;
        if (ns > 96)
            ns = 96;
        uint64_t so = o + P.sizeOpt;
        for (uint32_t i = 0; i < ns; i++)
        {
            uint64_t s = so + 40ull * i;
            if (!inb(s, 40))
                break;
            Section sec;
            memset(&sec, 0, sizeof sec);
            for (int k = 0; k < 8; k++)
            {
                uint8_t ch = P.d[(size_t)s + k];
                if (!ch)
                    break;
                sec.name[k] = (ch >= 32 && ch < 127) ? (char)ch : '?';
            }
            sec.vsize = u32(s + 8);
            sec.vaddr = u32(s + 12);
            sec.rawsize = u32(s + 16);
            sec.rawptr = u32(s + 20);
            sec.chars = u32(s + 36);
            if (sec.rawptr < n && sec.rawsize)
            {
                size_t len = sec.rawsize;
                if (len > n - sec.rawptr)
                    len = n - sec.rawptr;
                sec.entropy = entropyOf(&P.d[sec.rawptr], len);
            }
            P.sec.push_back(sec);
        }
        P.epSec = P.entry ? secByRva(P.entry) : -1;
        return true;
    }

     

    static void checkSectionOverlap()
    {
        int reported = 0;
        for (size_t i = 0; i < P.sec.size() && reported < 12; i++)
            for (size_t j = i + 1; j < P.sec.size() && reported < 12; j++)
            {
                const Section &a = P.sec[i];
                const Section &b = P.sec[j];
                uint64_t aSpan = (uint64_t)a.vaddr + (a.vsize > a.rawsize ? a.vsize : a.rawsize);
                uint64_t bSpan = (uint64_t)b.vaddr + (b.vsize > b.rawsize ? b.vsize : b.rawsize);
                if (a.vsize && b.vsize && (uint64_t)a.vaddr < bSpan && (uint64_t)b.vaddr < aSpan)
                {
                    addF(SEV_HIGH, sfmt("Sections overlapping in memory: %s / %s", a.name, b.name),
                         sfmt("RVA ranges %s-%s and %s-%s overlap: the second mapping covers the first.",
                              H32(a.vaddr).c_str(), H32((uint32_t)aSpan).c_str(),
                              H32(b.vaddr).c_str(), H32((uint32_t)bSpan).c_str()));
                    reported++;
                }
                if (a.rawsize && b.rawsize)
                {
                    uint64_t aEnd = (uint64_t)a.rawptr + a.rawsize, bEnd = (uint64_t)b.rawptr + b.rawsize;
                    if ((uint64_t)a.rawptr < bEnd && (uint64_t)b.rawptr < aEnd)
                    {
                        addF(SEV_MED, sfmt("On-disk data overlapping: %s / %s", a.name, b.name),
                             "Both sections point to the same file bytes: one of them is malformed.");
                        reported++;
                    }
                }
            }
    }

    static void checkAlignment()
    {
        uint32_t sa = P.secAlign ? P.secAlign : 0x1000;
        uint32_t fa = P.fileAlign ? P.fileAlign : 0x200;
        if (sa & (sa - 1))
            addF(SEV_LOW, sfmt("SectionAlignment is not a power of two (%s)", H32(sa).c_str()), "");
        if (fa & (fa - 1))
            addF(SEV_LOW, sfmt("FileAlignment is not a power of two (%s)", H32(fa).c_str()), "");
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            if (s.vaddr % sa)
                addF(SEV_LOW, sfmt("VirtualAddress not aligned in %s (%s)", s.name, H32(s.vaddr).c_str()),
                     sfmt("Not a multiple of SectionAlignment %s: malformed section.", H32(sa).c_str()));
            if (s.rawsize && (s.rawptr % fa))
                addF(SEV_MED, sfmt("PointerToRawData not aligned in %s (%s)", s.name, H32(s.rawptr).c_str()),
                     sfmt("Not a multiple of FileAlignment %s: some loaders reject the file.", H32(fa).c_str()));
            if (s.rawsize && s.rawptr && s.rawptr < P.sizeHdr)
                addF(SEV_MED, sfmt("Section %s inside the headers (%s)", s.name, H32(s.rawptr).c_str()),
                     sfmt("PointerToRawData falls before SizeOfHeaders (%s): data overlaps PE structures.",
                          H32(P.sizeHdr).c_str()));
            if ((uint64_t)s.vaddr + s.vsize > (uint64_t)P.sizeImage && P.sizeImage)
                addF(SEV_MED, sfmt("Section %s beyond SizeOfImage", s.name),
                     sfmt("Virtual end %s > SizeOfImage %s: memory not reserved by the loader.",
                          H32((uint32_t)((uint64_t)s.vaddr + s.vsize)).c_str(), H32(P.sizeImage).c_str()));
        }
    }

    static void checkDuplicateNames()
    {
        int reported = 0;
        for (size_t i = 0; i < P.sec.size() && reported < 8; i++)
            for (size_t j = i + 1; j < P.sec.size() && reported < 8; j++)
                if (!strcmp(P.sec[i].name, P.sec[j].name))
                {
                    addF(SEV_LOW, sfmt("Duplicate section names: %s", P.sec[i].name),
                         "Sections sharing the same name make identification ambiguous.");
                    reported++;
                }
    }

    static void checkGaps()
    {
        std::vector<size_t> idx;
        for (size_t i = 0; i < P.sec.size(); i++)
            if (P.sec[i].rawsize)
                idx.push_back(i);
        if (idx.size() < 2)
            return;
        std::sort(idx.begin(), idx.end(),
                  [](size_t a, size_t b)
                  { return P.sec[a].rawptr < P.sec[b].rawptr; });

        int reported = 0;
        for (size_t k = 0; k + 1 < idx.size() && reported < 8; k++)
        {
            const Section &a = P.sec[idx[k]];
            const Section &b = P.sec[idx[k + 1]];
            uint64_t gapStart = (uint64_t)a.rawptr + a.rawsize;
            uint64_t gapEnd = b.rawptr;
            if (gapEnd <= gapStart)
                continue;
            uint64_t len = gapEnd - gapStart;
            if (len < 0x200)
                continue;
            if (gapStart >= P.d.size())
                continue;
            uint64_t limit = gapEnd < P.d.size() ? gapEnd : P.d.size();
            uint64_t nz = 0;
            for (uint64_t i = gapStart; i < limit && i - gapStart < 8192; i++)
                if (P.d[(size_t)i])
                    nz++;
            if (!nz)
                continue;
            addF(SEV_MED, sfmt("Unreferenced data between %s and %s (%lu bytes)", a.name, b.name, (unsigned long)len),
                 sfmt("Range %s-%s outside every section with %lu non-null bytes: room for hidden payloads.",
                      H32((uint32_t)gapStart).c_str(), H32((uint32_t)gapEnd).c_str(), (unsigned long)nz));
            reported++;
        }
    }

    static void checkOrdinalImports()
    {
        if (!P.ordinalCount)
            return;
        std::string worst;
        int worstN = 0;
        for (size_t i = 0; i < P.imports.size(); i++)
        {
            int n = 0;
            for (size_t j = 0; j < P.imports[i].fns.size(); j++)
                if (P.imports[i].fns[j].byOrd)
                    n++;
            if (n > worstN)
            {
                worstN = n;
                worst = P.imports[i].dll;
            }
        }
        if (worstN >= 3)
            addF(SEV_MED, sfmt("Ordinal imports: %d functions from %s", worstN, worst.c_str()),
                 "The function name does not appear in the file: used by malware to hide APIs.");
        if (P.importFnCount && P.ordinalCount * 10 >= P.importFnCount * 3)
            addF(SEV_MED, sfmt("%u of %u imports are by ordinal (%.0f%%)", (unsigned)P.ordinalCount, (unsigned)P.importFnCount, 100.0 * (double)P.ordinalCount / (double)P.importFnCount),
                 "Very high share of anonymous imports: the import table loses its informative value.");
    }

    static void checkDllNames()
    {
        int reported = 0;
        for (size_t i = 0; i < P.imports.size() && reported < 20; i++)
        {
            const std::string &d = P.imports[i].dll;
            std::string ld = lower(d);
            if (d.find('\\') != std::string::npos || d.find('/') != std::string::npos)
            {
                addF(SEV_LOW, "Path in DLL name: " + d,
                     "Import APIs expect a name without a path.");
                reported++;
            }
            if (ld.find(".dll") == std::string::npos && ld.find(".ocx") == std::string::npos &&
                ld.find(".sys") == std::string::npos && d.find("invalid name") == std::string::npos)
            {
                addF(SEV_LOW, "DLL without extension: " + d, "The loader appends .dll, but the name is anomalous.");
                reported++;
            }
            if (d.size() <= 3 && d.find("invalid name") == std::string::npos)
            {
                addF(SEV_LOW, "Very short DLL name: " + d, "");
                reported++;
            }
            if (d.find('?') != std::string::npos)
            {
                addF(SEV_LOW, "Non-printable characters in DLL name: " + d, "");
                reported++;
            }
        }
        reported = 0;
        for (size_t i = 0; i < P.imports.size() && reported < 8; i++)
            for (size_t j = i + 1; j < P.imports.size() && reported < 8; j++)
                if (lower(P.imports[i].dll) == lower(P.imports[j].dll))
                {
                    addF(SEV_LOW, "DLL imported more than once: " + P.imports[i].dll,
                         "The same library appears in multiple descriptors.");
                    reported++;
                }
    }

    static void checkImportSection()
    {
        if (!P.dir[1].rva)
            return;
        int k = secByRva(P.dir[1].rva);
        if (k < 0)
            return;
        std::string n = lower(P.sec[(size_t)k].name);
        if (n != ".idata" && n != ".text" && n != ".rdata")
            addF(SEV_LOW, sfmt("Import directory in unexpected section (%s)", P.sec[(size_t)k].name),
                 "Imports normally live in .idata or .rdata.");
    }

    static void checkStub()
    {
        if (P.lfanew <= 64 || P.lfanew > P.d.size())
            return;
        size_t len = (size_t)P.lfanew - 64;
        if (len > 512)
            len = 512;
        if (!len || !inb(64, len))
            return;
        std::string stub((const char *)&P.d[64], len);
        if (stub.find("This program cannot be run in DOS mode") == std::string::npos)
            addF(SEV_LOW, "Non-standard DOS stub",
                 "Missing the standard MSVC linker message: replaced stub or built with different tools.");
        if (P.lfanew > 0x200)
            addF(SEV_LOW, sfmt("Very large DOS stub (%u bytes)", (unsigned)(P.lfanew - 64)),
                 "Space between stub and PE header above normal: possible embedded code or data.");
    }

    static void checkRich()
    {
        if (P.hasRich || P.subsystem == 1 || P.isNet)
            return;
        if (P.machine != 0x014c && P.machine != 0x8664)
            return;
        addF(SEV_LOW, "Missing Rich header",
             "The MSVC linker 'Rich' marker was removed (typical of packers and manual stripping).");
    }

    static void checkTimestampSignature()
    {
        if (P.timestamp == 0x2A425E19)
            addF(SEV_LOW, "Typical Delphi/Borland timestamp",
                 "Constant value emitted by Borland compilers: useful for toolchain attribution.");
    }

    static void checkEntryBounds()
    {
        if (P.epSec < 0 || !P.entry)
            return;
        const Section &s = P.sec[(size_t)P.epSec];
        if (s.vsize && (uint64_t)(P.entry - s.vaddr) >= s.vsize)
            addF(SEV_HIGH, "Entry point beyond section end",
                 sfmt("EP %s exceeds VirtualSize %s by %s: execution outside mapped data.",
                      H32(P.entry).c_str(), H32(s.vsize).c_str(),
                      H32((uint32_t)(P.entry - s.vaddr - s.vsize)).c_str()));
    }

    static void analyzeChunks()
    {
        const uint32_t CH = 4096;
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            Section &s = P.sec[i];
            s.chunkHigh = s.chunkTotal = s.maxChunkOff = 0;
            s.maxChunkEntropy = 0.0;
            if (s.rawsize < CH * 2)
                continue;
            size_t start = s.rawptr, len = s.rawsize;
            if (start >= P.d.size())
                continue;
            if (start + len > P.d.size())
                len = P.d.size() - start;
            for (size_t off = 0; off + CH <= len; off += CH)
            {
                double e = entropyOf(&P.d[start + off], CH);
                s.chunkTotal++;
                if (e >= 7.2)
                {
                    s.chunkHigh++;
                    if (e > s.maxChunkEntropy)
                    {
                        s.maxChunkEntropy = e;
                        s.maxChunkOff = (uint32_t)(start + off);
                    }
                }
            }
            if (!s.chunkTotal)
                continue;
            double ratio = (double)s.chunkHigh / (double)s.chunkTotal;
            bool rsrc = (lower(s.name) == ".rsrc");
            if (ratio >= 0.25 && ratio < 0.90 && s.entropy < 7.2 && !rsrc)
                addF(SEV_HIGH, sfmt("Encrypted content embedded in %s (%.0f%% of blocks)", s.name, ratio * 100.0),
                     sfmt("%u of %u 4 KB blocks exceed 7.2 bits/byte, peak %.2f at offset %s, while the "
                          "section stays below 7.2: encrypted tail or insert, typical pattern of an appended payload.",
                          s.chunkHigh, s.chunkTotal, s.maxChunkEntropy, H32(s.maxChunkOff).c_str()));
            else if (ratio >= 0.90 && s.entropy < 6.5 && !rsrc)
                addF(SEV_MED, sfmt("Section %s: uniformly high-entropy blocks (%.0f%%)", s.name, ratio * 100.0),
                     "Blocks exceed 7.2 but the section average is lower: block-wise compression.");
        }
    }

    static void analyze()
    {
        P.findings.clear();
        P.apiHits.clear();
        time_t now = time(nullptr);

        std::vector<std::string> odd;
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            bool x = (s.chars & 0x20000000u) != 0;
            bool w = (s.chars & 0x80000000u) != 0;
            bool code = (s.chars & 0x20u) != 0;

            if (x && w)
                addF(SEV_HIGH, sfmt("Writable AND executable section (W+X): %s", s.name),
                     "A W+X section allows writing code at runtime and executing it: typical of packers, "
                     "in-memory unpackers and injection.");
            else if (x && !code && !isStdSection(s.name))
                addF(SEV_MED, sfmt("Executable section without CODE flag: %s", s.name),
                     "Execution permission on a section not marked as code.");
            if (x && s.rawsize == 0 && s.vsize > 0)
                addF(SEV_HIGH, sfmt("Executable section empty on disk: %s (VSize %s)", s.name, H32(s.vsize).c_str()),
                     "Executable section with no file data: typical unpacking stub (e.g. UPX0).");
            const char *pk = packerOf(s.name);
            if (pk)
                addF(SEV_HIGH, sfmt("Packer/protector section name: %s (%s)", s.name, pk),
                     "The name matches those left by this packer.");
            if (s.rawsize >= 1024)
            {
                std::string ln = lower(s.name);
                bool rsrc = (ln == ".rsrc");
                if (s.entropy >= 7.2)
                    addF(rsrc ? SEV_LOW : SEV_HIGH, sfmt("Very high entropy in %s: %.2f / 8.00", s.name, s.entropy),
                         rsrc ? "Resources often contain already-compressed data: may be normal."
                              : "Compressed, encrypted or obfuscated data: classic packing indicator.");
                else if (s.entropy >= 6.8 && !rsrc)
                    addF(SEV_MED, sfmt("High entropy in %s: %.2f / 8.00", s.name, s.entropy),
                         "Above typical native code (5.5-6.5): partial compression or encrypted data.");
            }
            if ((uint64_t)s.rawptr + s.rawsize > P.d.size() && s.rawsize)
                addF(SEV_MED, sfmt("Section %s beyond end of file", s.name),
                     "Declared data exceeds the file: truncated or manipulated header.");
            if (s.rawsize && s.vsize && s.vsize > 16u * s.rawsize && s.vsize > (1u << 20) && !x)
                addF(SEV_LOW, sfmt("Section %s: VirtualSize much larger than RawSize", s.name),
                     "Large memory-expanded area (bss or unpacking buffer).");
            if (!isStdSection(s.name) && !pk)
                odd.push_back(s.name);
        }
        if (!odd.empty())
        {
            std::string l;
            for (size_t i = 0; i < odd.size() && i < 8; i++)
            {
                if (i)
                    l += ", ";
                l += odd[i];
            }
            addF(SEV_LOW, sfmt("Non-standard section names (%u)", (unsigned)odd.size()), l);
        }
        if (P.nsec > 20)
            addF(SEV_LOW, sfmt("High section count (%u)", (unsigned)P.nsec), "");
        if (P.nsec == 0)
            addF(SEV_MED, "No sections", "The PE file declares no sections.");

        bool isDll = (P.chars & 0x2000) != 0;
        if (P.entry)
        {
            if (P.epSec < 0)
                addF(SEV_HIGH, "Entry point outside every section",
                     sfmt("EP RVA %s does not fall inside any section.", H32(P.entry).c_str()));
            else
            {
                const Section &s = P.sec[(size_t)P.epSec];
                if (!(s.chars & 0x20000000u))
                    addF(SEV_HIGH, sfmt("Entry point in NON-executable section (%s)", s.name),
                         "The EP points to a section without execution permission.");
                else if (P.epSec == (int)P.sec.size() - 1 && P.sec.size() > 1 && lower(s.name) != ".text")
                    addF(SEV_MED, sfmt("Entry point in last section (%s)", s.name),
                         "Typical pattern of packers and infected files.");
                else if (!isStdSection(s.name))
                    addF(SEV_LOW, sfmt("Entry point in non-standard section (%s)", s.name), "");
            }
        }
        else if (!isDll && P.subsystem != 1)
        {
            addF(SEV_MED, "Null entry point in an executable", "An EXE without AddressOfEntryPoint is anomalous.");
        }

        if (!P.isNet)
        {
            if (P.importFnCount == 0 && P.imports.empty())
                addF(isDll ? SEV_LOW : SEV_MED, "No imports",
                     "Typical of shellcode wrappers, packers or manually resolved imports.");
            else if (P.importFnCount > 0 && P.importFnCount <= 6)
                addF(SEV_MED, sfmt("Very few imports (%u)", (unsigned)P.importFnCount),
                     "With few imports the program resolves APIs at runtime: typical of packer/protector.");
        }

        int catSum[CAT_COUNT], catCnt[CAT_COUNT];
        for (int i = 0; i < CAT_COUNT; i++)
        {
            catSum[i] = 0;
            catCnt[i] = 0;
        }
        for (size_t i = 0; i < P.imports.size(); i++)
            for (size_t j = 0; j < P.imports[i].fns.size(); j++)
            {
                const ImportFn &f = P.imports[i].fns[j];
                if (f.byOrd)
                    continue;
                const ApiDef *a = lookupApi(f.name);
                if (!a)
                    continue;
                ApiHit h;
                h.dll = P.imports[i].dll;
                h.fn = f.name;
                h.cat = a->cat;
                h.w = a->w;
                P.apiHits.push_back(h);
                catSum[a->cat] += a->w;
                catCnt[a->cat]++;
            }
        for (int c = 0; c < CAT_COUNT; c++)
        {
            if (!catCnt[c] || c == CAT_DYN || c == CAT_MEM)
                continue;
            int sev = catSum[c] >= 6 ? SEV_HIGH : (catSum[c] >= 3 ? SEV_MED : SEV_INFO);
            if (sev == SEV_INFO && c != CAT_INJ && c != CAT_ANTIDBG && c != CAT_PRIV)
                continue;
            addF(sev, sfmt("Suspicious APIs - %s (%d)", CAT_NAME[c], catCnt[c]),
                 "Context required: many legitimate programs use the same APIs.");
        }

        checkSectionOverlap();
        checkAlignment();
        checkDuplicateNames();
        checkGaps();
        checkOrdinalImports();
        checkDllNames();
        checkImportSection();
        checkStub();
        checkRich();
        checkTimestampSignature();
        checkEntryBounds();
        analyzeChunks();

        if (P.tlsCallbacks > 0)
            addF(SEV_MED, sfmt("TLS callbacks present (%d)", P.tlsCallbacks),
                 "TLS callbacks run BEFORE the entry point: anti-debug or hidden init.");

        P.csumCalc = calcChecksum();
        if (P.checksum != 0 && P.checksum != P.csumCalc)
            addF(SEV_MED, "Invalid PE checksum",
                 sfmt("Stored %s, computed %s: file modified after linking.",
                      H32(P.checksum).c_str(), H32(P.csumCalc).c_str()));
        else if (P.checksum == 0 && P.subsystem == 1)
            addF(SEV_LOW, "Driver with zero checksum", "Kernel drivers require a valid checksum.");

        if (P.timestamp == 0)
            addF(SEV_LOW, "Missing compile timestamp (0)", "Zeroed or deterministic build (/Brepro).");
        else if ((time_t)P.timestamp > now + 86400)
            addF(SEV_MED, "Compile timestamp in the future",
                 sfmt("%s: falsified or unreliable.", tsStr(P.timestamp).c_str()));

        if (P.overlaySize > 0)
        {
            if (P.overlaySize > 1024 && P.overlayEntropy >= 7.2)
                addF(SEV_MED, sfmt("Encrypted/compressed overlay (%lu bytes, entropy %.2f)", (unsigned long)P.overlaySize, P.overlayEntropy),
                     "High-entropy data appended after the last section: hidden payload or SFX archive.");
            else
                addF(SEV_INFO, sfmt("Overlay present (%lu bytes, entropy %.2f)", (unsigned long)P.overlaySize, P.overlayEntropy),
                     "Data after the last section (installer, archives, configuration).");
        }

        {
            std::string miss;
            if (!(P.dllChars & 0x0040))
                miss += "ASLR ";
            if (!(P.dllChars & 0x0100))
                miss += "DEP ";
            if (!miss.empty() && P.subsystem != 1)
                addF(SEV_LOW, "Missing security mitigations", "Not enabled: " + trim(miss));
        }

        if (P.lfanew > 0x1000)
            addF(SEV_LOW, "PE header very far from the start",
                 sfmt("e_lfanew = %s: possible hidden data before the PE.", H32(P.lfanew).c_str()));

        if (P.isNet)
            addF(SEV_INFO, ".NET assembly (CLR header present)",
                 "Managed IL code: use a .NET decompiler (dnSpy/ILSpy).");
        if (P.hasSig)
            addF(SEV_INFO, "Digital signature present", "Presence detected, validity NOT verified.");
        else
            addF(SEV_INFO, "No digital signature", "");
        if (!P.pdb.empty())
            addF(SEV_INFO, "PDB path present", P.pdb);

        std::stable_sort(P.findings.begin(), P.findings.end(), findingLess);

        int sc = 0;
        for (size_t i = 0; i < P.findings.size(); i++)
        {
            switch (P.findings[i].sev)
            {
            case SEV_HIGH:
                sc += 18;
                break;
            case SEV_MED:
                sc += 8;
                break;
            case SEV_LOW:
                sc += 3;
                break;
            default:
                break;
            }
        }
        P.score = sc > 100 ? 100 : sc;
    }

    static const char *scoreLabel(int s)
    {
        if (s < 10)
            return "LOW";
        if (s < 30)
            return "MODERATE";
        if (s < 60)
            return "HIGH";
        return "CRITICAL";
    }

    static bool loadFile(const std::string &path, Info &target)
    {
        Info fresh;
        Info *savedP = activeP;
        activeP = &fresh;
        fresh.path = path;

        FILE *f = fopen(path.c_str(), "rb");
        if (!f)
        {
            target.err = "Cannot open file (wrong path or access denied).";
            activeP = savedP;
            return false;
        }
        auto fail = [&](const std::string &msg)
        {
            fclose(f);
            target.err = msg;
            activeP = savedP;
            return false;
        };
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0)
            return fail("Empty file or unreadable size.");
        if (sz > 512L * 1024L * 1024L)
            return fail("File too large (512 MB limit).");

        fresh.d.resize((size_t)sz);
        size_t rd = fread(&fresh.d[0], 1, (size_t)sz, f);
        fclose(f);
        if (rd != (size_t)sz)
        {
            target.err = "Incomplete file read.";
            activeP = savedP;
            return false;
        }

        fresh.md5 = md5Of(&fresh.d[0], fresh.d.size());
        fresh.sha256 = sha256Of(&fresh.d[0], fresh.d.size());
        fresh.fileEntropy = entropyOf(&fresh.d[0], fresh.d.size());

        if (!parsePE())
        {
            target.err = fresh.err;
            activeP = savedP;
            return false;
        }
        parseImports();
        parseExports();
        parseExtras();
        analyze();
        fresh.loaded = true;

        target = std::move(fresh);
        activeP = savedP;
        return true;
    }

    static bool load(const std::string &path)
    {
        return loadFile(path, P_global);
    }

    static std::string peType()
    {
        std::string t;
        if (P.chars & 0x2000)
            t = "DLL";
        else if (P.subsystem == 1)
            t = "Driver (SYS)";
        else
            t = "Executable (EXE)";
        t += P.is64 ? "  PE32+ (64 bit)" : "  PE32 (32 bit)";
        if (P.isNet)
            t += "  .NET";
        return t;
    }

     
    static std::string hexDumpText(uint64_t off, uint64_t len)
    {
        size_t n = P.d.size();
        if (off >= n)
            return "  Offset out of file range.\n";
        if (off + len > n)
            len = n - off;
        std::string out = "  Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n";
        for (uint64_t base = off; base < off + len; base += 16)
        {
            std::string hex, asc;
            for (int i = 0; i < 16; i++)
            {
                if (base + (uint64_t)i < off + len)
                {
                    uint8_t b = P.d[(size_t)(base + (uint64_t)i)];
                    hex += sfmt("%02X ", (unsigned)b);
                    asc += (b >= 32 && b < 127) ? (char)b : '.';
                }
                else
                {
                    hex += "   ";
                    asc += ' ';
                }
                if (i == 7)
                    hex += ' ';
            }
            out += sfmt("  %08X  ", (unsigned)base) + hex + "  |" + asc + "|\n";
        }
        return out;
    }

    static std::string offsetInfo(uint64_t off)
    {
        std::string s = sfmt("Offset %s", H32((uint32_t)off).c_str());
        long long rva = off2rva(off);
        if (rva >= 0)
            s += sfmt("   RVA %s", H32((uint32_t)rva).c_str());
        int k = secByOff(off);
        if (k >= 0)
            s += sfmt("   %s +%s", P.sec[(size_t)k].name, H32((uint32_t)(off - P.sec[(size_t)k].rawptr)).c_str());
        else if (off < P.sizeHdr)
            s += "   headers";
        else if (P.overlaySize && off >= P.overlayOff)
            s += "   overlay";
        return s;
    }

#undef P
    static Info &P = P_global;

}  

 
 
 

static bool writeReport(const std::string &outPath)
{
    using namespace pe;
    FILE *f = fopen(outPath.c_str(), "w");
    if (!f)
        return false;

    fprintf(f, "%s v%s - PE Static Triage Report\nDeveloped by %s\n", CI_NAME, CI_VERSION, CI_AUTHOR);
    time_t now = time(nullptr);
    char tb[64];
    struct tm *g = localtime(&now);
    if (g)
    {
        strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S", g);
        fprintf(f, "Generated on: %s\n", tb);
    }
    fprintf(f, "\n");

    fprintf(f, "FILE\n  path      : %s\n  size      : %s\n  md5       : %s\n  sha256    : %s\n  imphash   : %s\n"
               "  type      : %s\n  entropy   : %.4f (%s)\n\n",
            P.path.c_str(), sizeStr(P.d.size()).c_str(), P.md5.c_str(), P.sha256.c_str(),
            P.imphash.empty() ? "n/a" : P.imphash.c_str(), peType().c_str(),
            P.fileEntropy, entropyVerdict(P.fileEntropy));

    fprintf(f, "HEADER\n  architecture  : %s\n  subsystem     : %s\n  imagebase     : %s\n  entry point   : %s\n"
               "  sizeofimage   : %s\n  timestamp     : %s\n  checksum      : %s (computed %s)\n"
               "  characteristics: %s\n  dllcharacteristics: %s\n\n",
            machineName(P.machine), subsysName(P.subsystem), H64(P.imageBase).c_str(),
            P.entry ? H32(P.entry).c_str() : "none", H32(P.sizeImage).c_str(),
            tsStr(P.timestamp).c_str(), H32(P.checksum).c_str(), H32(P.csumCalc).c_str(),
            charsStr(P.chars).c_str(), dllCharsStr(P.dllChars).c_str());

    fprintf(f, "SECTIONS\n");
    for (size_t i = 0; i < P.sec.size(); i++)
    {
        const Section &s = P.sec[i];
        fprintf(f, "  %-10s VA %s VS %s RAW %s SIZE %s PERM %s ENT %.3f%s\n",
                s.name, H32(s.vaddr).c_str(), H32(s.vsize).c_str(), H32(s.rawptr).c_str(),
                H32(s.rawsize).c_str(), permStr(s.chars).c_str(), s.entropy,
                (int)i == P.epSec ? "  <EP>" : "");
    }
    fprintf(f, "\nDATA DIRECTORIES\n");
    for (int i = 0; i < 16; i++)
    {
        if (!P.dir[i].rva && !P.dir[i].size)
            continue;
        fprintf(f, "  %-20s RVA %s SIZE %s\n", dirName(i), H32(P.dir[i].rva).c_str(), H32(P.dir[i].size).c_str());
    }

    fprintf(f, "\nIMPORT\n");
    for (size_t i = 0; i < P.imports.size(); i++)
    {
        fprintf(f, "  %s%s\n", P.imports[i].dll.c_str(), P.imports[i].delay ? " (delay-load)" : "");
        for (size_t j = 0; j < P.imports[i].fns.size(); j++)
        {
            const ImportFn &fn = P.imports[i].fns[j];
            fprintf(f, "      %s\n", fn.byOrd ? sfmt("Ordinal #%u", (unsigned)fn.ordinal).c_str() : fn.name.c_str());
        }
    }

    fprintf(f, "\nEXPORT\n");
    for (size_t i = 0; i < P.exports.size(); i++)
        fprintf(f, "  ord %u  %s  %s%s%s\n", (unsigned)P.exports[i].ordinal,
                H32(P.exports[i].rva).c_str(), P.exports[i].name.c_str(),
                P.exports[i].forward.empty() ? "" : " -> ", P.exports[i].forward.c_str());

    fprintf(f, "\nENTROPY\n  file %.4f\n", P.fileEntropy);
    for (size_t i = 0; i < P.sec.size(); i++)
        fprintf(f, "  %-10s %.4f  4K blocks over 7.2: %u/%u\n",
                P.sec[i].name, P.sec[i].entropy, P.sec[i].chunkHigh, P.sec[i].chunkTotal);
    if (P.overlaySize)
        fprintf(f, "  overlay    %.4f (%lu bytes)\n", P.overlayEntropy, (unsigned long)P.overlaySize);

    fprintf(f, "\nSUSPICION INDEX: %d/100 (%s)\n\nFINDINGS\n", P.score, scoreLabel(P.score));
    for (size_t i = 0; i < P.findings.size(); i++)
    {
        static const char *T[4] = {"INFO", "LOW", "MEDIUM", "HIGH"};
        fprintf(f, "  [%-5s] %s\n", T[P.findings[i].sev], P.findings[i].title.c_str());
        if (!P.findings[i].detail.empty())
            fprintf(f, "          %s\n", P.findings[i].detail.c_str());
    }

    fprintf(f, "\nHEX DUMP (first 256 bytes)\n%s\n", hexDumpText(0, 256).c_str());
    fclose(f);
    return true;
}

 
 
 



 
class CiDialog : public Dialog
{
public:
    using Dialog::Dialog;
    std::function<void()> on_enter;
};

 
 
 

class CodeInquisitor
{
public:
    App app;
    std::shared_ptr<Vertical> root;
    std::shared_ptr<StatusBar> header;
    std::shared_ptr<Vertical> contentHost;
    std::shared_ptr<Notification> notif;
    std::shared_ptr<Dialog> openDlg;
    std::shared_ptr<Input> pathInput;
    std::shared_ptr<CiFileBrowser> explorer;
    std::shared_ptr<Label> openStatus;
    std::shared_ptr<SplashWidget> splash;
    bool splashActive = false;
    std::chrono::steady_clock::time_point splashStart;

    std::string hexOffText = "0";
    std::string strFilter;
    unsigned strMin = 6;
    bool asciiToo = true, wideToo = true;
    bool analyzing = false;

    int activeTabIdx = 0;
    std::shared_ptr<Tabs> currentTabs;
    std::shared_ptr<MenuBar> mainMenuBar;

    std::string dropBuf;
    std::chrono::steady_clock::time_point dropLast = std::chrono::steady_clock::now();
    bool dropSwallow = false;

    void cycleTheme()
    {
        static int i = 0;
        const int n = 5;
        i = (i + 1) % n;
        switch (i)
        {
        case 0: Theme::set_theme(Theme::Dark()); break;
        case 1: Theme::set_theme(Theme::Light()); break;
        case 2: Theme::set_theme(Theme::Nord()); break;
        case 3: Theme::set_theme(Theme::TokyoNight()); break;
        default: Theme::set_theme(Theme::SolarizedLight()); break;
        }
        rebuild();
    }

    void selectTab(int idx)
    {
        if (idx < 0 || idx >= 10)
            return;
        activeTabIdx = idx;
        if (currentTabs)
            currentTabs->select_tab(idx);
    }

     
    static Color sevColor(int sev)
    {
        const Theme &t = Theme::current();
        switch (sev)
        {
        case pe::SEV_HIGH:
            return t.error;
        case pe::SEV_MED:
            return t.warning;
        case pe::SEV_LOW:
            return t.primary;
        default:
            return t.secondary;
        }
    }

    static Color scoreColor(int s)
    {
        const Theme &t = Theme::current();
        if (s < 10)
            return t.success;
        if (s < 30)
            return t.warning;
        return t.error;
    }

    static Color entColor(double e)
    {
        const Theme &t = Theme::current();
        if (e < 6.8)
            return t.success;
        if (e < 7.2)
            return t.warning;
        return t.error;
    }

    void finishSplash()
    {
        if (!splashActive)
            return;
        splashActive = false;
        splash.reset();
        rebuild();
    }

    void notify(const std::string &text, Notification::Type type = Notification::Type::Info)
    {
        if (notif)
            notif->show(text, type, 3500);
    }

     
    static std::shared_ptr<Label> kv(const std::string &k, const std::string &v, Color col)
    {
        auto l = std::make_shared<Label>(StyledText().colored(k, Theme::current().secondary).add("  ").colored(v, col));
        l->fixed_height = 1;
        return l;
    }

    static std::shared_ptr<TableScrollable> makeTable(const std::vector<std::string> &cols,
                                                      const std::vector<int> &widths)
    {
        auto t = std::make_shared<TableScrollable>();
        for (size_t i = 0; i < cols.size(); i++)
            t->columns.push_back(cols[i]);
        t->col_widths = widths;
        t->header_color = Theme::current().table_header_bg;
        t->row_fg_color = Theme::current().foreground;
        return t;
    }

    static void addCell(std::vector<StyledText> &row, const std::string &text, Color col)
    {
        row.push_back(StyledText().colored(text, col));
    }

     
    bool preEvent(const Event &ev)
    {
        if (splashActive &&
            (ev.type == EventType::Key || ev.is_mouse_event() ||
             ev.type == EventType::Paste))
        {
            finishSplash();
            return false;
        }
        if (ev.type == EventType::Paste)
        {
            if (!app.dialog_stack.empty())
                return false;
            std::string p = firstExistingFile(ev.paste_text);
            if (p.empty())
                return false;
            notify("Loading dropped file: " + baseName(p), Notification::Type::Info);
            loadAsync(p);
            return true;
        }
        if (ev.type != EventType::Key)
            return false;
        if (!app.dialog_stack.empty())
        {
            dropBuf.clear();
            dropSwallow = false;
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        auto msSince = [&](std::chrono::steady_clock::time_point t)
        { return std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count(); };

        if (ev.is_printable() && !ev.ctrl && !ev.alt)
        {
            Widget *f = CiFocusProbe::focused();
            if (f && (dynamic_cast<Input *>(f) || dynamic_cast<TextArea *>(f)))
                return false;

            if (ev.key >= '0' && ev.key <= '9' && !ev.shift)
            {
                int d = (ev.key == '0') ? 9 : ev.key - '1';
                selectTab(d);
                dropBuf.clear();
                dropSwallow = false;
                return true;
            }

            if (dropSwallow)
            {
                if (msSince(dropLast) < 400)
                {
                    dropLast = now;
                    return true;
                }
                dropSwallow = false;
                dropBuf.clear();
            }
            if (msSince(dropLast) > 300)
                dropBuf.clear();
            if (ev.key < 128)
                dropBuf += (char)ev.key;
            dropLast = now;
            std::string cand = dropCandidate(dropBuf);
            if (!cand.empty())
            {
                dropBuf.clear();
                dropSwallow = true;
                notify("Loading dropped file: " + baseName(cand), Notification::Type::Info);
                loadAsync(cand);
                return true;
            }
        }
        else if (!ev.is_printable())
        {
            dropSwallow = false;
        }
        return false;
    }

    void openFileDialog()
    {
        if (openDlg)
            return;
        openDlg = std::make_shared<Dialog>(&app, BorderStyle::Double);
        openDlg->set_title(" Open PE file ", Alignment::Center);
        openDlg->modal = true;
        openDlg->shadow = true;
        openDlg->fixed_width = 74;
        openDlg->fixed_height = 22;

        auto col = std::make_shared<Vertical>();

        pathInput = std::make_shared<Input>();
        pathInput->placeholder = "full path to an EXE / DLL / SYS (press Enter to analyze)";
        pathInput->fixed_height = 1;
        col->add(pathInput);

        auto btns = std::make_shared<Horizontal>();
        btns->fixed_height = 1;
        auto bOpen = std::make_shared<Button>("Analyze", [this]()
                                              { submitOpen(); });
        auto bCancel = std::make_shared<Button>("Cancel", [this]()
                                                { closeFileDialog(); });
        bOpen->fixed_width = 14;
        bCancel->fixed_width = 14;
        btns->add(bOpen);
        btns->add(std::make_shared<HorizontalSpacer>(1));
        btns->add(bCancel);
        col->add(btns);

        openStatus = std::make_shared<Label>(" Pick a file, paste a path + Enter, or drag & drop a file here.");
        openStatus->fixed_height = 1;
        col->add(openStatus);

        explorer = std::make_shared<CiFileBrowser>();
        explorer->on_file_selected = [this](const std::string &p)
        {
            if (pathInput)
                pathInput->set_text(p);
            submitOpen();
        };
        col->add(explorer);

        openDlg->add(col);
        app.open_dialog(openDlg);
        pathInput->set_focus(true);
    }

    void closeFileDialog()
    {
        if (!openDlg)
            return;
        app.close_dialog(openDlg);
        openDlg.reset();
        pathInput.reset();
        explorer.reset();
        openStatus.reset();
    }

    void submitOpen()
    {
        if (!pathInput)
            return;
        std::string p = stripQuotes(pathInput->get_value());
        if (p.empty())
        {
            if (openStatus)
                openStatus->set_text(" Empty path.");
            return;
        }
        closeFileDialog();
        loadAsync(p);
    }

    void closeCurrentFile()
    {
        if (!pe::P.loaded)
        {
            notify("No file loaded", Notification::Type::Warning);
            return;
        }
        std::string fn = baseName(pe::P.path);
        pe::P.reset();
        hexOffText = "0";
        activeTabIdx = 0;
        updateHeader();
        rebuild();
        notify("Closed: " + fn, Notification::Type::Info);
    }

    void copyHashesToClipboard()
    {
        if (!pe::P.loaded)
        {
            notify("Load a PE file first to copy hashes", Notification::Type::Warning);
            return;
        }
        std::string text = "File: " + baseName(pe::P.path) + "\n" +
                           "MD5: " + pe::P.md5 + "\n" +
                           "SHA-256: " + pe::P.sha256 + "\n" +
                           "Imphash: " + (pe::P.imphash.empty() ? "n/a" : pe::P.imphash) + "\n";
        copy_to_clipboard(text);
        notify("Hashes (MD5, SHA-256, Imphash) copied!", Notification::Type::Success);
    }

    void showGoToDialog()
    {
        if (!pe::P.loaded)
        {
            notify("Load a PE file first", Notification::Type::Warning);
            return;
        }
        auto d = std::make_shared<CiDialog>(&app, BorderStyle::Double);
        d->set_title(" Go to Offset / RVA ", Alignment::Center);
        d->modal = true;
        d->shadow = true;
        d->fixed_width = 56;
        d->fixed_height = 8;

        auto col = std::make_shared<Vertical>();
        auto lbl = std::make_shared<Label>(" Enter a file offset or RVA in hex (e.g. 0x400 or 1A0):");
        lbl->fixed_height = 1;
        col->add(lbl);

        auto in = std::make_shared<Input>();
        in->set_text(hexOffText);
        in->fixed_height = 1;
        col->add(in);

        auto btns = std::make_shared<Horizontal>();
        btns->fixed_height = 1;
        auto doGo = [this, d, in]()
        {
            std::string v = in->get_value();
            uint64_t o = 0;
            if (!parseHex(v, o))
            {
                notify("Invalid hexadecimal value", Notification::Type::Error);
                return;
            }
            if (o >= pe::P.d.size())
            {
                long long fo = pe::rva2off((uint32_t)o);
                if (fo < 0)
                {
                    notify("Offset or RVA not present in file", Notification::Type::Error);
                    return;
                }
                o = (uint64_t)fo;
            }
            hexOffText = H32((uint32_t)o);
            app.close_dialog(d);
            selectTab(8);
            rebuild();
        };
        auto ok = std::make_shared<Button>("Go", doGo);
        d->on_enter = doGo;
        auto cancel = std::make_shared<Button>("Cancel", [this, d]() {
            app.close_dialog(d);
        });
        ok->fixed_width = 12;
        cancel->fixed_width = 12;
        btns->add(ok);
        btns->add(std::make_shared<HorizontalSpacer>(2));
        btns->add(cancel);
        col->add(btns);

        d->add(col);
        app.open_dialog(d);
        in->set_focus(true);
    }

    void showHelp()
    {
        auto d = std::make_shared<CiDialog>(&app, BorderStyle::Rounded);
        d->set_title(" Keyboard Shortcuts ", Alignment::Center);
        d->modal = true;
        d->shadow = true;
        d->fixed_width = 72;
        d->fixed_height = 22;

        auto p = std::make_shared<Paragraph>();
        p->word_wrap = true;
        p->styled_content = StyledText()
            .colored("  Actions:\n", Theme::current().primary)
            .colored("    F1                   ", Theme::current().foreground).colored(": Show this help\n", Theme::current().secondary)
            .colored("    F2                   ", Theme::current().foreground).colored(": Open a PE file to analyze\n", Theme::current().secondary)
            .colored("    F3                   ", Theme::current().foreground).colored(": Reload the current file analysis\n", Theme::current().secondary)
            .colored("    F4                   ", Theme::current().foreground).colored(": Export a plain-text triage report\n", Theme::current().secondary)
            .colored("    F5                   ", Theme::current().foreground).colored(": Cycle through color themes\n", Theme::current().secondary)
            .colored("    F6 / F11              ", Theme::current().foreground).colored(": Close file and return to Home\n", Theme::current().secondary)
            .colored("    F7                   ", Theme::current().foreground).colored(": Jump to a hex file offset or RVA\n", Theme::current().secondary)
            .colored("    F8                   ", Theme::current().foreground).colored(": Copy MD5, SHA-256 and Imphash to clipboard\n", Theme::current().secondary)
            .colored("    F9 / F10             ", Theme::current().foreground).colored(": Previous / next view\n", Theme::current().secondary)
            .colored("    F12                  ", Theme::current().foreground).colored(": Quit the application\n\n", Theme::current().secondary)
            .colored("  Views & Navigation:\n", Theme::current().primary)
            .colored("    1 .. 0               ", Theme::current().foreground).colored(": Jump directly to views 1 through 10\n", Theme::current().secondary)
            .colored("    Tab / Shift+Tab      ", Theme::current().foreground).colored(": Move focus between fields and buttons\n", Theme::current().secondary)
            .colored("    Arrows / Enter / Esc ", Theme::current().foreground).colored(": Navigate, select and close menus/dialogs\n", Theme::current().secondary)
            .colored("    Drag & drop          ", Theme::current().foreground).colored(": Drop an EXE / DLL / SYS file to analyze it\n", Theme::current().secondary);

        auto col = std::make_shared<Vertical>();
        col->add(p);

        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto b = std::make_shared<Button>("Close (Esc)", [this, d]() { app.close_dialog(d); });
        b->fixed_width = 16;
        bar->add(std::make_shared<HorizontalSpacer>(0));
        bar->add(b);
        bar->add(std::make_shared<HorizontalSpacer>(0));
        col->add(bar);

        d->add(col);
        app.open_dialog(d);
    }

    void showAbout()
    {
        auto d = std::make_shared<CiDialog>(&app, BorderStyle::Rounded);
        d->set_title(" About ", Alignment::Center);
        d->modal = true;
        d->shadow = true;
        d->fixed_width = 68;
        d->fixed_height = 16;

        auto p = std::make_shared<Paragraph>();
        p->text =
            std::string(CI_NAME) + " v" + CI_VERSION + " - PE Static Triage Console\n"
                                                       "Developed by " CI_AUTHOR "\n\n"
                                                       "Static analyzer for PE files (EXE / DLL / SYS).\n"
                                                       "The target is never executed: only bytes on disk are read.\n\n"
                                                       "Interface built with cpp-tui (MIT).\n"
                                                       "The suspicion index is heuristic, not an absolute verdict.\n"
                                                       "The Authenticode signature is detected, not cryptographically verified.";
        p->word_wrap = true;

        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto b = std::make_shared<Button>("Close (Esc)", [this, d]()
                                          { app.close_dialog(d); });
        b->fixed_width = 16;
        bar->add(std::make_shared<HorizontalSpacer>(0));
        bar->add(b);
        bar->add(std::make_shared<HorizontalSpacer>(0));

        auto col = std::make_shared<Vertical>();
        col->add(p);
        col->add(bar);
        d->add(col);
        app.open_dialog(d);
    }

     
    void loadAsync(const std::string &path)
    {
        if (analyzing)
            return;
        analyzing = true;
        if (header)
        {
            header->sections.clear();
            header->add_section(StyledText().colored(" Analyzing: ", Theme::current().warning).add(baseName(path)));
        }

        auto dlg = std::make_shared<Dialog>(&app, BorderStyle::Rounded);
        dlg->set_title(" Analyzing ", Alignment::Center);
        dlg->modal = true;
        dlg->fixed_width = 56;
        dlg->fixed_height = 6;
        auto sp = std::make_shared<Spinner>(&app);
        sp->value = -1;
        sp->color = Theme::current().primary;
        auto lbl = std::make_shared<Label>(" " + baseName(path));
        auto row = std::make_shared<Horizontal>();
        row->fixed_height = 1;
        row->add(std::make_shared<HorizontalSpacer>(1));
        row->add(sp);
        row->add(lbl);
        dlg->add(row);
        app.open_dialog(dlg);

        std::thread([this, path, dlg]()
                    {
            pe::Info info;
            bool ok = pe::loadFile(path, info);
            std::string err = info.err;
            std::string p = path;
            app.post([this, ok, err, p, dlg, loadedInfo = std::move(info)]() mutable {
                analyzing = false;
                app.close_dialog(dlg);
                if (!ok) {
                    notify("Error: " + err, Notification::Type::Error);
                    updateHeader();
                    return;
                }
                pe::P = std::move(loadedInfo);
                rebuild();
                notify("Analysis complete: " + baseName(p), Notification::Type::Success);
            }); })
            .detach();
    }

     
    void updateHeader()
    {
        if (!header)
            return;
        header->sections.clear();
        if (!pe::P.loaded)
        {
            header->add_section(StyledText().colored(" No file loaded", Theme::current().warning).add("   F2 or drag & drop a PE file"));
            header->add_section(StyledText().colored(" " + std::string(CI_NAME) + " v" CI_VERSION + "  by " CI_AUTHOR, Theme::current().secondary));
            return;
        }
        auto l1 = StyledText().colored(" " + baseName(pe::P.path), Theme::current().foreground);
        header->add_section(l1);
        header->add_section(StyledText().add(pe::peType() + "  |  " + pe::machineName(pe::P.machine) + "  |  " + sizeStr(pe::P.d.size()) + "  |  " + std::to_string(pe::P.sec.size()) + " sections" + "  |  " + std::to_string(pe::P.importFnCount) + " imports"),
                            0, Alignment::Left);
        header->add_section(StyledText().colored(" Suspicion: ", Theme::current().secondary).colored(sfmt("%d/100 %s", pe::P.score, pe::scoreLabel(pe::P.score)), scoreColor(pe::P.score)), 0, Alignment::Right);
    }

     
    std::shared_ptr<Widget> buildOverview()
    {
        using namespace pe;
        auto scroll = std::make_shared<ScrollableVertical>();

        auto hash = std::make_shared<Vertical>();
        hash->fixed_height = 8;
        hash->add(kv("MD5", P.md5, Theme::current().foreground));
        hash->add(kv("SHA-256", P.sha256, Theme::current().foreground));
        hash->add(kv("Imphash", P.imphash.empty() ? "n/a" : P.imphash, Theme::current().foreground));
        hash->add(kv("Type", peType(), Theme::current().primary));
        hash->add(kv("File entropy", sfmt("%.4f  %s", P.fileEntropy, entropyVerdict(P.fileEntropy)),
                     entColor(P.fileEntropy)));
        hash->add(kv("Size", sizeStr(P.d.size()), Theme::current().foreground));
        scroll->add(hash);

        auto hdr = std::make_shared<Vertical>();
        hdr->fixed_height = 12;
        hdr->add(kv("Architecture", sfmt("%s (0x%04X)", machineName(P.machine), (unsigned)P.machine),
                    Theme::current().foreground));
        hdr->add(kv("Subsystem", sfmt("%s (%u)", subsysName(P.subsystem), (unsigned)P.subsystem),
                    Theme::current().foreground));
        hdr->add(kv("ImageBase", H64(P.imageBase), Theme::current().foreground));
        {
            std::string ep = sfmt("RVA %s", H32(P.entry).c_str());
            if (P.epSec >= 0)
                ep += sfmt("  [%s]", P.sec[(size_t)P.epSec].name);
            else if (P.entry)
                ep += "  [outside section]";
            else
                ep += "  [none]";
            hdr->add(kv("Entry point", ep, (P.entry && P.epSec < 0) ? Theme::current().error : Theme::current().foreground));
        }
        hdr->add(kv("SizeOfImage / Headers", H32(P.sizeImage) + " / " + H32(P.sizeHdr),
                    Theme::current().foreground));
        hdr->add(kv("Section/File Align", H32(P.secAlign) + " / " + H32(P.fileAlign), Theme::current().foreground));
        hdr->add(kv("Timestamp", tsStr(P.timestamp), Theme::current().foreground));
        {
            bool bad = (P.checksum != 0 && P.checksum != P.csumCalc);
            hdr->add(kv("Checksum", H32(P.checksum) + (bad ? "  [INVALID]" : "  [ok]"),
                        bad ? Theme::current().error : Theme::current().foreground));
        }
        hdr->add(kv("Characteristics", charsStr(P.chars).empty() ? "(none)" : charsStr(P.chars),
                    Theme::current().secondary));
        hdr->add(kv("DllCharacteristics", dllCharsStr(P.dllChars).empty() ? "(none)" : dllCharsStr(P.dllChars),
                    Theme::current().secondary));
        scroll->add(hdr);

        auto extra = std::make_shared<Vertical>();
        extra->fixed_height = 8;
        extra->add(kv("Overlay", P.overlaySize ? sfmt("%lu bytes (entropy %.2f)", (unsigned long)P.overlaySize, P.overlayEntropy) : "absent",
                      P.overlaySize ? Theme::current().warning : Theme::current().secondary));
        extra->add(kv("Digital signature", P.hasSig ? "present (not verified)" : "absent",
                      P.hasSig ? Theme::current().success : Theme::current().secondary));
        extra->add(kv("PDB", P.pdb.empty() ? "n/a" : P.pdb, Theme::current().secondary));
        extra->add(kv("TLS callback", std::to_string(P.tlsCallbacks),
                      P.tlsCallbacks ? Theme::current().warning : Theme::current().secondary));
        extra->add(kv("Ordinal imports", std::to_string(P.ordinalCount),
                      P.ordinalCount ? Theme::current().warning : Theme::current().secondary));
        extra->add(kv("Rich header", P.hasRich ? "present" : "absent",
                      P.hasRich ? Theme::current().foreground : Theme::current().warning));
        scroll->add(extra);

        auto gaugeRow = std::make_shared<Horizontal>();
        gaugeRow->fixed_height = 6;
        auto g = std::make_shared<Gauge>(P.score / 100.0);
        g->value_format = "%d%%";
        g->fill_color = scoreColor(P.score);
        g->min_label = "0";
        g->max_label = "100";
        g->fixed_width = 30;
        gaugeRow->add(g);
        gaugeRow->add(kv("Suspicion index", sfmt("%d/100  %s", P.score, scoreLabel(P.score)),
                         scoreColor(P.score)));
        scroll->add(gaugeRow);

        return scroll;
    }

    std::shared_ptr<Widget> buildSections()
    {
        using namespace pe;
        auto t = makeTable({"#", "Name", "VirtAddr", "VirtSize", "RawPtr", "RawSize", "Perm", "Entropy", "Notes"},
                           {4, 12, 12, 12, 12, 12, 6, 10, 30});
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            bool wx = (s.chars & 0x80000000u) && (s.chars & 0x20000000u);
            std::string note;
            if (s.chars & 0x20)
                note += "CODE ";
            if (wx)
                note += "W+X! ";
            if ((int)i == P.epSec)
                note += "<EP> ";
            const char *pk = packerOf(s.name);
            if (pk)
                note += std::string("[") + pk + "] ";

            std::vector<StyledText> r;
            addCell(r, std::to_string(i + 1), Theme::current().secondary);
            addCell(r, s.name, Theme::current().foreground);
            addCell(r, H32(s.vaddr), Theme::current().foreground);
            addCell(r, H32(s.vsize), Theme::current().foreground);
            addCell(r, H32(s.rawptr), Theme::current().foreground);
            addCell(r, H32(s.rawsize), Theme::current().foreground);
            addCell(r, permStr(s.chars), wx ? Theme::current().error : Theme::current().primary);
            addCell(r, sfmt("%.3f", s.entropy), s.rawsize ? entColor(s.entropy) : Theme::current().secondary);
            addCell(r, trim(note), wx ? Theme::current().error : Theme::current().warning);
            t->rows.push_back(r);
        }
        return t;
    }

    std::shared_ptr<Widget> buildDirs()
    {
        using namespace pe;
        auto t = makeTable({"#", "Directory", "RVA", "Size", "Section", "Status"},
                           {4, 26, 12, 12, 14, 10});
        for (int i = 0; i < 16; i++)
        {
            bool present = P.dir[i].rva != 0 || P.dir[i].size != 0;
            std::string sec = "-";
            if (present && i != 4)
            {
                int k = secByRva(P.dir[i].rva);
                if (k >= 0)
                    sec = P.sec[(size_t)k].name;
            }
            if (i == 4 && present)
                sec = "(file offset)";
            std::vector<StyledText> r;
            addCell(r, std::to_string(i), Theme::current().secondary);
            addCell(r, dirName(i), present ? Theme::current().foreground : Theme::current().secondary);
            addCell(r, present ? H32(P.dir[i].rva) : "-", Theme::current().foreground);
            addCell(r, present ? H32(P.dir[i].size) : "-", Theme::current().foreground);
            addCell(r, sec, Theme::current().secondary);
            addCell(r, present ? "present" : "-", present ? Theme::current().success : Theme::current().secondary);
            t->rows.push_back(r);
        }
        return t;
    }

    std::shared_ptr<Widget> buildImports()
    {
        using namespace pe;
        auto col = std::make_shared<Vertical>();

        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto in = std::make_shared<Input>();
        in->placeholder = "filter by library or function";
        auto t = makeTable({"Library", "Function", "Hint", "Category"}, {26, 46, 8, 40});

        auto fill = [t](const std::string &filter)
        {
            t->rows.clear();
            std::string f = lower(filter);
            for (size_t i = 0; i < pe::P.imports.size(); i++)
            {
                const ImportDll &d = pe::P.imports[i];
                if (!f.empty() && lower(d.dll).find(f) == std::string::npos)
                    continue;
                for (size_t j = 0; j < d.fns.size(); j++)
                {
                    const ImportFn &fn = d.fns[j];
                    if (!f.empty() && lower(fn.name).find(f) == std::string::npos &&
                        lower(d.dll).find(f) == std::string::npos)
                        continue;
                    std::vector<StyledText> r;
                    addCell(r, d.dll + (d.delay ? " (delay)" : ""), Theme::current().warning);
                    if (fn.byOrd)
                    {
                        addCell(r, sfmt("Ordinal #%u", (unsigned)fn.ordinal), Theme::current().warning);
                        addCell(r, "-", Theme::current().secondary);
                        addCell(r, "(name not present)", Theme::current().secondary);
                    }
                    else
                    {
                        const ApiDef *a = lookupApi(fn.name);
                        Color c = a ? (a->w >= 3 ? Theme::current().error
                                                 : (a->w == 2 ? Theme::current().warning : Theme::current().primary))
                                    : Theme::current().foreground;
                        addCell(r, fn.name, c);
                        addCell(r, std::to_string(fn.hint), Theme::current().secondary);
                        addCell(r, a ? CAT_NAME[a->cat] : "", c);
                    }
                    t->rows.push_back(r);
                }
            }
        };

        in->on_change = [fill](std::string v)
        { fill(v); };
        bar->add(in);
        col->add(bar);
        col->add(t);
        fill("");
        return col;
    }

    std::shared_ptr<Widget> buildExports()
    {
        using namespace pe;
        auto t = makeTable({"Ordinal", "RVA", "Name", "Forward"}, {10, 12, 50, 40});
        for (size_t i = 0; i < P.exports.size(); i++)
        {
            const ExportFn &e = P.exports[i];
            std::vector<StyledText> r;
            addCell(r, std::to_string(e.ordinal), Theme::current().secondary);
            addCell(r, H32(e.rva), Theme::current().foreground);
            addCell(r, e.name.empty() ? "(ordinal only)" : e.name,
                    e.name.empty() ? Theme::current().secondary : Theme::current().foreground);
            addCell(r, e.forward, Theme::current().secondary);
            t->rows.push_back(r);
        }
        return t;
    }

    std::shared_ptr<Widget> buildEntropy()
    {
        using namespace pe;
        auto scroll = std::make_shared<ScrollableVertical>();

        auto head = std::make_shared<Label>(StyledText()
                                                .colored("Scale 0-8 bits/byte. Native code 5.5-6.5. Above 7.2 = compressed, encrypted or obfuscated.",
                                                         Theme::current().secondary));
        head->fixed_height = 1;
        scroll->add(head);

        auto bars = std::make_shared<Vertical>();
        bars->fixed_height = 2 + (int)P.sec.size() * 2 + (P.overlaySize ? 2 : 0);
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            auto row = std::make_shared<Horizontal>();
            row->fixed_height = 1;
            auto name = std::make_shared<Label>(StyledText().colored(s.name, Theme::current().foreground));
            name->fixed_width = 14;
            auto pb = std::make_shared<ProgressBar>();
            pb->value = (float)(s.entropy / 8.0);
            pb->color = entColor(s.entropy);
            pb->char_filled = "#";
            pb->char_empty = "-";
            row->add(name);
            row->add(pb);
            auto val = std::make_shared<Label>(StyledText().colored(sfmt(" %.3f %s", s.entropy,
                                                                         entropyVerdict(s.entropy)),
                                                                    entColor(s.entropy)));
            val->fixed_width = 34;
            row->add(val);
            bars->add(row);
            if (s.chunkTotal)
            {
                auto cl = std::make_shared<Label>(StyledText().colored(
                    sfmt("    4K blocks over 7.2: %u/%u  peak %.2f @ %s", s.chunkHigh, s.chunkTotal,
                         s.maxChunkEntropy, H32(s.maxChunkOff).c_str()),
                    s.chunkHigh ? Theme::current().warning : Theme::current().secondary));
                cl->fixed_height = 1;
                bars->add(cl);
            }
        }
        if (P.overlaySize)
        {
            auto row = std::make_shared<Horizontal>();
            row->fixed_height = 1;
            auto name = std::make_shared<Label>(StyledText().colored("overlay", Theme::current().foreground));
            name->fixed_width = 14;
            auto pb = std::make_shared<ProgressBar>();
            pb->value = (float)(P.overlayEntropy / 8.0);
            pb->color = entColor(P.overlayEntropy);
            row->add(name);
            row->add(pb);
            bars->add(row);
        }
        scroll->add(bars);

         
        const size_t cells = 96;
        size_t n = P.d.size();
        size_t blk = n / cells;
        if (blk < 512)
            blk = 512;
        std::vector<double> line;
        for (size_t i = 0; i < cells; i++)
        {
            size_t s = i * blk, len = blk;
            if (s >= n)
                break;
            if (s + len > n)
                len = n - s;
            line.push_back(entropyOf(&P.d[s], len) / 8.0);
        }
        auto hm = std::make_shared<Heatmap>();
        hm->show_values = false;
        hm->low_color = Theme::current().success;
        hm->high_color = Theme::current().error;
        hm->fixed_height = 3;
        hm->set_data({line});
        scroll->add(hm);

        auto legend = std::make_shared<Label>(StyledText()
                                                  .colored("green", Theme::current().success)
                                                  .add(" normal   ")
                                                  .colored("yellow", Theme::current().warning)
                                                  .add(" elevated   ")
                                                  .colored("red", Theme::current().error)
                                                  .add(" very high   ")
                                                  .colored(sfmt("each cell = %lu bytes", (unsigned long)blk), Theme::current().secondary));
        legend->fixed_height = 1;
        scroll->add(legend);
        return scroll;
    }

    std::shared_ptr<Widget> buildFindings()
    {
        using namespace pe;
        auto scroll = std::make_shared<ScrollableVertical>();

        auto g = std::make_shared<Gauge>(P.score / 100.0);
        g->fill_color = scoreColor(P.score);
        g->value_format = "%d%%";
        g->fixed_height = 6;
        g->fixed_width = 40;
        scroll->add(g);

        int cnt[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < P.findings.size(); i++)
            cnt[P.findings[i].sev]++;
        scroll->add(std::make_shared<Label>(StyledText()
                                                .colored(sfmt("  HIGH %d", cnt[SEV_HIGH]), Theme::current().error)
                                                .colored(sfmt("    MEDIUM %d", cnt[SEV_MED]), Theme::current().warning)
                                                .colored(sfmt("    LOW %d", cnt[SEV_LOW]), Theme::current().primary)
                                                .colored(sfmt("    INFO %d", cnt[SEV_INFO]), Theme::current().secondary)));

        auto p = std::make_shared<Paragraph>();
        p->word_wrap = true;
        StyledText st;
        for (size_t i = 0; i < P.findings.size(); i++)
        {
            const Finding &f = P.findings[i];
            const char *tag = (f.sev == SEV_HIGH)  ? "[HIGH]  "
                              : (f.sev == SEV_MED) ? "[MED]  "
                              : (f.sev == SEV_LOW) ? "[LOW]   "
                                                   : "[INFO]  ";
            st.colored(std::string("  ") + tag, sevColor(f.sev));
            st.colored(f.title + "\n", Theme::current().foreground);
            if (!f.detail.empty())
                st.colored("        " + f.detail + "\n", Theme::current().secondary);
        }
        if (P.findings.empty())
            st.colored("  No anomalies detected.\n", Theme::current().success);
        p->styled_content = st;
        scroll->add(p);

        if (!P.apiHits.empty())
        {
            scroll->add(std::make_shared<Label>(StyledText().colored("  Suspicious APIs by category",
                                                                     Theme::current().warning)));
            for (int c = 0; c < CAT_COUNT; c++)
            {
                std::string list;
                int n = 0;
                for (size_t i = 0; i < P.apiHits.size(); i++)
                {
                    if (P.apiHits[i].cat != c)
                        continue;
                    if (n)
                        list += ", ";
                    list += P.apiHits[i].fn;
                    n++;
                }
                if (!n)
                    continue;
                auto pp = std::make_shared<Paragraph>();
                pp->word_wrap = true;
                pp->styled_content = StyledText()
                                         .colored(sfmt("  %s (%d)\n", CAT_NAME[c], n), Theme::current().warning)
                                         .colored("      " + list + "\n", Theme::current().foreground);
                scroll->add(pp);
            }
        }
        return scroll;
    }

    std::shared_ptr<Widget> buildStructure()
    {
        using namespace pe;
        auto col = std::make_shared<Vertical>();

        uint32_t sa = P.secAlign ? P.secAlign : 0x1000;
        uint32_t fa = P.fileAlign ? P.fileAlign : 0x200;

        auto t = makeTable({"#", "Name", "VAddr mod SAlign", "RawPtr mod FAlign", "In header", "4K blocks"},
                           {4, 12, 18, 18, 12, 16});
        for (size_t i = 0; i < P.sec.size(); i++)
        {
            const Section &s = P.sec[i];
            uint32_t ra = sa ? (s.vaddr % sa) : 0;
            uint32_t rr = (fa && s.rawsize) ? (s.rawptr % fa) : 0;
            bool inHdr = (s.rawsize && s.rawptr && s.rawptr < P.sizeHdr);
            std::vector<StyledText> r;
            addCell(r, std::to_string(i + 1), Theme::current().secondary);
            addCell(r, s.name, Theme::current().foreground);
            addCell(r, ra ? H32(ra) : "ok", ra ? Theme::current().warning : Theme::current().success);
            addCell(r, rr ? H32(rr) : "ok", rr ? Theme::current().error : Theme::current().success);
            addCell(r, inHdr ? "SI" : "-", inHdr ? Theme::current().error : Theme::current().secondary);
            addCell(r, s.chunkTotal ? sfmt("%u/%u", s.chunkHigh, s.chunkTotal) : "n/a",
                    s.chunkHigh ? Theme::current().warning : Theme::current().secondary);
            t->rows.push_back(r);
        }
        t->fixed_height = 3 + (int)P.sec.size();
        col->add(t);

        auto p = std::make_shared<Paragraph>();
        p->word_wrap = true;
        StyledText st;
        int found = 0;
        for (size_t i = 0; i < P.sec.size(); i++)
            for (size_t j = i + 1; j < P.sec.size(); j++)
            {
                const Section &a = P.sec[i];
                const Section &b = P.sec[j];
                uint64_t aSpan = (uint64_t)a.vaddr + (a.vsize > a.rawsize ? a.vsize : a.rawsize);
                uint64_t bSpan = (uint64_t)b.vaddr + (b.vsize > b.rawsize ? b.vsize : b.rawsize);
                if (a.vsize && b.vsize && (uint64_t)a.vaddr < bSpan && (uint64_t)b.vaddr < aSpan)
                {
                    st.colored(sfmt("  Overlapping RVAs: %s / %s\n", a.name, b.name), Theme::current().error);
                    found++;
                }
            }
        std::vector<size_t> idx;
        for (size_t i = 0; i < P.sec.size(); i++)
            if (P.sec[i].rawsize)
                idx.push_back(i);
        std::sort(idx.begin(), idx.end(), [](size_t a, size_t b)
                  { return P.sec[a].rawptr < P.sec[b].rawptr; });
        for (size_t k = 0; k + 1 < idx.size(); k++)
        {
            const Section &a = P.sec[idx[k]];
            const Section &b = P.sec[idx[k + 1]];
            uint64_t gs = (uint64_t)a.rawptr + a.rawsize;
            if ((uint64_t)b.rawptr <= gs)
                continue;
            uint64_t len = b.rawptr - gs;
            if (len < 16)
                continue;
            st.colored(sfmt("  Gap between %s and %s: %lu bytes @ %s\n", a.name, b.name, (unsigned long)len,
                            H32((uint32_t)gs).c_str()),
                       len >= 0x200 ? Theme::current().warning : Theme::current().secondary);
            found++;
        }
        if (!found)
            st.colored("  No overlap or significant gap.\n", Theme::current().success);
        p->styled_content = st;
        col->add(p);
        return col;
    }

    std::shared_ptr<Widget> buildHex()
    {
        using namespace pe;
        auto col = std::make_shared<Vertical>();

        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto in = std::make_shared<Input>();
        in->placeholder = "offset or RVA in hex (e.g. 1A0, 0x400)";
        in->fixed_width = 40;
        auto go = std::make_shared<Button>("Vai", [this, in]()
                                           {
            std::string v = in->get_value();
            uint64_t o;
            if (!parseHex(v, o)) { notify("Invalid hexadecimal value", Notification::Type::Error); return; }
            if (o >= pe::P.d.size()) {
                long long fo = pe::rva2off((uint32_t)o);
                if (fo < 0) { notify("Offset or RVA out of file", Notification::Type::Error); return; }
                o = (uint64_t)fo;
            }
            hexOffText = H32((uint32_t)o);
            rebuild(); });
        go->fixed_width = 8;
        bar->add(in);
        bar->add(go);
        col->add(bar);

        uint64_t off = 0;
        parseHex(hexOffText, off);
        auto info = std::make_shared<Label>(StyledText().colored("  " + offsetInfo(off), Theme::current().foreground));
        info->fixed_height = 1;
        col->add(info);

        auto p = std::make_shared<Paragraph>();
        p->word_wrap = false;
        p->text = hexDumpText(off, 1024);
        auto sc = std::make_shared<ScrollableVertical>();
        sc->add(p);
        col->add(sc);
        return col;
    }

    std::shared_ptr<Widget> buildStrings()
    {
        using namespace pe;
        auto col = std::make_shared<Vertical>();

        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto in = std::make_shared<Input>();
        in->placeholder = "filter (empty = all)";
        auto t = makeTable({"Offset", "Type", "String"}, {12, 6, 90});

        auto scan = [t](const std::string &filter, unsigned minLen, bool ascii, bool wide)
        {
            t->rows.clear();
            size_t n = pe::P.d.size();
            unsigned total = 0;
            const unsigned CAP = 20000;
            std::string f = lower(filter);
            if (ascii)
            {
                size_t i = 0;
                while (i < n && total < CAP)
                {
                    size_t j = i;
                    while (j < n && ((pe::P.d[j] >= 32 && pe::P.d[j] < 127) || pe::P.d[j] == '\t'))
                        j++;
                    if (j - i >= minLen)
                    {
                        std::string s((const char *)&pe::P.d[i], j - i);
                        if (f.empty() || lower(s).find(f) != std::string::npos)
                        {
                            std::vector<StyledText> r;
                            addCell(r, sfmt("%08X", (unsigned)i), Theme::current().primary);
                            addCell(r, "A", Theme::current().secondary);
                            addCell(r, s.substr(0, 120), Theme::current().foreground);
                            t->rows.push_back(r);
                            total++;
                        }
                    }
                    i = (j == i) ? i + 1 : j;
                }
            }
            if (wide)
            {
                size_t i = 0;
                while (i + 1 < n && total < CAP)
                {
                    size_t j = i;
                    std::string s;
                    while (j + 1 < n && pe::P.d[j + 1] == 0 && pe::P.d[j] >= 32 && pe::P.d[j] < 127)
                    {
                        s += (char)pe::P.d[j];
                        j += 2;
                    }
                    if (s.size() >= minLen && (f.empty() || lower(s).find(f) != std::string::npos))
                    {
                        std::vector<StyledText> r;
                        addCell(r, sfmt("%08X", (unsigned)i), Theme::current().primary);
                        addCell(r, "W", Theme::current().secondary);
                        addCell(r, s.substr(0, 120), Theme::current().foreground);
                        t->rows.push_back(r);
                        total++;
                    }
                    i = (j == i) ? i + 1 : j;
                }
            }
            if (t->rows.empty())
            {
                std::vector<StyledText> r;
                addCell(r, "-", Theme::current().secondary);
                addCell(r, "-", Theme::current().secondary);
                addCell(r, "no strings found", Theme::current().warning);
                t->rows.push_back(r);
            }
        };

        auto run = std::make_shared<Button>("Search", [this, in, scan]()
                                            { scan(in->get_value(), strMin, asciiToo, wideToo); });
        run->fixed_width = 10;
        bar->add(in);
        bar->add(run);
        col->add(bar);

        auto opts = std::make_shared<Horizontal>();
        opts->fixed_height = 1;
        auto cA = std::make_shared<Checkbox>("ASCII", true);
        cA->fixed_width = 12;
        cA->on_change = [this](bool v)
        { asciiToo = v; };
        auto cW = std::make_shared<Checkbox>("UTF-16", true);
        cW->fixed_width = 12;
        cW->on_change = [this](bool v)
        { wideToo = v; };
        auto lMin = std::make_shared<Label>("  min length ");
        lMin->fixed_width = 20;
        auto nMin = std::make_shared<NumberInput>((int)strMin);
        nMin->min_value = 3;
        nMin->max_value = 200;
        nMin->fixed_width = 14;
        nMin->on_change = [this](int v)
        { strMin = (unsigned)v; };
        opts->add(cA);
        opts->add(cW);
        opts->add(lMin);
        opts->add(nMin);
        col->add(opts);

        scan("", strMin, true, true);
        col->add(t);
        return col;
    }

    static const char *const TAB_NAMES[10];

    std::shared_ptr<Widget> buildTab(int i)
    {
        switch (i)
        {
        case 0: return buildOverview();
        case 1: return buildSections();
        case 2: return buildDirs();
        case 3: return buildImports();
        case 4: return buildExports();
        case 5: return buildEntropy();
        case 6: return buildFindings();
        case 7: return buildStructure();
        case 8: return buildHex();
        default: return buildStrings();
        }
    }

    void showViewsMenu()
    {
        auto menu = std::make_shared<MenuDialog>(&app);
        menu->set_style(BorderStyle::Double);
        menu->set_title(" Views ");
        menu->shadow = true;
        menu->items.clear();
        for (int i = 0; i < 10; i++)
        {
            char key = (i == 9) ? '0' : (char)('1' + i);
            menu->items.push_back({std::string(1, key) + "  " + TAB_NAMES[i], [this, i]()
                                   { selectTab(i); }});
        }
        menu->show(4, 2);
    }

    std::shared_ptr<Widget> buildHome()
    {
        const Theme &t = Theme::current();
        auto scroll = std::make_shared<ScrollableVertical>();

        auto gap = [&]()
        {
            auto l = std::make_shared<Label>("");
            l->fixed_height = 1;
            return l;
        };
        auto head = [&](const std::string &s)
        {
            auto l = std::make_shared<Label>(StyledText().colored(" " + s, t.primary));
            l->fixed_height = 1;
            return l;
        };
        auto para = [&](const StyledText &st, int lines)
        {
            auto p = std::make_shared<Paragraph>(st);
            p->word_wrap = false;
            p->fixed_height = lines;
            return p;
        };

        scroll->add(gap());
        scroll->add(para(StyledText()
                             .colored_bold(std::string("  ") + CI_NAME "  ", t.primary)
                             .colored("v" CI_VERSION, t.secondary)
                             .add("\n")
                             .colored("  Windows PE static-analysis\n\n", t.secondary)
                             .colored("  Developed by ", t.secondary)
                             .colored(CI_AUTHOR, t.primary)
                             .colored("  -  files are parsed statically, never executed\n", t.secondary),
                         5));

        scroll->add(head("QUICK START"));
        scroll->add(para(StyledText()
                             .add("  1. Press F2 to browse for a file, or drag & drop an EXE / DLL / SYS\n"
                                  "     onto the window. You can also paste a full path in the dialog.\n"
                                  "  2. Analysis is fully static: the target is never executed.\n"
                                  "  3. Browse the 10 views with keys 1-0, or F9 / F10 to switch tabs.\n"
                                  "  4. F4 exports a report - F1 lists shortcuts - F5 changes theme -\n"
                                  "     F12 exits."),
                         7));

        scroll->add(gap());
        scroll->add(head("KEYBOARD SHORTCUTS"));
        {
            auto tb = makeTable({"Key", "Action"}, {10, 62});
            std::vector<std::pair<std::string, std::string>> rows = {
                {"F1", "Help - shortcut list"},
                {"F2", "Open file dialog (browse, type or paste path)"},
                {"F3", "Reload current file"},
                {"F4", "Export analysis report (.txt)"},
                {"F5", "Switch color theme"},
                {"F6 / F11", "Home - close file and show welcome screen"},
                {"F7", "Go to offset / RVA (hex view)"},
                {"F8", "Copy hashes to clipboard (MD5, SHA-256, imphash)"},
                {"F9", "Previous view"},
                {"F10", "Next view"},
                {"F11", "Home (alias of F6)"},
                {"F12", "Exit application"},
                {"1 - 0", "Jump directly to view 1 - 10"},
                {"Enter", "Confirm dialog / submit path"},
                {"Esc", "Close dialog / cancel"},
                {"Mouse", "Click and double-click supported everywhere"},
            };
            for (auto &r : rows)
            {
                std::vector<StyledText> sr;
                sr.push_back(StyledText().colored(r.first, t.primary));
                sr.push_back(StyledText().add(r.second));
                tb->rows.push_back(sr);
            }
            tb->fixed_height = (int)tb->rows.size() + 1;
            scroll->add(tb);
        }

        scroll->add(gap());
        scroll->add(head("VIEWS"));
        {
            auto tb = makeTable({"Key", "View", "Content"}, {6, 12, 48});
            std::vector<std::vector<std::string>> rows = {
                {"1", "Overview", "Hashes, file type, suspicion score"},
                {"2", "Sections", "Sections, permissions, entropy"},
                {"3", "Directories", "Data directories status"},
                {"4", "Imports", "Imports by DLL, imphash"},
                {"5", "Exports", "Export table, forwarders"},
                {"6", "Entropy", "Per-section entropy chart"},
                {"7", "Anomalies", "Heuristic findings"},
                {"8", "Structure", "Header and section validation"},
                {"9", "Hex", "Hex dump (F7 to jump to offset)"},
                {"0", "Strings", "ASCII / UTF-16 strings + filter"},
            };
            for (auto &r : rows)
                tb->add_row(r);
            tb->fixed_height = (int)tb->rows.size() + 1;
            scroll->add(tb);
        }

        scroll->add(gap());
        scroll->add(head("ANALYSIS FEATURES"));
        scroll->add(para(StyledText()
                             .add("  - PE32 / PE32+ headers, sections and data directories\n"
                                  "  - Import & delay-load tables, ordinal imports, imphash\n"
                                  "  - Export table with forwarded-export detection\n"
                                  "  - Per-section entropy and packing indicators\n"
                                  "  - Suspicious-API detection and heuristic score (0-100)\n"
                                  "  - Authenticode signature, PDB path, TLS callbacks\n"
                                  "  - Rich header, overlay and checksum analysis\n"
                                  "  - ASCII and UTF-16LE string extraction\n"
                                  "  - Fully static: files are read from disk, never executed"),
                         9));

        scroll->add(gap());
        scroll->add(para(StyledText()
                             .colored("  " + std::string(CI_NAME) + " v" CI_VERSION "  by " CI_AUTHOR "  -  Developed with cpp-tui",
                                      t.secondary),
                         1));
        scroll->add(gap());
        return scroll;
    }

    void rebuild()
    {
        if (!contentHost)
            return;
        contentHost->clear_children();
        currentTabs.reset();
        updateHeader();
        if (!pe::P.loaded)
        {
            if (splashActive && splash)
            {
                contentHost->add(splash);
                return;
            }
            contentHost->add(buildHome());
            return;
        }

        auto tabs = std::make_shared<Tabs>();
        for (int i = 0; i < 10; i++)
            tabs->add_tab(TAB_NAMES[i], buildTab(i));
        contentHost->add(tabs);
        currentTabs = tabs;
        if (activeTabIdx >= 0 && activeTabIdx < 10)
            tabs->select_tab(activeTabIdx);
    }

     
    void showMenu(int x, int y)
    {
        auto menu = std::make_shared<MenuDialog>(&app);
        menu->set_style(BorderStyle::Double);
        menu->set_title(" Menu ");
        menu->shadow = true;
        menu->items = {
            {"Open file...          F2", [this]()
             { openFileDialog(); }},
            {"Reload                F3", [this]()
             { if (pe::P.loaded) loadAsync(pe::P.path); }},
            {"Export report...      F4", [this]()
             { exportDialog(); }},
            {"Home / close file   F6", [this]()
             { closeCurrentFile(); }},
            {"Go to offset / RVA   F7", [this]()
             { showGoToDialog(); }},
            {"Copy hashes          F8", [this]()
             { copyHashesToClipboard(); }},
            {"Cycle theme          F5", [this]()
             { cycleTheme(); }},
            {"Keyboard shortcuts    F1", [this]()
             { showHelp(); }},
            {"About", [this]()
             { showAbout(); }},
            {"Exit                 F12", []()
             { App::quit(); }}};
        menu->show(x, y);
    }

    void exportDialog()
    {
        if (!pe::P.loaded)
        {
            notify("No file loaded", Notification::Type::Warning);
            return;
        }
        std::string def = baseName(pe::P.path) + ".codeinquisitor.txt";
        auto d = std::make_shared<CiDialog>(&app, BorderStyle::Double);
        d->set_title(" Export report ", Alignment::Center);
        d->modal = true;
        d->fixed_width = 60;
        d->fixed_height = 7;
        auto in = std::make_shared<Input>();
        in->set_text(def);
        auto col = std::make_shared<Vertical>();
        col->add(in);
        auto bar = std::make_shared<Horizontal>();
        bar->fixed_height = 1;
        auto doSave = [this, d, in]()
        {
            std::string p = stripQuotes(in->get_value());
            if (p.empty()) return;
            if (writeReport(p)) notify("Report written: " + baseName(p), Notification::Type::Success);
            else notify("Cannot write file", Notification::Type::Error);
            app.close_dialog(d);
        };
        auto ok = std::make_shared<Button>("Save", doSave);
        d->on_enter = doSave;
        ok->fixed_width = 12;
        auto no = std::make_shared<Button>("Cancel", [this, d]()
                                           { app.close_dialog(d); });
        no->fixed_width = 12;
        bar->add(ok);
        bar->add(no);
        col->add(bar);
        d->add(col);
        app.open_dialog(d);
    }

     
    int run(const std::string &initial)
    {
        Theme::set_theme(Theme::Dark());

        root = std::make_shared<Vertical>();

        auto menuBar = std::make_shared<MenuBar>();
        menuBar->fixed_height = 1;
        menuBar->items = {
            {"File", [this, menuBar]()
             { showMenu(1, 1); }},
            {"Views", [this]()
             { showViewsMenu(); }},
            {"Help", [this]()
             { showAbout(); }}};
        root->add(menuBar);

        header = std::make_shared<StatusBar>();
        header->separator = " ";
        root->add(header);

        notif = std::make_shared<Notification>();
        notif->position = Notification::Position::TopRight;
        app.set_notification(notif);

        contentHost = std::make_shared<Vertical>();
        auto overlay = std::make_shared<Stack>();
        overlay->add(contentHost);
        overlay->add(notif);
        root->add(overlay);

        auto footer = std::make_shared<ShortcutBar>();
        footer->fixed_height = 1;
        footer->spacing = 2;
        footer->add("F1", "Help");
        footer->add("F2", "Open");
        footer->add("F4", "Report");
        footer->add("F5", "Theme");
        footer->add("F6", "Home");
        footer->add("1-0", "Views");
        footer->add("F12", "Quit");
        footer->add("", StyledText().colored(std::string(CI_NAME) + " v" CI_VERSION + "  Developed by " CI_AUTHOR,
                                             Theme::current().secondary));
        root->add(footer);

        splashActive = initial.empty();
        if (splashActive)
            splash = std::make_shared<SplashWidget>();

        updateHeader();
        rebuild();

        app.register_key(1011, [this]()
                         { showHelp(); });
        app.register_key(1012, [this]()
                         { openFileDialog(); });
        app.register_key(1013, [this]()
                         { if (pe::P.loaded) loadAsync(pe::P.path); });
        app.register_key(1014, [this]()
                         { exportDialog(); });
        app.register_key(1015, [this]()
                         { cycleTheme(); });
        app.register_key(1016, [this]()
                         { closeCurrentFile(); });
        app.register_key(1017, [this]()
                         { showGoToDialog(); });
        app.register_key(1018, [this]()
                         { copyHashesToClipboard(); });
        app.register_key(1019, [this]()
                         { selectTab((activeTabIdx + 9) % 10); });
        app.register_key(1020, [this]()
                         { selectTab((activeTabIdx + 1) % 10); });
        app.register_key(1021, [this]()
                         { closeCurrentFile(); });
        app.register_key(1022, []()
                         { App::quit(); });

        if (splashActive)
        {
            splashStart = std::chrono::steady_clock::now();
            app.add_timer(50, [this]()
                          {
                if (!splash)
                    return;
                float el = std::chrono::duration<float>(
                               std::chrono::steady_clock::now() - splashStart)
                               .count();
                splash->progress = el / 3.0f;
                if (splash->progress > 1.0f)
                    splash->progress = 1.0f;
                if (el >= 3.0f)
                    finishSplash(); });
        }
        auto onEnterKey = [this]()
        {
            if (openDlg) { submitOpen(); return; }
            if (!app.dialog_stack.empty())
            {
                auto d = app.dialog_stack.back();
                if (auto cd = std::dynamic_pointer_cast<CiDialog>(d))
                    if (cd->on_enter) cd->on_enter();
            }
        };
        auto onEscKey = [this]()
        {
            if (openDlg) { closeFileDialog(); return; }
            if (!app.dialog_stack.empty())
            {
                auto d = app.dialog_stack.back();
                if (std::dynamic_pointer_cast<CiDialog>(d))
                    app.close_dialog(d);
            }
        };
        app.register_key(13, onEnterKey, false, false, false, false);
        app.register_key(10, onEnterKey, false, false, false, false);
        app.register_key(27, onEscKey, false, false, false, false);
        app.register_exit_key('q', true);
        app.pre_event_hook = [this](const Event &ev)
        { return preEvent(ev); };

        if (!initial.empty())
        {
             
            app.add_timer(120, [this, initial]()
                          {
                static bool done = false;
                if (done) return;
                done = true;
                loadAsync(initial); });
        }

        app.run(root);
        return 0;
    }
};

 
 
 

const char *const CodeInquisitor::TAB_NAMES[10] = {
    "Overview", "Sections", "Directories", "Imports", "Exports",
    "Entropy", "Anomalies", "Structure", "Hex", "Strings"};

int main(int argc, char **argv)
{
    std::string argFile, reportFile;
    bool doReport = false;

    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "-h" || a == "--help")
        {
            printf("%s v%s - PE Static Triage Console\nDeveloped by %s\n\n", CI_NAME, CI_VERSION, CI_AUTHOR);
            printf("Usage:\n  %s [file]\n  %s --report <file> [out.txt]\n", CI_NAME, CI_NAME);
            return 0;
        }
        else if (a == "--report")
        {
            doReport = true;
            if (i + 1 < argc)
                argFile = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-')
                reportFile = argv[++i];
        }
        else if (argFile.empty())
        {
            argFile = a;
        }
    }

    if (doReport)
    {
        if (argFile.empty())
        {
            printf("Missing file to analyze.\n");
            return 1;
        }
        if (!pe::load(argFile))
        {
            fprintf(stderr, "Error: %s\n", pe::P.err.c_str());
            return 2;
        }
        if (reportFile.empty())
            reportFile = baseName(argFile) + ".codeinquisitor.txt";
        if (!writeReport(reportFile))
        {
            fprintf(stderr, "Cannot write %s\n", reportFile.c_str());
            return 3;
        }
        printf("Report written to: %s  (suspicion %d/100 - %s)\n", reportFile.c_str(), pe::P.score,
               pe::scoreLabel(pe::P.score));
        return 0;
    }

    CodeInquisitor tool;
    return tool.run(stripQuotes(argFile));
}
