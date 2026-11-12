#pragma once
#include "transport.hpp"
#include <functional>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <windowsx.h>
namespace cobalt {
inline constexpr int editorWidth = 1440, editorHeight = 1080;
inline std::wstring wide(const std::string &s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring v(size_t(n), 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, v.data(), n);
    if (!v.empty())
        v.pop_back();
    return v;
}
struct Rect {
    float x, y, w, h;
    bool contains(float a, float b) const {
        return a >= x && a < x + w && b >= y && b < y + h;
    }
};
struct Item {
    int id;
    Style style;
    Rect bounds;
    bool below = false;
    std::string label;
};
class Editor {
    Model &model;
    Transport &transport;
    HWND window = nullptr;
    HINSTANCE instance = nullptr;
    std::wstring className;
    ULONG_PTR token = 0;
    std::vector<Item> items;
    std::array<int, 256> observed{};
    std::string observedStatus;
    int drag = -1, focused = 0, popup = -1;
    float startY = 0, startX = 0;
    int startValue = 0;
    ULONGLONG popupUntil = 0;
    std::wstring selected = L"COBALT8";
    const Gdiplus::Color bg{255, 17, 25, 35}, panelColor{255, 26, 37, 51}, field{255, 17, 27, 40},
        line{255, 47, 65, 84}, ink{255, 230, 237, 246}, muted{255, 156, 176, 199},
        accent{255, 108, 156, 221};
    void fill(Gdiplus::Graphics &g, Rect r, Gdiplus::Color c) {
        Gdiplus::SolidBrush b(c);
        g.FillRectangle(&b, r.x, r.y, r.w, r.h);
    }
    void text(Gdiplus::Graphics &g, const std::string &s, Rect r, float size = 12,
              Gdiplus::Color c = Gdiplus::Color(255, 230, 237, 246), bool center = false) {
        Gdiplus::FontFamily family(L"Segoe UI");
        Gdiplus::Font font(&family, size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush b(c);
        Gdiplus::StringFormat f;
        f.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        f.SetAlignment(center ? Gdiplus::StringAlignmentCenter : Gdiplus::StringAlignmentNear);
        f.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        auto w = wide(s);
        g.DrawString(w.c_str(), -1, &font, Gdiplus::RectF(r.x, r.y, r.w, r.h), &f, &b);
    }
    void panel(Gdiplus::Graphics &g, Rect r, const std::string &title) {
        fill(g, r, panelColor);
        Gdiplus::Pen p(line);
        g.DrawRectangle(&p, r.x, r.y, r.w, r.h);
        text(g, title, {r.x + 12, r.y + 6, r.w - 24, 22}, 14);
    }
    void add(int id, Rect r, Style style, bool below = false, std::string label = "") {
        auto *d = definition(id);
        if (d)
            items.push_back({id, style, r, below, label.empty() ? d->name : std::move(label)});
    }
    void layout() {
        items.clear();
        for (int n = 0; n < 2; ++n) {
            float x = 12 + n * 476.f;
            add(62 + n, {x + 12, 86, 440, 27}, Style::Select);
            const int ids[2][4] = {{14, 15, 16, 19}, {30, 31, 17, 20}};
            for (int j = 0; j < 4; ++j)
                add(ids[n][j], {x + 12 + j * 110, 211, 100, 44}, Style::Horizontal);
        }
        float x = 964;
        add(42, {x + 12, 86, 440, 26}, Style::Select);
        add(195, {x + 12, 119, 140, 24}, Style::Select);
        add(197, {x + 160, 119, 150, 24}, Style::Select);
        int knobs[] = {34, 35, 33, 18, 9, 151, 152, 5, 196};
        for (int j = 0; j < 9; ++j)
            add(knobs[j], {x + 12 + (j % 5) * 88.f, 151 + (j / 5) * 61.f, 82, 56}, Style::Knob,
                true);
        const int eg[3][5] = {{26, 27, 28, 29, 51}, {22, 23, 24, 25, 32}, {43, 44, 45, 46, 49}};
        for (int n = 0; n < 3; ++n) {
            float ex = 12 + n * 476.f;
            add(80 + n, {ex + 320, 302, 132, 24}, Style::Select);
            for (int j = 0; j < 5; ++j)
                add(eg[n][j], {ex + 20 + j * 88.f, 402, 70, 94}, Style::Vertical);
        }
        for (int n = 0; n < 8; ++n) {
            float y = 568 + n * 26.f;
            add(100 + n, {42, y, 172, 23}, Style::Select);
            add(108 + n, {222, y, 254, 23}, Style::Select);
            add(88 + n, {488, y, 278, 23}, Style::Horizontal);
        }
        for (int i = 0; i < 4; ++i)
            add(163 + i, {28 + (i % 2) * 380.f, 790 + (i / 2) * 28.f, 360, 27}, Style::Horizontal);
        const int rate[] = {36, 47, 3}, dep[] = {37, 48, 13}, shape[] = {39, 50, 21},
                  mode[] = {56, 57, 54};
        for (int n = 0; n < 3; ++n) {
            float y = 556 + n * 57.f;
            add(shape[n], {872, y + 6, 130, 23}, Style::Select);
            add(mode[n], {1010, y + 6, 104, 23}, Style::Select);
            add(rate[n], {1122, y, 126, 44}, Style::Knob);
            add(dep[n], {1258, y, 150, 43}, Style::Horizontal);
        }
        for (int i = 0; i < 8; ++i)
            add(155 + i, {826 + (i % 4) * 147.f, 748 + (i / 4) * 43.f, 134, 41}, Style::Horizontal);
        for (int n = 0; n < 3; ++n) {
            float fx = 12 + n * 476.f;
            add(128 + 7 * n, {fx + 12, 897, 440, 25}, Style::Select);
            for (int j = 0; j < 6; ++j)
                add(129 + 7 * n + j, {fx + 12 + (j % 3) * 148.f, 930 + (j / 3) * 43.f, 138, 40},
                    Style::Knob);
        }
        const int outputs[] = {73, 119, 149, 83};
        for (int j = 0; j < 4; ++j)
            add(outputs[j], {16 + j * 356.f, 1021, 338, 26}, Style::Horizontal);
    }
    std::string label(const Item &c) {
        if (c.id >= 129 && c.id <= 148) {
            int n = (c.id - 128) / 7, fieldIndex = (c.id - 128) % 7 - 1;
            if (fieldIndex >= 0)
                return effectLabels[std::clamp(model.get(128 + n * 7), 0, 26)][fieldIndex];
        }
        return c.label;
    }
    void button(Gdiplus::Graphics &g, Rect r, const std::string &label) {
        fill(g, r, field);
        Gdiplus::Pen p(line);
        g.DrawRectangle(&p, r.x, r.y, r.w, r.h);
        text(g, label, r, 12, ink, true);
    }
    void control(Gdiplus::Graphics &g, const Item &c, int index) {
        auto *d = definition(c.id);
        int value = model.get(c.id);
        float fraction = float(value) / std::max(1, d->maximum);
        auto r = c.bounds;
        Gdiplus::Pen track(line, 3), signal(accent, 2);
        Gdiplus::SolidBrush color(accent);
        if (c.style == Style::Select) {
            fill(g, r, field);
            Gdiplus::Pen edge(line);
            g.DrawRectangle(&edge, r.x, r.y, r.w, r.h);
            std::string s = format(c.id, value);
            if (c.id == 195)
                s = "Free run: " + s;
            if (c.id == 197)
                s = "Scaling: " + s;
            text(g, s, {r.x + 7, r.y, r.w - 21, r.h}, 12);
            text(g, "v", {r.x + r.w - 17, r.y, 14, r.h}, 10, muted, true);
        } else if (c.style == Style::Knob) {
            float cx = c.below ? r.x + r.w / 2 : r.x + r.w - 20,
                  cy = c.below ? r.y + 19 : r.y + r.h / 2;
            Gdiplus::SolidBrush face(field);
            g.FillEllipse(&face, cx - 17, cy - 17, 34.f, 34.f);
            Gdiplus::Pen edge(line);
            g.DrawEllipse(&edge, cx - 17, cy - 17, 34.f, 34.f);
            double a = (-225 + 270 * fraction) * 3.141592653589793 / 180;
            g.DrawLine(&signal, cx + float(std::cos(a)) * 7, cy + float(std::sin(a)) * 7,
                       cx + float(std::cos(a)) * 13, cy + float(std::sin(a)) * 13);
            if (c.below)
                text(g, label(c), {r.x, r.y + 38, r.w, 17}, 11, muted, true);
            else
                text(g, label(c), {r.x, r.y, r.w - 43, r.h}, 11, muted);
        } else if (c.style == Style::Vertical) {
            float cx = r.x + r.w / 2, top = r.y + 23, bottom = r.y + r.h - 18;
            text(g, label(c), {r.x, r.y, r.w, 18}, 12, muted, true);
            g.DrawLine(&track, cx, top, cx, bottom);
            float y = bottom - fraction * (bottom - top);
            g.FillRectangle(&color, cx - 9, y - 2, 18.f, 4.f);
            text(g, format(c.id, value), {r.x, r.y + r.h - 17, r.w, 17}, 11, muted, true);
        } else {
            bool compact = r.h < 30, matrixAmount = c.id >= 88 && c.id <= 95;
            float y = matrixAmount ? r.y + r.h / 2 : compact ? r.y + r.h - 6 : r.y + 25;
            float start = r.x, end = r.x + r.w - (compact ? 36 : 0);
            if (!matrixAmount) {
                text(g, label(c), {r.x, r.y, r.w - 35, compact ? 15.f : 18.f}, 11, muted);
            }
            g.DrawLine(&track, start, y, end, y);
            g.FillEllipse(&color, start + fraction * (end - start) - 4, y - 4, 8.f, 8.f);
            text(g, format(c.id, value),
                 {compact ? end + 4 : r.x, compact ? r.y : r.y + 31, compact ? 32.f : r.w, 16}, 11,
                 muted, !compact);
        }
        if (index == focused && GetFocus() == window) {
            Gdiplus::Pen p(accent);
            g.DrawRectangle(&p, r.x - 2, r.y - 2, r.w + 4, r.h + 4);
        }
    }
    static double wave(int family, double p) {
        p -= std::floor(p);
        if (family == 0)
            return 2 * p - 1;
        if (family == 1)
            return p < .5 ? 1 : -1;
        if (family == 2)
            return 1 - 4 * std::abs(p - .5);
        return std::sin(p * 6.28318530718);
    }
    void oscillator(Gdiplus::Graphics &g, int n, Rect r) {
        fill(g, r, field);
        Gdiplus::Pen grid(line);
        g.DrawLine(&grid, r.x, r.y + r.h / 2, r.x + r.w, r.y + r.h / 2);
        int alg = model.get(62 + n);
        double a = model.get(n ? 17 : 16) / 127., b = model.get(n ? 20 : 19) / 127.;
        std::vector<Gdiplus::PointF> points;
        double last = 0;
        for (int i = 0; i <= 400; ++i) {
            double p = i / 133., y = 0;
            if (alg <= 1) {
                double morph = a * 3;
                int f = int(morph);
                y = (1 - (morph - f)) * wave(f % 4, p) + (morph - f) * wave((f + 1) % 4, p);
                if (alg == 1) {
                    double steps = 2 + std::floor(b * 30);
                    y = std::round(y * steps) / steps;
                }
            } else if (alg <= 4) {
                int family = alg - 2;
                y = wave(family, p);
                for (int k = 1; k < 4; ++k)
                    y += a * wave(family, p * (1 + (k - 2) * .015 * b) + k * .19 * b);
                y /= 1 + 3 * a;
            } else if (alg <= 9)
                y = wave(1, p + (alg == 6 ? b * .25 * std::sin(p * 6.28) : 0)) *
                    (p - std::floor(p) < .05 + .9 * a ? 1 : -1);
            else if (alg <= 12)
                y = wave(alg - 10, p * (1 + 7 * a));
            else if (alg <= 15)
                y = (wave(alg - 13, p) + a * wave(alg - 13, p * (2 + 3 * b))) / (1 + a);
            else if (alg <= 18)
                y = wave(alg - 16, p + (p - std::floor(p) > a ? b : -b));
            else if (alg == 19)
                y = wave(0, p * (1 + 7 * a)) * (.5 + .5 * std::sin(p * 6.28));
            else if (alg <= 22)
                y = wave(alg - 20, p + a * std::sin(p * 6.283 * (1 + 5 * b)));
            else if (alg <= 28)
                y = wave((alg - 23) % 3, p) * wave((alg - 22) % 3, p * (1 + 5 * a) + b);
            else if (alg <= 31)
                y = wave((alg - 29) % 3, p + a * std::sin(p * 6.283 * (1 + 6 * b)));
            else if (alg <= 34) {
                double noise = std::sin(i * 91.731 + 23) * 43758.5453;
                noise = (noise - std::floor(noise)) * 2 - 1;
                y = wave(alg - 32, p) * ((1 - a) + a * noise);
            } else if (alg <= 37) {
                y = wave(alg - 35, p + a * last * (.1 + b));
                last = y;
            } else if (alg == 38)
                y = std::sin(wave(2, p) * (1 + 8 * a));
            else {
                double noise = std::sin(i * 91.731 + 23) * 43758.5453;
                noise = (noise - std::floor(noise)) * 2 - 1;
                last += (noise - last) * (.05 + .9 * a);
                y = last;
            }
            points.emplace_back(r.x + float(i) * r.w / 400, r.y + r.h / 2 - float(y) * r.h * .40f);
        }
        Gdiplus::Pen p(accent, 1.5f);
        g.DrawLines(&p, points.data(), int(points.size()));
    }
    void envelope(Gdiplus::Graphics &g, int n, Rect r) {
        const int ids[3][5] = {{26, 27, 28, 29, 51}, {22, 23, 24, 25, 32}, {43, 44, 45, 46, 49}};
        auto *id = ids[n];
        int type = model.get(80 + n) % 4;
        double factor = model.get(80 + n) >= 4 ? 2 : 1;
        double a = (10 + model.get(id[0]) * .4) * factor, d = (10 + model.get(id[1]) * .4) * factor,
               release = (10 + model.get(id[3]) * .4) * factor, hold = 40,
               total = a + d + release + hold;
        double sustain = model.get(id[2]) / 127.,
               depth =
                   n ? std::clamp((model.get(id[4]) - 64) / 63., -1., 1.) : model.get(id[4]) / 127.;
        float base = n ? r.y + r.h / 2 : r.y + r.h - 5, amp = n ? r.h * .43f : r.h * .84f;
        Gdiplus::Pen grid(line);
        g.DrawLine(&grid, r.x, base, r.x + r.w, base);
        auto attack = [&](double t) {
            return type == 0   ? 1 - std::pow(1 - t, 2.5)
                   : type == 1 ? 1 - std::pow(1 - t, 6)
                   : type == 2 ? t * t * (3 - 2 * t)
                               : t;
        };
        auto fall = [&](double t) {
            return type == 0   ? std::pow(1 - t, 2.5)
                   : type == 1 ? std::pow(1 - t, 6)
                   : type == 2 ? 1 - t * t * (3 - 2 * t)
                               : 1 - t;
        };
        std::vector<Gdiplus::PointF> pts;
        auto point = [&](double x, double y) {
            pts.emplace_back(r.x + float(x / total) * r.w, base - float(y * depth) * amp);
        };
        point(0, 0);
        for (int j = 1; j <= 40; ++j) {
            point(a * j / 40, attack(j / 40.));
        }
        for (int j = 1; j <= 40; ++j) {
            point(a + d * j / 40, sustain + (1 - sustain) * fall(j / 40.));
        }
        point(a + d + hold, sustain);
        for (int j = 1; j <= 40; ++j) {
            point(a + d + hold + release * j / 40, sustain * fall(j / 40.));
        }
        Gdiplus::Pen p(accent, 1.5);
        g.DrawLines(&p, pts.data(), int(pts.size()));
    }
    void paint(HDC dc) {
        Gdiplus::Bitmap image(editorWidth, editorHeight, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&image);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        g.Clear(bg);
        text(g, "COBALT / EDITOR", {16, 5, 350, 23}, 20);
        text(g, "HARDWARE INSTRUMENT  /  0.1.0 ALPHA", {16, 28, 400, 16}, 10, muted);
        std::string device(selected.begin(), selected.end());
        button(g, {474, 12, 220, 27}, device);
        button(g, {704, 12, 72, 27}, "CH " + std::to_string(transport.channel.load() + 1));
        button(g, {786, 12, 190, 27},
               transport.configuration().second ? "Disconnect" : "Connect + Send All");
        button(g, {986, 12, 100, 27}, "Send All");
        button(g, {1096, 12, 80, 27}, "Panic");
        text(g, transport.connected ? "USB output connected" : "USB output disconnected",
             {1190, 10, 230, 30}, 12, muted);
        for (int n = 0; n < 3; ++n) {
            float x = 12 + n * 476.f;
            panel(g, {x, 54, 464, 228},
                  n < 2 ? "Oscillator " + std::to_string(n + 1) : "Filter & Voice");
            panel(g, {x, 294, 464, 214},
                  n == 0   ? "Amp envelope"
                  : n == 1 ? "Filter envelope"
                           : "Mod envelope");
            envelope(g, n, {x + 20, 336, 424, 56});
        }
        oscillator(g, 0, {24, 125, 440, 70});
        oscillator(g, 1, {500, 125, 440, 70});
        panel(g, {12, 520, 786, 332}, "Modulation matrix");
        text(g, "Source", {42, 548, 170, 18}, 11, muted);
        text(g, "Destination", {222, 548, 254, 18}, 11, muted);
        text(g, "Amount", {488, 548, 260, 18}, 11, muted);
        for (int i = 0; i < 8; ++i)
            text(g, std::to_string(i + 1), {22, 568 + i * 26.f, 16, 23}, 11, muted);
        text(g, "FIXED MODULATION", {28, 776, 500, 15}, 10, muted);
        panel(g, {810, 520, 618, 332}, "LFOs & source depths");
        for (int n = 0; n < 3; ++n)
            text(g, "LFO " + std::to_string(n + 1), {822, 559 + n * 57.f, 49, 32}, 12);
        text(g, "SOURCE DEPTHS", {826, 729, 500, 16}, 10, muted);
        for (int n = 0; n < 3; ++n)
            panel(g, {12 + n * 476.f, 864, 464, 148}, "Effect " + std::to_string(n + 1));
        for (int i = 0; i < int(items.size()); ++i)
            control(g, items[i], i);
        text(g, transport.status(), {16, 1053, 1050, 23}, 11, muted);
        text(g, "Illustrative waveforms / separate audio return", {1065, 1053, 360, 23}, 10, muted);
        if (popup >= 0 && popup < int(items.size()) && GetTickCount64() < popupUntil) {
            auto &c = items[popup];
            Rect box{std::clamp(c.bounds.x + c.bounds.w - 90, 8.f, editorWidth - 160.f),
                     std::max(49.f, c.bounds.y - 30), 148, 25};
            fill(g, box, field);
            Gdiplus::Pen p(accent);
            g.DrawRectangle(&p, box.x, box.y, box.w, box.h);
            text(g, format(c.id, model.get(c.id)), box, 12, ink, true);
        }
        Gdiplus::Graphics target(dc);
        target.DrawImage(&image, 0, 0);
    }
    void set(int index, int value) {
        if (index < 0 || index >= int(items.size()))
            return;
        auto &c = items[index];
        change(c.id, valid(c.id, value));
        popup = index;
        popupUntil = GetTickCount64() + 850;
        InvalidateRect(window, nullptr, FALSE);
    }
    void menu(int index) {
        auto &c = items[index];
        auto *d = definition(c.id);
        HMENU m = CreatePopupMenu();
        for (size_t i = 0; i < d->choices.size(); ++i) {
            auto w = wide(d->choices[i].text);
            AppendMenuW(m, MF_STRING | (model.get(c.id) == d->choices[i].value ? MF_CHECKED : 0),
                        UINT_PTR(i + 1), w.c_str());
        }
        POINT pos{LONG(c.bounds.x), LONG(c.bounds.y + c.bounds.h)};
        ClientToScreen(window, &pos);
        int result =
            TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pos.x, pos.y, 0, window, nullptr);
        DestroyMenu(m);
        if (result > 0 && begin(c.id)) {
            set(index, d->choices[size_t(result - 1)].value);
            end(c.id);
        }
    }
    void header(int x, int y) {
        if (y < 12 || y > 39)
            return;
        if (x >= 474 && x < 694) {
            auto names = outputNames();
            HMENU m = CreatePopupMenu();
            for (size_t i = 0; i < names.size(); ++i)
                AppendMenuW(m, MF_STRING, UINT_PTR(i + 1), names[i].c_str());
            POINT p{x, y};
            ClientToScreen(window, &p);
            int result =
                TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, window, nullptr);
            DestroyMenu(m);
            if (result > 0) {
                selected = names[size_t(result - 1)];
                transport.configure(selected, false);
                dirtyState();
            }
        } else if (x >= 704 && x < 776) {
            HMENU m = CreatePopupMenu();
            for (int i = 0; i < 16; ++i) {
                auto t = std::to_wstring(i + 1);
                AppendMenuW(m, MF_STRING | (transport.channel == i ? MF_CHECKED : 0), i + 1,
                            t.c_str());
            }
            POINT p{x, y};
            ClientToScreen(window, &p);
            int result =
                TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, window, nullptr);
            DestroyMenu(m);
            if (result) {
                transport.panic();
                transport.channel = result - 1;
                model.sendAll();
                dirtyState();
            }
        } else if (x >= 786 && x < 976) {
            auto config = transport.configuration();
            transport.configure(selected, !config.second);
            dirtyState();
        } else if (x >= 986 && x < 1086) {
            model.sendAll();
        } else if (x >= 1096 && x < 1176) {
            transport.panic();
        }
        InvalidateRect(window, nullptr, FALSE);
    }
    LRESULT message(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(window, &ps);
            paint(dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            paint(reinterpret_cast<HDC>(wp));
            return 0;
        case WM_TIMER: {
            bool changed = false;
            for (auto &d : definitions()) {
                int v = model.get(d.id);
                if (observed[d.id] != v) {
                    observed[d.id] = v;
                    changed = true;
                }
            }
            auto status = transport.status();
            if (status != observedStatus) {
                observedStatus = status;
                changed = true;
            }
            auto config = transport.configuration();
            if (!config.first.empty() && config.first != selected) {
                selected = config.first;
                changed = true;
            }
            if (popup >= 0 && GetTickCount64() >= popupUntil) {
                popup = -1;
                changed = true;
            }
            if (changed)
                InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(window);
            float x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (y < 48) {
                header(int(x), int(y));
                return 0;
            }
            for (int i = 0; i < int(items.size()); ++i) {
                if (items[i].bounds.contains(x, y)) {
                    focused = i;
                    auto &c = items[i];
                    if (c.style == Style::Select) {
                        menu(i);
                        return 0;
                    }
                    if (begin(c.id)) {
                        drag = i;
                        startY = y;
                        startX = x;
                        startValue = model.get(c.id);
                        SetCapture(window);
                        popup = i;
                        popupUntil = GetTickCount64() + 850;
                    }
                    break;
                }
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (drag >= 0) {
                float y = GET_Y_LPARAM(lp), x = GET_X_LPARAM(lp);
                float motion = items[drag].style == Style::Horizontal ? x - startX : startY - y;
                int delta = int(std::round(motion * ((GetKeyState(VK_SHIFT) & 0x8000) ? .15 : .8)));
                set(drag, startValue + delta);
            }
            return 0;
        case WM_LBUTTONUP:
            if (drag >= 0) {
                end(items[drag].id);
                drag = -1;
                ReleaseCapture();
                popupUntil = GetTickCount64() + 850;
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (drag >= 0) {
                end(items[drag].id);
                drag = -1;
            }
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(window, &pt);
            for (int i = 0; i < int(items.size()); ++i)
                if (items[i].bounds.contains(float(pt.x), float(pt.y)) &&
                    items[i].style != Style::Select && begin(items[i].id)) {
                    set(i, model.get(items[i].id) + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1));
                    end(items[i].id);
                    break;
                }
            return 0;
        }
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTTAB | DLGC_WANTCHARS;
        case WM_KEYDOWN:
            if (wp == VK_TAB) {
                focused =
                    (focused + ((GetKeyState(VK_SHIFT) & 0x8000) ? int(items.size()) - 1 : 1)) %
                    int(items.size());
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (focused >= 0 && focused < int(items.size())) {
                auto &c = items[focused];
                if ((wp == VK_SPACE || wp == VK_RETURN) && c.style == Style::Select) {
                    menu(focused);
                    return 0;
                }
                if ((wp == VK_UP || wp == VK_RIGHT || wp == VK_DOWN || wp == VK_LEFT ||
                     wp == VK_HOME) &&
                    c.style != Style::Select && begin(c.id)) {
                    set(focused, wp == VK_HOME ? definition(c.id)->initial
                                               : model.get(c.id) +
                                                     ((wp == VK_UP || wp == VK_RIGHT) ? 1 : -1));
                    end(c.id);
                    return 0;
                }
            }
            break;
        case WM_DESTROY:
            KillTimer(window, 1);
            return 0;
        }
        return DefWindowProcW(window, msg, wp, lp);
    }
    static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        Editor *self = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            self = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
            self->window = h;
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->message(m, w, l) : DefWindowProcW(h, m, w, l);
    }

  public:
    std::function<bool(int)> begin;
    std::function<void(int, int)> change;
    std::function<void(int)> end;
    std::function<void()> dirtyState;
    Editor(Model &m, Transport &t) : model(m), transport(t) {
        auto c = t.configuration();
        if (!c.first.empty())
            selected = c.first;
        layout();
    }
    ~Editor() {
        if (window)
            DestroyWindow(window);
        if (!className.empty())
            UnregisterClassW(className.c_str(), instance);
        if (token)
            Gdiplus::GdiplusShutdown(token);
    }
    bool create() {
        Gdiplus::GdiplusStartupInput startup;
        if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok)
            return false;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&proc), &instance);
        className = L"AbdicantCobaltEditor_" + std::to_wstring(reinterpret_cast<uintptr_t>(this));
        WNDCLASSW c{};
        c.lpfnWndProc = proc;
        c.hInstance = instance;
        c.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        c.lpszClassName = className.c_str();
        if (!RegisterClassW(&c))
            return false;
        window = CreateWindowExW(0, className.c_str(), L"Cobalt hardware editor", WS_POPUP, 0, 0,
                                 editorWidth, editorHeight, nullptr, nullptr, instance, this);
        if (!window)
            return false;
        SetTimer(window, 1, 40, nullptr);
        return true;
    }
    bool parent(HWND host) {
        if (!window || !IsWindow(host))
            return false;
        SetParent(window, host);
        SetWindowLongPtrW(window, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
        SetWindowPos(window, nullptr, 0, 0, editorWidth, editorHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return true;
    }
    bool show(bool visible) {
        if (!window)
            return false;
        ShowWindow(window, visible ? SW_SHOW : SW_HIDE);
        return true;
    }
    HWND handle() const {
        return window;
    }
    const std::vector<Item> &controls() const {
        return items;
    }
};
} // namespace cobalt
