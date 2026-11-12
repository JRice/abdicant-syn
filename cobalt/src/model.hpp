#pragma once
#include "tables.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
namespace cobalt {
struct Choice {
    int value;
    std::string text;
};
enum class Style { Horizontal, Vertical, Knob, Select };
struct Def {
    int id;
    std::string name, group;
    int initial = 0, maximum = 127;
    bool bipolar = false;
    Style style = Style::Horizontal;
    std::vector<Choice> choices;
};
inline const std::vector<Def> &definitions() {
    static const auto all = [] {
        std::vector<Def> d;
        auto add = [&](int id, const char *name, const char *group, int initial = 0,
                       int maximum = 127, bool bipolar = false,
                       Style style = Style::Horizontal) -> Def & {
            d.push_back({id, name, group, initial, maximum, bipolar, style, {}});
            return d.back();
        };
        auto enumeration = [&](int id, const char *name, const char *group, auto &names, int n,
                               int initial = 0, int step = 1) -> Def & {
            auto &p = add(id, name, group, initial, (n - 1) * step, false, Style::Select);
            for (int i = 0; i < n; ++i)
                p.choices.push_back({i * step, names[i]});
            return p;
        };
        for (int osc = 0; osc < 2; ++osc) {
            const char *g = osc ? "Oscillator 2" : "Oscillator 1";
            enumeration(62 + osc, "Algorithm", g, algorithms, 40);
            add(osc ? 30 : 14, "Tune", g, 64, 127, true);
            add(osc ? 31 : 15, "Fine", g, 64, 127, true);
            add(osc ? 17 : 16, "Parameter A", g, 0);
            add(osc ? 20 : 19, "Parameter B", g, 0);
        }
        enumeration(42, "Filter type", "Filter / Voice", filters, 31);
        add(34, "Cutoff", "Filter / Voice", 100, 127, false, Style::Knob);
        add(35, "Resonance", "Filter / Voice", 0, 127, false, Style::Knob);
        add(33, "Morph", "Filter / Voice", 0, 127, false, Style::Knob);
        add(18, "Osc mix", "Filter / Voice", 64, 127, false, Style::Knob);
        add(151, "Drift", "Filter / Voice", 0, 127, false, Style::Knob);
        add(152, "Voice width", "Filter / Voice", 64, 127, false, Style::Knob);
        add(5, "Glide", "Filter / Voice", 0, 127, false, Style::Knob);
        add(196, "Vintage", "Filter / Voice", 0, 127, false, Style::Knob);
        const char *onoff[] = {"Off", "On"};
        enumeration(195, "Free run", "Filter / Voice", onoff, 2);
        const char *scaling[] = {"Relative", "Bipolar", "Absolute"};
        enumeration(197, "Cutoff scaling", "Filter / Voice", scaling, 3);
        // This is a compound field: the upper bits select the voice family and the
        // lower bits carry the allocation/legato variant. Preserve all 112 values.
        add(9, "Voice mode", "Filter / Voice", 16, 111, false, Style::Knob);
        const char *eg[] = {"Expo",      "Snappy",      "Soft",      "Linear",
                            "Expo Long", "Snappy Long", "Soft Long", "Linear Long"};
        const int adsr[3][4] = {{26, 27, 28, 29}, {22, 23, 24, 25}, {43, 44, 45, 46}};
        const int depths[] = {51, 32, 49};
        const char *groups[] = {"Amp envelope", "Filter envelope", "Mod envelope"};
        const char *stages[] = {"Attack", "Decay", "Sustain", "Release"};
        for (int n = 0; n < 3; ++n) {
            enumeration(80 + n, "Type", groups[n], eg, 8);
            for (int j = 0; j < 4; ++j)
                add(adsr[n][j], stages[j], groups[n], j == 2 ? 127 : 0, 127, false,
                    Style::Vertical);
            add(depths[n], "Depth", groups[n], n == 0 ? 127 : 64, 127, n != 0, Style::Vertical);
        }
        for (int n = 0; n < 3; ++n) {
            std::string g = "LFO " + std::to_string(n + 1);
            const int rate[] = {36, 47, 3}, depth[] = {37, 48, 13}, shape[] = {39, 50, 21},
                      mode[] = {56, 57, 54};
            add(rate[n], "Rate", g.c_str(), 64, 127, false, Style::Knob);
            add(depth[n], "Depth", g.c_str(), 0);
            const char *shapes[] = {"Sine",     "Triangle",      "Square",    "Saw up",
                                    "Saw down", "Sample & hold", "Slewed S&H"};
            enumeration(shape[n], "Shape", g.c_str(), shapes, 7, 0, 18);
            const char *modes[] = {"Retrigger", "Free", "Single"};
            enumeration(mode[n], "Mode", g.c_str(), modes, 3, 42, 42);
        }
        for (int n = 0; n < 8; ++n) {
            std::string g = "Matrix / Slot " + std::to_string(n + 1);
            auto &s = add(100 + n, "Source", g.c_str(), 127, 127, false, Style::Select);
            for (int i = 0; i < 12; ++i)
                s.choices.push_back({i, sources[i]});
            s.choices.push_back({127, "None"});
            auto &t = add(108 + n, "Destination", g.c_str(), 127, 127, false, Style::Select);
            for (int i = 0; i < 55; ++i)
                t.choices.push_back({i, destinations[i]});
            t.choices.push_back({127, "None"});
            add(88 + n, "Amount", g.c_str(), 64, 127, true);
        }
        const char *fixed[] = {"Note > Cutoff", "Y+ > LFO1 depth", "Aftertouch > Cutoff",
                               "Velocity > Amp depth"};
        for (int i = 0; i < 4; ++i)
            add(163 + i, fixed[i], "Fixed modulation", i == 3 ? 0 : 64, 127, i != 3);
        const char *gains[] = {"Note",        "Velocity",    "Expression",  "Aftertouch",
                               "Joystick X+", "Joystick X-", "Joystick Y+", "Joystick Y-"};
        for (int i = 0; i < 8; ++i)
            add(155 + i, gains[i], "Source depths", 127);
        for (int n = 0; n < 3; ++n) {
            std::string g = "Effect " + std::to_string(n + 1);
            enumeration(128 + 7 * n, "Type", g.c_str(), effects, 27);
            for (int j = 0; j < 6; ++j) {
                std::string name = "Parameter " + std::string(1, char('A' + j));
                add(129 + 7 * n + j, name.c_str(), g.c_str(), 0, 127, false, Style::Knob);
            }
        }
        add(73, "FX level", "Output", 64);
        add(119, "Patch gain", "Output", 100);
        add(149, "Audio > FX", "Output", 0);
        add(83, "Audio input", "Output", 0);
        return d;
    }();
    return all;
}
inline const Def *definition(int id) {
    for (auto &d : definitions())
        if (d.id == id)
            return &d;
    return nullptr;
}
inline int valid(int id, double v) {
    auto *d = definition(id);
    if (!d || !std::isfinite(v))
        return 0;
    int result = int(std::clamp(std::round(v), 0., double(d->maximum)));
    if (!d->choices.empty()) {
        auto best = d->choices.front();
        for (auto &c : d->choices)
            if (std::abs(c.value - result) < std::abs(best.value - result))
                best = c;
        result = best.value;
    }
    return result;
}
inline std::string format(int id, int value) {
    auto *d = definition(id);
    if (!d)
        return "";
    for (auto &c : d->choices)
        if (c.value == value)
            return c.text;
    if (id == 9) {
        const char *voice[] = {"Mono",     "Poly",    "Unison 2", "Unison 4",
                               "Unison 8", "Stack 2", "Stack 4"};
        return std::string(voice[std::clamp(value / 16, 0, 6)]) + " / " +
               std::to_string(value & 15);
    }
    char t[80];
    if (d->bipolar)
        std::snprintf(t, sizeof t, "%+d", value - 64);
    else
        std::snprintf(t, sizeof t, "%d", value);
    return t;
}
struct Model {
    std::array<std::atomic<int>, 256> values{};
    std::array<std::atomic<bool>, 256> dirty{};
    std::atomic<uint32_t> recall{0};
    Model() {
        for (auto &d : definitions())
            values[d.id].store(d.initial);
    }
    int get(int id) const {
        return values[id].load(std::memory_order_relaxed);
    }
    void set(int id, double v) {
        if (!definition(id) || !std::isfinite(v))
            return;
        int x = valid(id, v);
        if (values[id].exchange(x) != x)
            dirty[id].store(true, std::memory_order_release);
    }
    void sendAll() {
        recall.fetch_add(1, std::memory_order_release);
    }
};
struct Transaction {
    std::array<uint32_t, 4> messages{};
    int size = 0;
};
inline Transaction encode(int id, int value, int channel) {
    Transaction t;
    auto push = [&](int cc, int v) {
        t.messages[t.size++] =
            uint32_t(0xb0 | (channel & 15)) | (uint32_t(cc) << 8) | (uint32_t(v & 127) << 16);
    };
    if (id < 128) {
        push(id, value);
        return t;
    }
    int group = -1, section = 0, field = 0;
    if (id >= 128 && id <= 148) {
        group = 34;
        section = (id - 128) / 7 + 1;
        field = (id - 128) % 7;
    }
    if (id == 149) {
        group = 34;
        section = 0;
    }
    if (id == 151) {
        group = 36;
        section = 0;
    }
    if (id == 152) {
        group = 36;
        section = 2;
    }
    if (id >= 155 && id <= 162) {
        group = 32;
        section = 12;
        field = id - 155;
    }
    if (id >= 163 && id <= 166) {
        group = 32;
        section = 13;
        field = id - 163;
    }
    if (id >= 195 && id <= 197) {
        group = 36;
        section = id - 186;
    }
    if (group >= 0) {
        push(99, group);
        push(98, section);
        push(38, value);
        push(6, field);
    }
    return t;
}
inline int rank(int id) {
    if (id == 128 || id == 135 || id == 142 || id == 62 || id == 63 || id == 42)
        return 0;
    if (id >= 100 && id <= 115)
        return 1;
    if (id >= 88 && id <= 95)
        return 3;
    return 2;
}
} // namespace cobalt
