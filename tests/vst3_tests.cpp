#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include "../src/synth.hpp"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Handler : IComponentHandler {
    int begin=0,edit=0,end=0;ParamID last=0;
    tresult PLUGIN_API queryInterface(const TUID iid,void** out) override {
        if(FUnknownPrivate::iidEqual(iid,IComponentHandler::iid)||FUnknownPrivate::iidEqual(iid,FUnknown::iid)){*out=this;return kResultOk;}*out=nullptr;return kNoInterface;
    }
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    tresult PLUGIN_API beginEdit(ParamID id)override{++begin;last=id;return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue)override{++edit;last=id;return kResultOk;}
    tresult PLUGIN_API endEdit(ParamID id)override{++end;last=id;return kResultOk;}
    tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
};
void pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
int main(){try{
    auto dll=LoadLibraryW(L"build/VST3/Shiboleth.vst3/Contents/x86_64-win/Shiboleth.vst3");check(dll,"Load self-contained VST3");
    auto init=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"InitDll"));auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(dll,"ExitDll"));
    auto getFactory=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(dll,"GetPluginFactory"));check(init&&exit&&getFactory&&init(),"VST3 entry points");
    auto* factory=getFactory();check(factory && factory->countClasses()>=1,"VST3 discovery");PClassInfo info{};bool found=false;
    for(int i=0;i<factory->countClasses();++i){factory->getClassInfo(i,&info);if(!std::strcmp(info.category,kVstAudioEffectClass)){found=true;break;}}
    check(found && !std::strcmp(info.name,"Shiboleth"),"Instrument class/name");
    IComponent* component=nullptr;check(factory->createInstance(info.cid,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk,"Create component");
    HostApplication host;check(component->initialize(&host)==kResultOk,"Initialize");
    IAudioProcessor* processor=nullptr;IEditController* controller=nullptr;
    check(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"Processor interface");
    check(component->queryInterface(IEditController::iid,reinterpret_cast<void**>(&controller))==kResultOk,"Controller interface");
    Handler handler;controller->setComponentHandler(&handler);
    check(component->getBusCount(kAudio,kOutput)==1 && component->getBusCount(kEvent,kInput)==1,"Audio and MIDI ports");
    for(int i=0;i<syn::Count;++i){ParameterInfo param{};bool exists=false;
        for(int j=0;j<controller->getParameterCount();++j){controller->getParameterInfo(j,param);if(param.id==uint32(1000+i)){exists=true;break;}}
        check(exists,"All synth parameters exposed");}
    IMidiMapping* midi=nullptr;check(controller->queryInterface(IMidiMapping::iid,reinterpret_cast<void**>(&midi))==kResultOk,"MIDI expression mapping");
    ParamID wheel,bend,pressure;
    check(midi->getMidiControllerAssignment(0,0,kCtrlModWheel,wheel)==kResultOk,"Modwheel mapped");
    check(midi->getMidiControllerAssignment(0,0,kPitchBend,bend)==kResultOk,"Pitch bend mapped");
    check(midi->getMidiControllerAssignment(0,0,kAfterTouch,pressure)==kResultOk,"Aftertouch mapped");
    ProcessSetup setup{kRealtime,kSample32,256,48000};check(processor->setupProcessing(setup)==kResultOk,"Setup processing");
    component->activateBus(kAudio,kOutput,0,true);component->activateBus(kEvent,kInput,0,true);
    check(component->setActive(true)==kResultOk && processor->setProcessing(true)==kResultOk,"Activate");check(processor->getLatencySamples()==12,"Latency");
    EventList events;ParameterChanges changes;float left[256]{},right[256]{};float* channels[]={left,right};
    AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=channels;
    ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=256;data.numOutputs=1;data.outputs=&output;data.inputEvents=&events;data.inputParameterChanges=&changes;
    Event note{};note.type=Event::kNoteOnEvent;note.sampleOffset=32;note.noteOn.pitch=60;note.noteOn.velocity=100.f/127;note.noteOn.noteId=1;events.addEvent(note);
    syn::Engine reference;reference.smooth=reference.values;double energy=0;
    for(int block=0;block<20;++block){check(processor->process(data)==kResultOk,"Process notes");
        for(int i=0;i<256;++i){if(block==0 && i==32)reference.noteOn(60,double(100.f/127));
            double expected=reference.next();check(std::abs(left[i]-expected)<1e-6,"VST3 audio matches shared engine");check(left[i]==right[i],"Dual mono");energy+=left[i]*left[i];}
        events.clear();}
    check(energy>.001,"Audible VST3 output");
    auto automate=[&](ParamID id,double v){int32 index=0;auto* queue=changes.addParameterData(id,index);queue->addPoint(0,v,index);};
    automate(1000+syn::Cutoff,.23);automate(wheel,.5);automate(bend,.75);automate(pressure,.4);
    check(processor->process(data)==kResultOk,"Automation and expression processing");changes.clearQueue();
    reference.values[syn::Cutoff]=.23;reference.modwheel=63./127;reference.pressure=50./127;reference.bend=2.*(12287-8192)/8191;
    // Compare the block just processed, including expression conversion to MIDI.
    for(int i=0;i<256;++i)check(std::abs(left[i]-reference.next())<1e-6,"MIDI expression and automation reach engine");
    Event off{};off.type=Event::kNoteOffEvent;off.noteOff.pitch=60;off.noteOff.noteId=1;off.sampleOffset=17;events.addEvent(off);
    check(processor->process(data)==kResultOk,"Note off processed");
    for(int i=0;i<256;++i){if(i==17)reference.noteOff(60);check(std::abs(left[i]-reference.next())<1e-6,"Sample-accurate note off");}events.clear();
    processor->setProcessing(false);component->setActive(false);
    MemoryStream state;check(component->getState(&state)==kResultOk,"Save VST3 state");
    controller->setParamNormalized(1000+syn::Cutoff,.8);
    state.seek(0,IBStream::kIBSeekSet,nullptr);check(component->setState(&state)==kResultOk,"Restore VST3 state");
    check(std::abs(controller->getParamNormalized(1000+syn::Cutoff)-.23)<1e-9,"Restored automation value");
    component->setActive(true);processor->setProcessing(true);
    HWND parent=CreateWindowExW(0,L"STATIC",L"VST3 test host",WS_OVERLAPPEDWINDOW,0,0,1320,810,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(parent,"Parent window");
    for(int cycle=0;cycle<12;++cycle){auto* view=controller->createView(ViewType::kEditor);check(view,"Create editor");
        if(cycle==0){IPlugViewContentScaleSupport* scaling=nullptr;
            check(view->queryInterface(IPlugViewContentScaleSupport::iid,reinterpret_cast<void**>(&scaling))==kResultOk,"VST3 scaling interface");
            check(scaling->setContentScaleFactor(1.5f)==kResultFalse,"VST3 first-open scale hint declined");scaling->release();}
        check(view->attached(parent,kPlatformTypeHWND)==kResultOk,"Attach editor");ViewRect bounds;view->getSize(&bounds);check(bounds.getWidth()==1300 && bounds.getHeight()==765,"Editor dimensions");
        HWND child=GetWindow(parent,GW_CHILD);check(child,"Native editor exists");
        {Gdiplus::Bitmap bitmap(1300,765,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&bitmap);HDC dc=graphics.GetHDC();SendMessageW(child,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);graphics.ReleaseHDC(dc);
         if(cycle==0){CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};check(bitmap.Save(L"build/editor-vst3.png",&png,nullptr)==Gdiplus::Ok,"Editor screenshot");}}
        LPARAM point=MAKELPARAM(int(364*1300./1635),int(751*765./962));SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point);SendMessageW(child,WM_LBUTTONUP,0,point);
        processor->process(data);for(int n=0;n<5;++n){Sleep(10);pump();}
        check(view->removed()==kResultOk,"Detach editor");view->release();check(!GetWindow(parent,GW_CHILD),"Editor removed");processor->process(data);
    }
    check(handler.begin>0 && handler.edit>0 && handler.end>0,"UI sends VST3 automation gestures");
    DestroyWindow(parent);processor->setProcessing(false);component->setActive(false);midi->release();controller->setComponentHandler(nullptr);controller->release();processor->release();component->terminate();component->release();factory->release();check(exit(),"Module exit");FreeLibrary(dll);
    std::cout<<"PASS: VST3 loading, ports, parameters, sample-accurate audio parity, expression mapping, automation/state, 12 editor reopen cycles\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
