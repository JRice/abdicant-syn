#define NOMINMAX
#include "../src/editor.hpp"
#include <clap/clap.h>
#include <cstring>
#include <gdiplus.h>
#include <iostream>
#include <objidl.h>
#include <set>
#include <stdexcept>
#include <windows.h>
void check(bool value, const char *why) {
    if (!value)
        throw std::runtime_error(why);
}
struct Events {
    clap_input_events_t in{};
    clap_output_events_t out{};
    std::vector<clap_event_param_value_t> values;
    std::vector<uint16_t> emitted;
    Events() {
        in.ctx = this;
        in.size = [](const clap_input_events_t *p) -> uint32_t {
            return uint32_t(static_cast<Events *>(p->ctx)->values.size());
        };
        in.get = [](const clap_input_events_t *p, uint32_t i) -> const clap_event_header_t * {
            return &static_cast<Events *>(p->ctx)->values.at(i).header;
        };
        out.ctx = this;
        out.try_push = [](const clap_output_events_t *p, const clap_event_header_t *e) {
            static_cast<Events *>(p->ctx)->emitted.push_back(e->type);
            return true;
        };
    }
    void add(int id, double value) {
        clap_event_param_value_t v{};
        v.header = {sizeof v, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
        v.param_id = 1000 + id;
        v.note_id = v.port_index = v.channel = v.key = -1;
        v.value = value;
        values.push_back(v);
    }
};
struct Memory {
    std::vector<char> bytes;
    size_t position = 0;
    clap_ostream_t out{};
    clap_istream_t in{};
    Memory() {
        out.ctx = this;
        out.write = [](const clap_ostream_t *p, const void *b, uint64_t n) -> int64_t {
            auto &m = *static_cast<Memory *>(p->ctx);
            size_t count = std::min<size_t>(size_t(n), 5);
            m.bytes.insert(m.bytes.end(), static_cast<const char *>(b),
                           static_cast<const char *>(b) + count);
            return int64_t(count);
        };
        in.ctx = this;
        in.read = [](const clap_istream_t *p, void *b, uint64_t n) -> int64_t {
            auto &m = *static_cast<Memory *>(p->ctx);
            size_t count = std::min({size_t(n), size_t(7), m.bytes.size() - m.position});
            std::memcpy(b, m.bytes.data() + m.position, count);
            m.position += count;
            return int64_t(count);
        };
    }
};
int main() {
    try {
        std::set<int> ids;
        for (auto &d : cobalt::definitions()) {
            check(ids.insert(d.id).second, "Unique parameter IDs");
            check(cobalt::valid(d.id, d.initial) == d.initial, "Valid defaults");
            check(cobalt::encode(d.id, d.initial, 0).size > 0, "Every control has an encoder");
        }
        {
            cobalt::Model m;
            cobalt::Transport t(m);
            cobalt::Editor e(m, t);
            std::set<int> visible;
            for (auto &c : e.controls()) {
                check(c.bounds.x >= 0 && c.bounds.y >= 0 && c.bounds.x + c.bounds.w <= 1440 &&
                          c.bounds.y + c.bounds.h <= 1080,
                      "Controls remain in fixed window");
                visible.insert(c.id);
            }
            check(visible == ids, "Every saved parameter has an on-screen control");
        }
        check(cobalt::encode(128, 25, 0).messages ==
                  std::array<uint32_t, 4>{0x2263b0, 0x0162b0, 0x1926b0, 0x0006b0},
              "Independent FX reference transaction");
        check(cobalt::encode(166, 64, 15).messages ==
                  std::array<uint32_t, 4>{0x2063bf, 0x0d62bf, 0x4026bf, 0x0306bf},
              "Fixed modulation reference");
        auto dll = LoadLibraryW(L"cobalt/build/CLAP/CobaltEditor.clap");
        check(dll, "Load CLAP");
        auto *entry =
            reinterpret_cast<const clap_plugin_entry_t *>(GetProcAddress(dll, "clap_entry"));
        check(entry && entry->init(""), "Entry");
        auto *factory =
            static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        check(factory && factory->get_plugin_count(factory) == 1, "Factory");
        clap_host_t host{CLAP_VERSION_INIT,
                         nullptr,
                         "Cobalt offline test host",
                         "Abdicant",
                         "",
                         "0.1",
                         [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                         [](const clap_host_t *) {},
                         [](const clap_host_t *) {},
                         [](const clap_host_t *) {}};
        auto *plugin =
            factory->create_plugin(factory, &host, factory->get_plugin_descriptor(factory, 0)->id);
        check(plugin && plugin->init(plugin), "Create");
        auto *params = static_cast<const clap_plugin_params_t *>(
            plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        auto *state =
            static_cast<const clap_plugin_state_t *>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        auto *gui =
            static_cast<const clap_plugin_gui_t *>(plugin->get_extension(plugin, CLAP_EXT_GUI));
        check(params && state && gui, "Extensions");
        check(params->count(plugin) == ids.size(), "All parameters exposed");
        Events events;
        for (uint32_t i = 0; i < params->count(plugin); ++i) {
            clap_param_info_t info{};
            check(params->get_info(plugin, i, &info), "Parameter info");
            events.add(int(info.id - 1000), info.max_value);
        }
        params->flush(plugin, &events.in, &events.out);
        Memory saved;
        check(state->save(plugin, &saved.out), "Save with short writes");
        events.values.clear();
        for (auto &d : cobalt::definitions())
            events.add(d.id, d.initial);
        params->flush(plugin, &events.in, &events.out);
        check(state->load(plugin, &saved.in), "Load with short reads");
        for (auto &d : cobalt::definitions()) {
            double v = -1;
            check(params->get_value(plugin, 1000 + d.id, &v) && v == cobalt::valid(d.id, d.maximum),
                  "Complete patch recall");
        }
        Memory invalid;
        invalid.bytes = saved.bytes;
        invalid.bytes[0] = 'X';
        check(!state->load(plugin, &invalid.in), "Invalid state rejected");
        Memory truncated;
        truncated.bytes.assign(saved.bytes.begin(), saved.bytes.begin() + 30);
        check(!state->load(plugin, &truncated.in), "Truncated state rejected");
        double before;
        params->get_value(plugin, 1034, &before);
        events.values.clear();
        events.add(34, std::nan(""));
        params->flush(plugin, &events.in, &events.out);
        double after;
        params->get_value(plugin, 1034, &after);
        check(before == after, "NaN rejected");
        check(!plugin->activate(plugin, 0, 1, 256) && plugin->activate(plugin, 48000, 1, 256),
              "Sample rate validation");
        plugin->start_processing(plugin);
        float left[256], right[256];
        std::fill_n(left, 256, 1.f);
        std::fill_n(right, 256, 1.f);
        float *channels[] = {left, right};
        clap_audio_buffer_t buffer{};
        buffer.channel_count = 2;
        buffer.data32 = channels;
        clap_process_t block{};
        block.frames_count = 256;
        block.audio_outputs_count = 1;
        block.audio_outputs = &buffer;
        events.values.clear();
        block.in_events = &events.in;
        block.out_events = &events.out;
        check(plugin->process(plugin, &block) == CLAP_PROCESS_CONTINUE && left[0] == 0 &&
                  right[255] == 0,
              "Silence without hardware audio");
        float inputL[256], inputR[256];
        for (int i = 0; i < 256; ++i) {
            inputL[i] = float(i) / 256;
            inputR[i] = -inputL[i];
        }
        float *inputs[] = {inputL, inputR};
        clap_audio_buffer_t input{};
        input.channel_count = 2;
        input.data32 = inputs;
        block.audio_inputs_count = 1;
        block.audio_inputs = &input;
        plugin->process(plugin, &block);
        check(left[123] == inputL[123] && right[255] == inputR[255],
              "Optional return passes through");
        HWND parent =
            CreateWindowExW(0, L"STATIC", L"Offline Cobalt host", WS_OVERLAPPEDWINDOW, 0, 0, 1460,
                            1120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        check(parent, "Parent");
        // Restore useful defaults for the actual native editor capture.
        events.values.clear();
        for (auto &d : cobalt::definitions())
            events.add(d.id, d.initial);
        events.add(32, 96);
        events.add(49, 100);
        events.add(128, 1);
        events.add(135, 8);
        events.add(142, 11);
        params->flush(plugin, &events.in, &events.out);
        for (int cycle = 0; cycle < 8; ++cycle) {
            check(gui->create(plugin, CLAP_WINDOW_API_WIN32, false), "Create editor");
            clap_window_t w{};
            w.api = CLAP_WINDOW_API_WIN32;
            w.win32 = parent;
            check(gui->set_parent(plugin, &w), "Attach editor");
            uint32_t width, height;
            gui->get_size(plugin, &width, &height);
            check(width == 1440 && height == 1080 && !gui->can_resize(plugin), "Fixed editor size");
            HWND child = GetWindow(parent, GW_CHILD);
            check(child, "Child");
            if (cycle == 0) {
                Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
                Gdiplus::Graphics graphics(&bitmap);
                HDC dc = graphics.GetHDC();
                SendMessageW(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
                graphics.ReleaseHDC(dc);
                CLSID png{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0, 0, 0xf8, 0x1e, 0xf3, 0x2e}};
                check(bitmap.Save(L"cobalt/build/native-editor.png", &png, nullptr) == Gdiplus::Ok,
                      "Native screenshot");
            }
            SendMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(1005, 170));
            SendMessageW(child, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(1005, 150));
            SendMessageW(child, WM_LBUTTONUP, 0, MAKELPARAM(1005, 150));
            params->flush(plugin, nullptr, &events.out);
            gui->destroy(plugin);
            check(!GetWindow(parent, GW_CHILD), "Destroy child");
        }
        check(std::find(events.emitted.begin(), events.emitted.end(),
                        CLAP_EVENT_PARAM_GESTURE_BEGIN) != events.emitted.end() &&
                  std::find(events.emitted.begin(), events.emitted.end(),
                            CLAP_EVENT_PARAM_GESTURE_END) != events.emitted.end(),
              "GUI automation gestures");
        DestroyWindow(parent);
        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        entry->deinit();
        FreeLibrary(dll);
        std::cout << "PASS: " << ids.size()
                  << " parameters, encoders, automation/state, malformed streams, audio return, "
                     "native UI and eight reopen cycles; no hardware connection\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
