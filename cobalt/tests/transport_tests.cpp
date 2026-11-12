#include "../src/transport.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace cobalt;
namespace {
std::mutex logMutex;
std::vector<uint32_t> logMessages;
std::atomic<int> opened{0}, closed{0};
UINT WINAPI devices() {
    return 1;
}
MMRESULT WINAPI caps(UINT_PTR, LPMIDIOUTCAPSW c, UINT size) {
    if (size < sizeof(*c))
        return MMSYSERR_INVALPARAM;
    *c = {};
    std::wcscpy(c->szPname, L"Offline Test Cobalt");
    return MMSYSERR_NOERROR;
}
MMRESULT WINAPI openPort(LPHMIDIOUT out, UINT id, DWORD_PTR, DWORD_PTR, DWORD) {
    if (id)
        return MMSYSERR_BADDEVICEID;
    *out = reinterpret_cast<HMIDIOUT>(uintptr_t(0x1234));
    ++opened;
    return MMSYSERR_NOERROR;
}
MMRESULT WINAPI closePort(HMIDIOUT) {
    ++closed;
    return MMSYSERR_NOERROR;
}
MMRESULT WINAPI shortMessage(HMIDIOUT handle, DWORD msg) {
    if (handle != reinterpret_cast<HMIDIOUT>(uintptr_t(0x1234)))
        return MMSYSERR_INVALHANDLE;
    std::lock_guard lock(logMutex);
    logMessages.push_back(msg);
    return MMSYSERR_NOERROR;
}
void check(bool value, const char *why) {
    if (!value)
        throw std::runtime_error(why);
}
void guardImports() {
    auto *base = reinterpret_cast<unsigned char *>(GetModuleHandleW(nullptr));
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    auto *imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(
        base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    int count = 0;
    for (; imports->Name; ++imports) {
        if (_stricmp(reinterpret_cast<char *>(base + imports->Name), "WINMM.dll"))
            continue;
        auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->OriginalFirstThunk);
        auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            auto *e = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData);
            auto *n = reinterpret_cast<char *>(e->Name);
            void *replacement = nullptr;
            if (!std::strcmp(n, "midiOutGetNumDevs"))
                replacement = reinterpret_cast<void *>(devices);
            if (!std::strcmp(n, "midiOutGetDevCapsW"))
                replacement = reinterpret_cast<void *>(caps);
            if (!std::strcmp(n, "midiOutOpen"))
                replacement = reinterpret_cast<void *>(openPort);
            if (!std::strcmp(n, "midiOutClose"))
                replacement = reinterpret_cast<void *>(closePort);
            if (!std::strcmp(n, "midiOutShortMsg"))
                replacement = reinterpret_cast<void *>(shortMessage);
            if (replacement) {
                DWORD old, unused;
                check(VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old),
                      "IAT protect");
                slots->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
                VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &unused);
                ++count;
            }
        }
    }
    check(count == 5, "All five MIDI imports replaced before any transport starts");
}
template <class F> void wait(F f, const char *why, int ms = 2500) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!f()) {
        check(std::chrono::steady_clock::now() < end, why);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
size_t count() {
    std::lock_guard lock(logMutex);
    return logMessages.size();
}
} // namespace
int main() {
    try {
        guardImports();
        Model model;
        {
            Transport t(model);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            check(opened == 0 && count() == 0, "Disconnected startup cannot write");
            t.configure(L"Offline Test Cobalt", true);
            wait([&] { return t.connected.load(); }, "Connect mock output");
            wait([&] { return count() > 180; }, "Paced initial state replay");
            // Wait for the initial snapshot to drain, then test automation while processing is
            // stopped.
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
            size_t start = count();
            model.set(34, 17);
            wait(
                [&] {
                    std::lock_guard lock(logMutex);
                    return std::find(logMessages.begin() + std::min(start, logMessages.size()),
                                     logMessages.end(), 0x1122b0u) != logMessages.end();
                },
                "Parameter send without process or GUI");
            model.set(129, 72);
            for (int i = 0; i < 50; ++i) {
                t.note(0x643c90);
                t.note(0x003c80);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            {
                std::lock_guard lock(logMutex);
                for (size_t i = 0; i < logMessages.size(); ++i)
                    if (((logMessages[i] >> 8) & 127) == 99) {
                        check(i + 3 < logMessages.size(), "Complete extended packet");
                        check(((logMessages[i + 1] >> 8) & 127) == 98 &&
                                  ((logMessages[i + 2] >> 8) & 127) == 38 &&
                                  ((logMessages[i + 3] >> 8) & 127) == 6,
                              "No note interleaving inside an extended transaction");
                    }
            }
            Model another;
            Transport second(another);
            second.configure(L"Offline Test Cobalt", true);
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            check(!second.connected && opened == 1, "Single instance owns output");
            t.configure(L"Offline Test Cobalt", false);
            wait([&] { return !t.connected.load(); }, "Disconnect");
            wait([&] { return second.connected.load(); }, "Waiting instance reconnects", 1800);
            second.configure(L"Offline Test Cobalt", false);
            wait([&] { return !second.connected.load(); }, "Second disconnect");
        }
        check(opened == closed, "Every mock handle closed");
        std::cout << "PASS: mock-only MIDI, pacing/recall, stopped automation, note/transaction "
                     "ordering, output ownership and reconnect; no physical MIDI access\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
