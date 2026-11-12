// Offline VST3 parameter inventory. MIDI imports are blocked before InitDll.
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
UINT WINAPI noDevices() { return 0; }
MMRESULT WINAPI noInput(LPHMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD) { return MMSYSERR_NODRIVER; }
MMRESULT WINAPI noOutput(LPHMIDIOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD) { return MMSYSERR_NODRIVER; }
MMRESULT WINAPI noShort(HMIDIOUT, DWORD) { return MMSYSERR_NODRIVER; }
MMRESULT WINAPI noLong(HMIDIOUT, LPMIDIHDR, UINT) { return MMSYSERR_NODRIVER; }
void require(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
void isolateMidi(HMODULE module) {
    auto* base = reinterpret_cast<unsigned char*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    int blocked = 0;
    for (; imports->Name; ++imports) {
        if (_stricmp(reinterpret_cast<char*>(base + imports->Name), "WINMM.dll")) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imports->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto* entry = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            auto* name = reinterpret_cast<char*>(entry->Name);
            void* replacement = nullptr;
            if (!std::strcmp(name,"midiInGetNumDevs") || !std::strcmp(name,"midiOutGetNumDevs")) replacement = reinterpret_cast<void*>(noDevices);
            if (!std::strcmp(name,"midiInOpen")) replacement = reinterpret_cast<void*>(noInput);
            if (!std::strcmp(name,"midiOutOpen")) replacement = reinterpret_cast<void*>(noOutput);
            if (!std::strcmp(name,"midiOutShortMsg")) replacement = reinterpret_cast<void*>(noShort);
            if (!std::strcmp(name,"midiOutLongMsg")) replacement = reinterpret_cast<void*>(noLong);
            if (!replacement) continue;
            DWORD old;
            require(VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old), "Protect MIDI import");
            slots->u1.Function = reinterpret_cast<ULONG_PTR>(replacement);
            DWORD unused;
            VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &unused);
            ++blocked;
        }
    }
    require(blocked == 6, "Expected all six MIDI import guards");
    std::cerr << "Blocked MIDI enumeration, input/output opens, short messages and SysEx (6 imports).\n";
}
std::string utf8(const char16* text) {
    char out[1024]{};
    WideCharToMultiByte(CP_UTF8,0,reinterpret_cast<const wchar_t*>(text),-1,out,sizeof(out)-1,nullptr,nullptr);
    return out;
}
int main() {
    try {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        auto module = LoadLibraryW(L"C:\\Program Files\\Common Files\\VST3\\MODALapp\\MODALapp.vst3");
        require(module,"Load MODALapp"); isolateMidi(module);
        auto init = reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
        auto factoryFn = reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
        require(init && factoryFn && init(),"Initialize module");
        auto* factory = factoryFn(); require(factory,"Factory");
        HostApplication host;
        for (int i=0;i<factory->countClasses();++i) {
            PClassInfo info{}; factory->getClassInfo(i,&info);
            std::cerr << "Class: " << info.name << " / " << info.category << '\n';
            if (std::strcmp(info.category,kVstComponentControllerClass)) continue;
            IEditController* controller=nullptr;
            require(factory->createInstance(info.cid,IEditController::iid,reinterpret_cast<void**>(&controller))==kResultOk,"Controller");
            require(controller->initialize(&host)==kResultOk,"Controller initialize");
            std::cout << "id\tname\tunits\tsteps\tdefault\tflags\n";
            for(int j=0;j<controller->getParameterCount();++j) {
                ParameterInfo p{}; controller->getParameterInfo(j,p);
                std::cout<<p.id<<'\t'<<utf8(p.title)<<'\t'<<utf8(p.units)<<'\t'<<p.stepCount<<'\t'<<p.defaultNormalizedValue<<'\t'<<p.flags<<'\n';
            }
            std::cerr << "Parameters: " << controller->getParameterCount() << '\n';
            controller->terminate(); controller->release();
        }
        // Process teardown reclaims module threads and JUCE globals together.
        std::cout.flush(); std::cerr.flush(); ExitProcess(0);
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
