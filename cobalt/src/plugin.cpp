#include "editor.hpp"
#include <clap/clap.h>
#include <cstring>
#include <new>
namespace {
using namespace cobalt;
constexpr clap_id firstId = 1000;
const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_UTILITY,
                          CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t descriptor = {CLAP_VERSION_INIT,
                                             "local.abdicant.cobalt.hardware-editor",
                                             "Cobalt Editor",
                                             "Abdicant",
                                             "",
                                             "",
                                             "",
                                             "0.1.0",
                                             "COBALT8 USB controller - external hardware required",
                                             features};
struct Plugin {
    clap_plugin_t api{};
    const clap_host_t *host;
    Model model;
    Transport transport{model};
    std::unique_ptr<Editor> editor;
    struct UiEvent {
        int id;
        uint16_t type;
        int value;
        uint32_t epoch;
    };
    std::array<UiEvent, 4096> queue{};
    std::atomic<uint32_t> write{0}, read{0}, epoch{0};
    int gesture = -1;
    explicit Plugin(const clap_host_t *h) : host(h) {}
    ~Plugin() {
        editor.reset();
    }
    bool enqueue(int id, uint16_t type, int value, uint32_t reserve) {
        auto w = write.load(std::memory_order_relaxed), r = read.load(std::memory_order_acquire);
        if (w - r >= queue.size() - reserve)
            return false;
        queue[w % queue.size()] = {id, type, value, epoch.load()};
        write.store(w + 1, std::memory_order_release);
        return true;
    }
    void requestFlush() {
        auto *h =
            static_cast<const clap_host_params_t *>(host->get_extension(host, CLAP_EXT_PARAMS));
        if (h && h->request_flush)
            h->request_flush(host);
        else if (host->request_process)
            host->request_process(host);
    }
    bool begin(int id) {
        if (gesture >= 0 || !enqueue(id, CLAP_EVENT_PARAM_GESTURE_BEGIN, 0, 2))
            return false;
        gesture = id;
        requestFlush();
        return true;
    }
    void edit(int id, int value) {
        if (gesture != id)
            return;
        value = valid(id, value);
        if (enqueue(id, CLAP_EVENT_PARAM_VALUE, value, 1)) {
            model.set(id, value);
            requestFlush();
        }
    }
    void end(int id) {
        if (gesture != id)
            return;
        enqueue(id, CLAP_EVENT_PARAM_GESTURE_END, 0, 0);
        gesture = -1;
        requestFlush();
    }
    void dirtyState() {
        auto *s = static_cast<const clap_host_state_t *>(host->get_extension(host, CLAP_EXT_STATE));
        if (s && s->mark_dirty)
            s->mark_dirty(host);
    }
    void drain(const clap_output_events_t *out) {
        if (!out)
            return;
        auto r = read.load(std::memory_order_relaxed), last = write.load(std::memory_order_acquire);
        while (r != last) {
            auto &e = queue[r % queue.size()];
            bool accepted = true;
            if (e.epoch == epoch.load()) {
                if (e.type == CLAP_EVENT_PARAM_VALUE) {
                    clap_event_param_value_t v{};
                    v.header = {sizeof v, 0, CLAP_CORE_EVENT_SPACE_ID, e.type, CLAP_EVENT_IS_LIVE};
                    v.param_id = firstId + e.id;
                    v.note_id = v.port_index = v.channel = v.key = -1;
                    v.value = e.value;
                    accepted = out->try_push(out, &v.header);
                } else {
                    clap_event_param_gesture_t g{};
                    g.header = {sizeof g, 0, CLAP_CORE_EVENT_SPACE_ID, e.type, CLAP_EVENT_IS_LIVE};
                    g.param_id = firstId + e.id;
                    accepted = out->try_push(out, &g.header);
                }
            }
            if (!accepted) {
                break;
            }
            read.store(++r, std::memory_order_release);
        }
    }
    void event(const clap_event_header_t *e) {
        if (!e || e->space_id != CLAP_CORE_EVENT_SPACE_ID)
            return;
        if (e->type == CLAP_EVENT_PARAM_VALUE && e->size >= sizeof(clap_event_param_value_t)) {
            auto *p = reinterpret_cast<const clap_event_param_value_t *>(e);
            if (p->param_id >= firstId && p->param_id < firstId + 256 && p->note_id == -1 &&
                p->key == -1 && p->channel == -1)
                model.set(int(p->param_id - firstId), p->value);
        } else if ((e->type == CLAP_EVENT_NOTE_ON || e->type == CLAP_EVENT_NOTE_OFF ||
                    e->type == CLAP_EVENT_NOTE_CHOKE) &&
                   e->size >= sizeof(clap_event_note_t)) {
            auto *n = reinterpret_cast<const clap_event_note_t *>(e);
            if (n->port_index > 0)
                return;
            if (e->type == CLAP_EVENT_NOTE_CHOKE || n->key < 0) {
                transport.panic();
                return;
            }
            if (n->key > 127 || !std::isfinite(n->velocity))
                return;
            int v = int(std::clamp(n->velocity, 0., 1.) * 127 + .5);
            transport.note(uint32_t((e->type == CLAP_EVENT_NOTE_ON && v ? 0x90 : 0x80) |
                                    transport.channel.load()) |
                           (uint32_t(n->key) << 8) | (uint32_t(v) << 16));
        } else if (e->type == CLAP_EVENT_NOTE_EXPRESSION &&
                   e->size >= sizeof(clap_event_note_expression_t)) {
            auto *n = reinterpret_cast<const clap_event_note_expression_t *>(e);
            if (n->expression_id == CLAP_NOTE_EXPRESSION_PRESSURE && std::isfinite(n->value)) {
                auto v = uint32_t(std::clamp(n->value, 0., 1.) * 127 + .5);
                if (n->key >= 0 && n->key < 128)
                    transport.note(0xa0u | transport.channel.load() | (uint32_t(n->key) << 8) |
                                   (v << 16));
                else
                    transport.note(0xd0u | transport.channel.load() | (v << 8));
            }
        } else if (e->type == CLAP_EVENT_MIDI && e->size >= sizeof(clap_event_midi_t)) {
            auto *m = reinterpret_cast<const clap_event_midi_t *>(e);
            if (m->port_index)
                return;
            int command = m->data[0] & 0xf0, a = m->data[1] & 127, b = m->data[2] & 127;
            bool forward = command == 0x80 || command == 0x90 || command == 0xa0 ||
                           command == 0xd0 || command == 0xe0;
            if (command == 0xb0)
                forward = a == 1 || a == 11 || a == 64 || a == 66 || a == 67 || a == 74 ||
                          a == 120 || a == 121 || a == 123;
            if (forward)
                transport.note(uint32_t(command | transport.channel.load()) | (uint32_t(a) << 8) |
                               (uint32_t(b) << 16));
        }
    }
};
Plugin &self(const clap_plugin_t *p) {
    return *static_cast<Plugin *>(p->plugin_data);
}
bool CLAP_ABI init(const clap_plugin_t *) {
    return true;
}
void CLAP_ABI destroy(const clap_plugin_t *p) {
    delete &self(p);
}
bool CLAP_ABI activate(const clap_plugin_t *, double rate, uint32_t min, uint32_t max) {
    return std::isfinite(rate) && rate >= 8000 && rate <= 384000 && min > 0 && max >= min;
}
void CLAP_ABI noop(const clap_plugin_t *) {}
bool CLAP_ABI start(const clap_plugin_t *) {
    return true;
}
void CLAP_ABI stop(const clap_plugin_t *p) {
    self(p).transport.panic();
}
clap_process_status CLAP_ABI process(const clap_plugin_t *p, const clap_process_t *block) {
    auto &s = self(p);
    s.drain(block->out_events);
    if (block->in_events)
        for (uint32_t i = 0; i < block->in_events->size(block->in_events); ++i)
            s.event(block->in_events->get(block->in_events, i));
    for (uint32_t bus = 0; bus < block->audio_outputs_count; ++bus) {
        auto &out = block->audio_outputs[bus];
        for (uint32_t c = 0; c < out.channel_count; ++c) {
            if (out.data32) {
                float *dst = out.data32[c];
                const float *src = bus < block->audio_inputs_count &&
                                           c < block->audio_inputs[bus].channel_count &&
                                           block->audio_inputs[bus].data32
                                       ? block->audio_inputs[bus].data32[c]
                                       : nullptr;
                if (src && src != dst)
                    std::memmove(dst, src, block->frames_count * sizeof(float));
                else if (!src)
                    std::fill_n(dst, block->frames_count, 0.f);
            } else if (out.data64) {
                double *dst = out.data64[c];
                const double *src = bus < block->audio_inputs_count &&
                                            c < block->audio_inputs[bus].channel_count &&
                                            block->audio_inputs[bus].data64
                                        ? block->audio_inputs[bus].data64[c]
                                        : nullptr;
                if (src && src != dst)
                    std::memmove(dst, src, block->frames_count * sizeof(double));
                else if (!src)
                    std::fill_n(dst, block->frames_count, 0.);
            }
        }
        out.constant_mask = 0;
    }
    return CLAP_PROCESS_CONTINUE;
}
uint32_t CLAP_ABI paramCount(const clap_plugin_t *) {
    return uint32_t(definitions().size());
}
bool CLAP_ABI paramInfo(const clap_plugin_t *, uint32_t i, clap_param_info_t *out) {
    if (i >= definitions().size())
        return false;
    auto &d = definitions()[i];
    *out = {};
    out->id = firstId + d.id;
    out->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED;
    if (!d.choices.empty())
        out->flags |= CLAP_PARAM_IS_ENUM;
    std::snprintf(out->name, sizeof out->name, "%s: %s", d.group.c_str(), d.name.c_str());
    std::snprintf(out->module, sizeof out->module, "%s", d.group.c_str());
    out->min_value = 0;
    out->max_value = d.maximum;
    out->default_value = d.initial;
    return true;
}
bool CLAP_ABI paramValue(const clap_plugin_t *p, clap_id id, double *value) {
    if (id < firstId || id >= firstId + 256 || !definition(int(id - firstId)))
        return false;
    *value = self(p).model.get(int(id - firstId));
    return true;
}
bool CLAP_ABI paramText(const clap_plugin_t *, clap_id id, double value, char *out, uint32_t size) {
    if (id < firstId || id >= firstId + 256 || !size || !definition(int(id - firstId)) ||
        !std::isfinite(value))
        return false;
    auto s = format(int(id - firstId), valid(int(id - firstId), value));
    std::snprintf(out, size, "%s", s.c_str());
    return true;
}
bool CLAP_ABI parseText(const clap_plugin_t *, clap_id id, const char *text, double *out) {
    if (id < firstId || id >= firstId + 256 || !definition(int(id - firstId)))
        return false;
    auto *d = definition(int(id - firstId));
    for (auto &c : d->choices)
        if (c.text == text) {
            *out = c.value;
            return true;
        }
    char *end = nullptr;
    double v = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(v))
        return false;
    if (d->bipolar)
        v += 64;
    if (v < 0 || v > d->maximum)
        return false;
    *out = valid(d->id, v);
    return true;
}
void CLAP_ABI flush(const clap_plugin_t *p, const clap_input_events_t *in,
                    const clap_output_events_t *out) {
    self(p).drain(out);
    if (in)
        for (uint32_t i = 0; i < in->size(in); ++i)
            self(p).event(in->get(in, i));
}
const clap_plugin_params_t params = {paramCount, paramInfo, paramValue,
                                     paramText,  parseText, flush};
uint32_t CLAP_ABI audioCount(const clap_plugin_t *, bool) {
    return 1;
}
bool CLAP_ABI audioInfo(const clap_plugin_t *, uint32_t index, bool input,
                        clap_audio_port_info_t *out) {
    if (index)
        return false;
    *out = {};
    out->id = input ? 0 : 1;
    std::strcpy(out->name, input ? "Optional hardware audio return" : "Audio return / silence");
    out->flags = CLAP_AUDIO_PORT_IS_MAIN | CLAP_AUDIO_PORT_SUPPORTS_64BITS;
    out->channel_count = 2;
    out->port_type = CLAP_PORT_STEREO;
    out->in_place_pair = input ? 1 : 0;
    return true;
}
const clap_plugin_audio_ports_t audioPorts = {audioCount, audioInfo};
uint32_t CLAP_ABI noteCount(const clap_plugin_t *, bool input) {
    return input ? 1 : 0;
}
bool CLAP_ABI noteInfo(const clap_plugin_t *, uint32_t index, bool input,
                       clap_note_port_info_t *out) {
    if (index || !input)
        return false;
    *out = {};
    out->id = 0;
    out->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    out->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;
    std::strcpy(out->name, "Notes / expression to hardware USB output");
    return true;
}
const clap_plugin_note_ports_t notePorts = {noteCount, noteInfo};
bool writeAll(const clap_ostream_t *s, const void *data, uint64_t size) {
    auto *b = static_cast<const char *>(data);
    while (size) {
        auto n = s->write(s, b, size);
        if (n <= 0 || uint64_t(n) > size)
            return false;
        b += n;
        size -= n;
    }
    return true;
}
bool readAll(const clap_istream_t *s, void *data, uint64_t size) {
    auto *b = static_cast<char *>(data);
    while (size) {
        auto n = s->read(s, b, size);
        if (n <= 0 || uint64_t(n) > size)
            return false;
        b += n;
        size -= n;
    }
    return true;
}
bool CLAP_ABI save(const clap_plugin_t *p, const clap_ostream_t *out) {
    auto &s = self(p);
    uint32_t count = uint32_t(definitions().size());
    auto config = s.transport.configuration();
    int bytes = WideCharToMultiByte(CP_UTF8, 0, config.first.c_str(), int(config.first.size()),
                                    nullptr, 0, nullptr, nullptr);
    std::string port(size_t(bytes), 0);
    if (bytes)
        WideCharToMultiByte(CP_UTF8, 0, config.first.c_str(), int(config.first.size()), port.data(),
                            bytes, nullptr, nullptr);
    if (!writeAll(out, "C8ED0001", 8) || !writeAll(out, &count, 4)) {
        return false;
    }
    for (auto &d : definitions()) {
        uint32_t pair[2] = {uint32_t(d.id), uint32_t(s.model.get(d.id))};
        if (!writeAll(out, pair, 8))
            return false;
    }
    uint32_t c[3] = {uint32_t(s.transport.channel.load()), config.second ? 1u : 0u,
                     uint32_t(port.size())};
    return writeAll(out, c, sizeof c) && writeAll(out, port.data(), port.size());
}
bool CLAP_ABI load(const clap_plugin_t *p, const clap_istream_t *in) {
    char magic[8];
    uint32_t count;
    if (!readAll(in, magic, 8) || std::memcmp(magic, "C8ED0001", 8) || !readAll(in, &count, 4) ||
        count > 512)
        return false;
    std::array<int, 256> values{};
    std::array<bool, 256> seen{};
    for (auto &d : definitions())
        values[d.id] = d.initial;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pair[2];
        if (!readAll(in, pair, 8) || pair[0] >= 256 || seen[pair[0]])
            return false;
        seen[pair[0]] = true;
        auto *d = definition(int(pair[0]));
        if (d) {
            if (pair[1] > uint32_t(d->maximum) || valid(d->id, pair[1]) != int(pair[1]))
                return false;
            values[pair[0]] = int(pair[1]);
        }
    }
    uint32_t c[3];
    if (!readAll(in, c, sizeof c) || c[0] > 15 || c[1] > 1 || c[2] > 512)
        return false;
    std::string port(c[2], 0);
    if (!readAll(in, port.data(), port.size()) || port.find('\0') != std::string::npos)
        return false;
    int n = port.empty() ? 0
                         : MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, port.data(),
                                               int(port.size()), nullptr, 0);
    if (!port.empty() && n <= 0)
        return false;
    std::wstring name(size_t(n), 0);
    if (n)
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, port.data(), int(port.size()),
                            name.data(), n);
    if (c[1] && name.empty()) {
        return false;
    }
    auto &s = self(p);
    s.epoch.fetch_add(1);
    for (auto &d : definitions())
        s.model.set(d.id, values[d.id]);
    s.transport.panic();
    s.transport.channel = int(c[0]);
    s.transport.configure(name, c[1] != 0);
    s.model.sendAll();
    auto *h =
        static_cast<const clap_host_params_t *>(s.host->get_extension(s.host, CLAP_EXT_PARAMS));
    if (h && h->rescan)
        h->rescan(s.host, CLAP_PARAM_RESCAN_VALUES);
    return true;
}
const clap_plugin_state_t state = {save, load};
bool CLAP_ABI supported(const clap_plugin_t *, const char *api, bool floating) {
    return api && !floating && !std::strcmp(api, CLAP_WINDOW_API_WIN32);
}
bool CLAP_ABI preferred(const clap_plugin_t *, const char **api, bool *floating) {
    *api = CLAP_WINDOW_API_WIN32;
    *floating = false;
    return true;
}
bool CLAP_ABI guiCreate(const clap_plugin_t *p, const char *api, bool floating) {
    auto &s = self(p);
    if (s.editor || !supported(p, api, floating))
        return false;
    auto e = std::make_unique<Editor>(s.model, s.transport);
    e->begin = [&s](int id) { return s.begin(id); };
    e->change = [&s](int id, int v) { s.edit(id, v); };
    e->end = [&s](int id) { s.end(id); };
    e->dirtyState = [&s] { s.dirtyState(); };
    if (!e->create())
        return false;
    s.editor = std::move(e);
    return true;
}
void CLAP_ABI guiDestroy(const clap_plugin_t *p) {
    self(p).editor.reset();
}
bool CLAP_ABI guiSize(const clap_plugin_t *, uint32_t *w, uint32_t *h) {
    *w = editorWidth;
    *h = editorHeight;
    return true;
}
bool CLAP_ABI guiScale(const clap_plugin_t *, double) {
    return false;
}
bool CLAP_ABI noResize(const clap_plugin_t *) {
    return false;
}
bool CLAP_ABI noHints(const clap_plugin_t *, clap_gui_resize_hints_t *) {
    return false;
}
bool CLAP_ABI noAdjust(const clap_plugin_t *, uint32_t *, uint32_t *) {
    return false;
}
bool CLAP_ABI resize(const clap_plugin_t *, uint32_t w, uint32_t h) {
    return w == editorWidth && h == editorHeight;
}
bool CLAP_ABI parent(const clap_plugin_t *p, const clap_window_t *w) {
    return w && w->api && !std::strcmp(w->api, CLAP_WINDOW_API_WIN32) && self(p).editor &&
           self(p).editor->parent(static_cast<HWND>(w->win32));
}
bool CLAP_ABI noTransient(const clap_plugin_t *, const clap_window_t *) {
    return false;
}
void CLAP_ABI title(const clap_plugin_t *, const char *) {}
bool CLAP_ABI show(const clap_plugin_t *p) {
    return self(p).editor && self(p).editor->show(true);
}
bool CLAP_ABI hide(const clap_plugin_t *p) {
    return self(p).editor && self(p).editor->show(false);
}
const clap_plugin_gui_t gui = {supported, preferred,   guiCreate, guiDestroy, guiScale,
                               guiSize,   noResize,    noHints,   noAdjust,   resize,
                               parent,    noTransient, title,     show,       hide};
const void *CLAP_ABI extension(const clap_plugin_t *, const char *id) {
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &params;
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
        return &audioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &notePorts;
    if (!std::strcmp(id, CLAP_EXT_STATE))
        return &state;
    if (!std::strcmp(id, CLAP_EXT_GUI))
        return &gui;
    return nullptr;
}
uint32_t CLAP_ABI count(const clap_plugin_factory_t *) {
    return 1;
}
const clap_plugin_descriptor_t *CLAP_ABI describe(const clap_plugin_factory_t *, uint32_t i) {
    return i ? nullptr : &descriptor;
}
const clap_plugin_t *CLAP_ABI create(const clap_plugin_factory_t *, const clap_host_t *host,
                                     const char *id) {
    if (!host || !clap_version_is_compatible(host->clap_version) || std::strcmp(id, descriptor.id))
        return nullptr;
    auto *s = new (std::nothrow) Plugin(host);
    if (!s)
        return nullptr;
    s->api = {&descriptor, s,    init, destroy, activate,  stop,
              start,       stop, stop, process, extension, noop};
    return &s->api;
}
const clap_plugin_factory_t factory = {count, describe, create};
bool CLAP_ABI entryInit(const char *) {
    return true;
}
void CLAP_ABI entryDeinit() {}
const void *CLAP_ABI getFactory(const char *id) {
    return std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &factory;
}
} // namespace
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, entryInit,
                                                               entryDeinit, getFactory};
