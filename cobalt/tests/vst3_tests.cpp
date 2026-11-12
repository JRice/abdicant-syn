#define NOMINMAX
#include "../src/model.hpp"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <windows.h>
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool v, const char *why) {
    if (!v)
        throw std::runtime_error(why);
}
int main() {
    try {
        auto dll = LoadLibraryW(
            L"cobalt/build/VST3/CobaltEditor.vst3/Contents/x86_64-win/CobaltEditor.vst3");
        check(dll, "Load VST3");
        auto init = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "InitDll"));
        auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(dll, "ExitDll"));
        auto get = reinterpret_cast<IPluginFactory *(*)()>(GetProcAddress(dll, "GetPluginFactory"));
        check(init && get && init(), "VST3 entry");
        auto *factory = get();
        PClassInfo info{};
        bool found = false;
        for (int i = 0; i < factory->countClasses(); ++i) {
            factory->getClassInfo(i, &info);
            if (!std::strcmp(info.category, kVstAudioEffectClass)) {
                found = true;
                break;
            }
        }
        check(found, "VST3 instrument class");
        HostApplication host;
        IComponent *component = nullptr;
        check(factory->createInstance(info.cid, IComponent::iid,
                                      reinterpret_cast<void **>(&component)) == kResultOk,
              "Component");
        check(component->initialize(&host) == kResultOk, "Initialize");
        IAudioProcessor *processor = nullptr;
        IEditController *controller = nullptr;
        check(component->queryInterface(IAudioProcessor::iid,
                                        reinterpret_cast<void **>(&processor)) == kResultOk,
              "Processor");
        check(component->queryInterface(IEditController::iid,
                                        reinterpret_cast<void **>(&controller)) == kResultOk,
              "Controller");
        for (auto &d : cobalt::definitions()) {
            ParameterInfo p{};
            bool exists = false;
            for (int j = 0; j < controller->getParameterCount(); ++j) {
                controller->getParameterInfo(j, p);
                if (p.id == uint32(1000 + d.id)) {
                    exists = true;
                    break;
                }
            }
            check(exists, "Every hardware parameter exposed in VST3");
        }
        ProcessSetup setup{kRealtime, kSample32, 256, 48000};
        check(processor->setupProcessing(setup) == kResultOk, "Setup");
        component->activateBus(kAudio, kOutput, 0, true);
        component->activateBus(kEvent, kInput, 0, true);
        check(component->setActive(true) == kResultOk &&
                  processor->setProcessing(true) == kResultOk,
              "Activate");
        EventList events;
        ParameterChanges changes;
        int32 index = 0;
        auto *queue = changes.addParameterData(1034, index);
        queue->addPoint(0, .25, index);
        controller->setParamNormalized(1034, .25);
        float l[256]{}, r[256]{};
        float *channels[] = {l, r};
        AudioBusBuffers output{};
        output.numChannels = 2;
        output.channelBuffers32 = channels;
        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = 256;
        data.numInputs = 1;
        AudioBusBuffers input{};
        float silentL[256]{}, silentR[256]{};
        float *silent[] = {silentL, silentR};
        input.numChannels = 2;
        input.channelBuffers32 = silent;
        data.inputs = &input;
        data.numOutputs = 1;
        data.outputs = &output;
        data.inputEvents = &events;
        data.inputParameterChanges = &changes;
        check(processor->process(data) == kResultOk, "Process automation");
        check(std::abs(controller->getParamNormalized(1034) - .25) < .0001,
              "Controller tracks host automation");
        check(l[0] == 0 && r[255] == 0, "No generated audio");
        processor->setProcessing(false);
        component->setActive(false);
        MemoryStream state;
        check(component->getState(&state) == kResultOk, "Save VST3 state");
        controller->setParamNormalized(1034, .9);
        state.seek(0, IBStream::kIBSeekSet, nullptr);
        check(component->setState(&state) == kResultOk, "Load VST3 state");
        check(std::abs(controller->getParamNormalized(1034) - 32. / 127) < .0001,
              "Recall VST3 state");
        HWND parent =
            CreateWindowExW(0, L"STATIC", L"Offline VST3 host", WS_OVERLAPPEDWINDOW, 0, 0, 1460,
                            1120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        for (int i = 0; i < 5; ++i) {
            auto *view = controller->createView(ViewType::kEditor);
            check(view, "Editor");
            check(view->attached(parent, kPlatformTypeHWND) == kResultOk, "Attach");
            ViewRect bounds;
            view->getSize(&bounds);
            check(bounds.getWidth() == 1440 && bounds.getHeight() == 1080,
                  "Native VST3 dimensions");
            check(view->removed() == kResultOk, "Remove");
            view->release();
        }
        DestroyWindow(parent);
        controller->release();
        processor->release();
        component->terminate();
        component->release();
        factory->release();
        check(exit(), "Exit");
        FreeLibrary(dll);
        std::cout << "PASS: VST3 discovery, all controller parameters, host automation, state "
                     "recall and five native-editor reopen cycles; disconnected hardware\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
