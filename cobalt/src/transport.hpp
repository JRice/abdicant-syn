#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "model.hpp"
#include <chrono>
#include <windows.h>
#include <mmsystem.h>
#include <mutex>
#include <thread>
namespace cobalt {
inline std::vector<std::wstring> outputNames() {
    std::vector<std::wstring> result;
    for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
        MIDIOUTCAPSW c{};
        if (midiOutGetDevCapsW(i, &c, sizeof c) == MMSYSERR_NOERROR)
            result.emplace_back(c.szPname);
    }
    return result;
}
class Transport {
    Model &model;
    std::mutex configMutex, statusMutex;
    std::wstring wanted;
    bool enabled = false;
    uint32_t revision = 0;
    std::string statusText = "Disconnected - choose USB output, then Connect + Send All";
    std::atomic<bool> running{true}, panicWanted{false};
    std::atomic<uint32_t> write{0}, read{0};
    std::array<uint32_t, 2048> notes{};
    std::thread worker;
    HMIDIOUT port = nullptr;
    HANDLE ownership = nullptr;
    uint16_t touchedChannels = 0;
    void status(const std::string &s) {
        std::lock_guard lock(statusMutex);
        statusText = s;
    }
    bool send(uint32_t message) {
        if (!port)
            return false;
        auto error = midiOutShortMsg(port, message);
        if (error != MMSYSERR_NOERROR) {
            status("USB write failed - reconnect the synth");
            return false;
        }
        return true;
    }
    void panicNow() {
        if (!port)
            return;
        for (int c = 0; c < 16; ++c)
            if (touchedChannels & (1 << c)) {
                send(0xb0u | c | (64u << 8));
                send(0xb0u | c | (123u << 8));
                send(0xb0u | c | (120u << 8));
            }
        touchedChannels = 0;
    }
    void close() {
        connected = false;
        if (port) {
            panicNow();
            midiOutClose(port);
            port = nullptr;
        }
        if (ownership) {
            ReleaseMutex(ownership);
            CloseHandle(ownership);
            ownership = nullptr;
        }
    }
    bool open(const std::wstring &name) {
        UINT index = UINT(-1);
        for (UINT i = 0; i < midiOutGetNumDevs(); ++i) {
            MIDIOUTCAPSW c{};
            if (!midiOutGetDevCapsW(i, &c, sizeof c) && name == c.szPname) {
                index = i;
                break;
            }
        }
        if (index == UINT(-1)) {
            status("USB output unavailable - waiting for the selected device");
            return false;
        }
        // Cross-process ownership: two plugin instances must not alternate patch writes.
        uint64_t hash = 1469598103934665603ull;
        for (auto c : name) {
            hash ^= uint16_t(c);
            hash *= 1099511628211ull;
        }
        auto mutexName = L"Local\\AbdicantCobaltOutput_" + std::to_wstring(hash);
        ownership = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        if (!ownership) {
            status("Cannot reserve the MIDI output");
            return false;
        }
        DWORD result = WaitForSingleObject(ownership, 0);
        if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
            CloseHandle(ownership);
            ownership = nullptr;
            status("USB output owned by another Cobalt editor instance");
            return false;
        }
        if (midiOutOpen(&port, index, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            ReleaseMutex(ownership);
            CloseHandle(ownership);
            ownership = nullptr;
            port = nullptr;
            status("USB output busy - release it in Bitwig/MODALapp, then reconnect");
            return false;
        }
        connected = true;
        status("Connected - USB output only; keyboard input remains with Bitwig");
        return true;
    }
    void loop() {
        uint32_t seenRevision = UINT32_MAX, seenRecall = 0;
        bool shouldConnect = false;
        std::wstring name;
        auto nextRetry = std::chrono::steady_clock::now();
        std::vector<int> order;
        for (int rankValue = 0; rankValue < 4; ++rankValue)
            for (auto &d : definitions())
                if (rank(d.id) == rankValue)
                    order.push_back(d.id);
        while (running.load()) {
            {
                std::lock_guard lock(configMutex);
                if (seenRevision != revision) {
                    seenRevision = revision;
                    name = wanted;
                    shouldConnect = enabled;
                    close();
                    read.store(write.load());
                    nextRetry = std::chrono::steady_clock::now();
                }
            }
            if (shouldConnect && !port && std::chrono::steady_clock::now() >= nextRetry) {
                if (open(name))
                    model.sendAll();
                nextRetry = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
            if (panicWanted.exchange(false)) {
                // A dropped note-off must not be followed by queued note-ons
                // after the panic. Discard the old batch before silencing it.
                read.store(write.load(std::memory_order_acquire), std::memory_order_release);
                panicNow();
            }
            if (!port) {
                read.store(write.load());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            auto drainNotes = [&]() {
                uint32_t r = read.load(std::memory_order_relaxed),
                         end = write.load(std::memory_order_acquire);
                while (r != end) {
                    uint32_t msg = notes[r % notes.size()];
                    touchedChannels |= uint16_t(1 << (msg & 15));
                    if (!send(msg)) {
                        close();
                        break;
                    }
                    ++r;
                    read.store(r, std::memory_order_release);
                }
            };
            drainNotes();
            if (!port)
                continue;
            uint32_t recall = model.recall.load(std::memory_order_acquire);
            if (recall != seenRecall) {
                seenRecall = recall;
                for (int i = 0; i < 8; ++i) {
                    auto t = encode(88 + i, 64, channel.load());
                    for (int n = 0; n < t.size; ++n)
                        send(t.messages[n]);
                }
                for (auto &d : definitions())
                    model.dirty[d.id].store(true, std::memory_order_release);
            }
            // One complete transaction per tick. Host automation coalesces to the newest value.
            for (int id : order)
                if (model.dirty[id].exchange(false, std::memory_order_acq_rel)) {
                    auto t = encode(id, model.get(id), channel.load());
                    bool ok = true;
                    for (int n = 0; n < t.size; ++n)
                        if (!send(t.messages[n])) {
                            ok = false;
                            break;
                        }
                    if (!ok) {
                        model.dirty[id] = true;
                        close();
                    }
                    break;
                }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        close();
    }

  public:
    std::atomic<bool> connected{false};
    std::atomic<int> channel{0};
    std::atomic<uint64_t> dropped{0};
    explicit Transport(Model &m) : model(m) {
        worker = std::thread([this] { loop(); });
    }
    ~Transport() {
        running = false;
        worker.join();
    }
    void configure(std::wstring name, bool on) {
        std::lock_guard lock(configMutex);
        wanted = std::move(name);
        enabled = on;
        ++revision;
        if (!on)
            status("Disconnected");
    }
    std::pair<std::wstring, bool> configuration() {
        std::lock_guard lock(configMutex);
        return {wanted, enabled};
    }
    std::string status() {
        std::lock_guard lock(statusMutex);
        return statusText;
    }
    void note(uint32_t message) {
        if (!connected.load(std::memory_order_relaxed))
            return;
        const auto w = write.load(std::memory_order_relaxed),
                   r = read.load(std::memory_order_acquire);
        if (w - r >= notes.size()) {
            dropped.fetch_add(1);
            panicWanted = true;
            return;
        }
        notes[w % notes.size()] = message;
        write.store(w + 1, std::memory_order_release);
    }
    void panic() {
        panicWanted = true;
    }
};
} // namespace cobalt
