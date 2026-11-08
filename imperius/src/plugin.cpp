#include "parameters.hpp"
#include "editor.hpp"
#include <clap/clap.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
using namespace imperius;
constexpr clap_id firstId=2000;
const char* features[]={CLAP_PLUGIN_FEATURE_INSTRUMENT,CLAP_PLUGIN_FEATURE_SYNTHESIZER,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t descriptor{CLAP_VERSION_INIT,"local.abdicant.imperius","Imperius","Personal instruments","","","","0.4.0","Five-voice wavefolding synthesizer",features};
struct Plugin {
    clap_plugin_t api{};
    const clap_host_t* host;
    Engine engine;
    std::array<std::atomic<double>,Count> base;
    Values modulation{},target=defaults(),smooth=defaults();
    std::atomic<bool> snap{true};
    std::atomic<bool> textRescan{false};
    std::unique_ptr<ui::Editor> editor;
    struct UiEvent { int id;uint16_t type;double value; };
    static constexpr uint32_t queueSize=4096;
    std::array<UiEvent,queueSize> queue{};
    std::atomic<uint32_t> writeIndex{0},readIndex{0};
    int uiGesture=-1;
    double smoothing=0,tempoIncrement=0;
    explicit Plugin(const clap_host_t* h):host(h) { for(int i=0;i<Count;++i) base[i].store(definitions[i].initial); }
    ~Plugin(){editor.reset();}
    void requestFlush() {
        auto h=host->get_extension?static_cast<const clap_host_params_t*>(host->get_extension(host,CLAP_EXT_PARAMS)):nullptr;
        if(h && h->request_flush)h->request_flush(host);else if(host->request_process)host->request_process(host);
    }
    bool enqueue(int id,uint16_t type,double value,uint32_t reserve) {
        uint32_t w=writeIndex.load(std::memory_order_relaxed),r=readIndex.load(std::memory_order_acquire);
        if(w-r>=queueSize-reserve)return false;
        queue[w%queueSize]={id,type,value};writeIndex.store(w+1,std::memory_order_release);return true;
    }
    bool beginEdit(int id) {
        if(uiGesture>=0 || !enqueue(id,CLAP_EVENT_PARAM_GESTURE_BEGIN,0,2))return false;
        uiGesture=id;requestFlush();return true;
    }
    void edit(int id,double value) {
        if(uiGesture!=id)return;
        value=valid(id,value);
        if(enqueue(id,CLAP_EVENT_PARAM_VALUE,value,1)) {
            double previous=base[id].exchange(value);
            if((id==Lfo1Sync || id==Lfo2Sync) && previous!=value) {
                textRescan.store(true);if(host->request_callback)host->request_callback(host);
            }
            requestFlush();
        }
    }
    void endEdit(int id) {
        if(uiGesture!=id)return;
        enqueue(id,CLAP_EVENT_PARAM_GESTURE_END,0,0);uiGesture=-1;requestFlush();
    }
    void drain(const clap_output_events_t* out) {
        if(!out || !out->try_push)return;
        uint32_t r=readIndex.load(std::memory_order_relaxed),end=writeIndex.load(std::memory_order_acquire);
        while(r!=end) {
            const auto& e=queue[r%queueSize];bool accepted;
            if(e.type==CLAP_EVENT_PARAM_VALUE) {
                clap_event_param_value_t v{};v.header={sizeof v,0,CLAP_CORE_EVENT_SPACE_ID,e.type,CLAP_EVENT_IS_LIVE};
                v.param_id=firstId+e.id;v.note_id=v.port_index=v.channel=v.key=-1;v.value=e.value;
                accepted=out->try_push(out,&v.header);
                if(accepted){base[e.id].store(e.value);target[e.id]=valid(e.id,e.value+modulation[e.id]);}
            } else {
                clap_event_param_gesture_t g{};g.header={sizeof g,0,CLAP_CORE_EVENT_SPACE_ID,e.type,CLAP_EVENT_IS_LIVE};
                g.param_id=firstId+e.id;accepted=out->try_push(out,&g.header);
            }
            if(!accepted)break;
            readIndex.store(++r,std::memory_order_release);
        }
    }
    void targets() { for(int i=0;i<Count;++i) target[i]=valid(i,base[i].load(std::memory_order_relaxed)+modulation[i]); }
    void sync() {
        targets(); smooth=target; imperius::apply(engine.controls,smooth);
        smoothing=1-std::exp(-1/(.005*engine.sampleRate)); snap.store(false);
    }
    void transport(const clap_event_transport_t* e) {
        if(e && (e->flags&CLAP_TRANSPORT_HAS_TEMPO) && std::isfinite(e->tempo) && e->tempo>0) {
            engine.bpm=std::clamp(e->tempo,20.,300.);
            tempoIncrement=std::isfinite(e->tempo_inc)?e->tempo_inc:0;
        }
    }
    void event(const clap_event_header_t* h) {
        if(!h || h->space_id!=CLAP_CORE_EVENT_SPACE_ID) return;
        if(h->type==CLAP_EVENT_PARAM_VALUE && h->size>=sizeof(clap_event_param_value_t)) {
            const auto& e=*reinterpret_cast<const clap_event_param_value_t*>(h);
            if(e.param_id<firstId || e.param_id>=firstId+Count || !std::isfinite(e.value)
                || e.note_id!=-1 || e.port_index!=-1 || e.channel!=-1 || e.key!=-1) return;
            int i=int(e.param_id-firstId);double value=valid(i,e.value);
            const double previous=base[i].exchange(value);target[i]=valid(i,value+modulation[i]);
            if((i==Lfo1Sync || i==Lfo2Sync) && previous!=value) {
                textRescan.store(true);
                if(host->request_callback)host->request_callback(host);
            }
        } else if(h->type==CLAP_EVENT_PARAM_MOD && h->size>=sizeof(clap_event_param_mod_t)) {
            const auto& e=*reinterpret_cast<const clap_event_param_mod_t*>(h);
            if(e.param_id<firstId || e.param_id>=firstId+Count || !std::isfinite(e.amount)
                || e.note_id!=-1 || e.port_index!=-1 || e.channel!=-1 || e.key!=-1) return;
            int i=int(e.param_id-firstId);
            if(definitions[i].stepped) return;
            modulation[i]=e.amount; target[i]=valid(i,base[i].load()+e.amount);
        } else if(h->type==CLAP_EVENT_TRANSPORT && h->size>=sizeof(clap_event_transport_t)) {
            transport(reinterpret_cast<const clap_event_transport_t*>(h));
        } else if(h->type==CLAP_EVENT_MIDI && h->size>=sizeof(clap_event_midi_t)) {
            const auto& e=*reinterpret_cast<const clap_event_midi_t*>(h);
            if(e.port_index) return;
            int command=e.data[0]&0xf0,ch=e.data[0]&15,a=e.data[1]&127,b=e.data[2]&127;
            // Note/release events use controls current at this exact event time.
            if(command==0x90 && b) engine.noteOn(a,b/127.,ch);
            else if(command==0x80 || command==0x90) engine.noteOff(a,ch);
            else if(command==0xe0) { int wheel=a+(b<<7)-8192; engine.bend[ch]=2.*wheel/(wheel<0?8192:8191); }
            else if(command==0xb0) {
                if(a==64) engine.sustain(b>=64,ch);
                else if(a==120) engine.allOff(true,ch);
                else if(a==123) engine.allOff(false,ch);
                else if(a==121) { engine.sustain(false,ch);engine.bend[ch]=0; }
            }
        }
    }
    double next() {
        for(int i=0;i<Count;++i) {
            if(definitions[i].stepped) smooth[i]=target[i];
            else { smooth[i]+=(target[i]-smooth[i])*smoothing; if(std::abs(target[i]-smooth[i])<1e-10) smooth[i]=target[i]; }
        }
        imperius::apply(engine.controls,smooth);
        double out=engine.next();
        engine.bpm=std::clamp(engine.bpm+tempoIncrement,20.,300.);
        return out;
    }
};
Plugin& self(const clap_plugin_t* p) { return *static_cast<Plugin*>(p->plugin_data); }
bool CLAP_ABI init(const clap_plugin_t*) { return true; }
void CLAP_ABI destroy(const clap_plugin_t* p) { delete &self(p); }
void CLAP_ABI noop(const clap_plugin_t*) {}
void CLAP_ABI onMainThread(const clap_plugin_t* p) {
    auto& s=self(p);
    if(!s.textRescan.exchange(false))return;
    auto h=s.host->get_extension?static_cast<const clap_host_params_t*>(s.host->get_extension(s.host,CLAP_EXT_PARAMS)):nullptr;
    if(h && h->rescan)h->rescan(s.host,CLAP_PARAM_RESCAN_TEXT);
}
bool CLAP_ABI start(const clap_plugin_t*) { return true; }
bool CLAP_ABI activate(const clap_plugin_t* p,double fs,uint32_t min,uint32_t max) {
    if(!std::isfinite(fs) || fs<8000 || fs>192000 || !min || max<min) return false;
    auto& s=self(p);s.engine.reset(fs);s.modulation.fill(0);s.tempoIncrement=0;s.sync();return true;
}
void CLAP_ABI reset(const clap_plugin_t* p) {
    auto& s=self(p);double fs=s.engine.sampleRate;
    s.engine.reset(fs);s.modulation.fill(0);s.tempoIncrement=0;s.sync();
}
clap_process_status CLAP_ABI process(const clap_plugin_t* p,const clap_process_t* block) {
    if(!block || block->audio_outputs_count!=1 || !block->audio_outputs) return CLAP_PROCESS_ERROR;
    auto& out=block->audio_outputs[0];
    if(out.channel_count!=2 || (!out.data32 && !out.data64)) return CLAP_PROCESS_ERROR;
    if((out.data32 && (!out.data32[0] || !out.data32[1])) || (out.data64 && (!out.data64[0] || !out.data64[1]))) return CLAP_PROCESS_ERROR;
    auto& s=self(p);s.targets();
    if(s.snap.exchange(false)) { s.modulation.fill(0);s.sync(); }
    s.drain(block->out_events);
    s.tempoIncrement=0;s.transport(block->transport);
    uint32_t index=0,count=block->in_events?block->in_events->size(block->in_events):0;
    for(uint32_t frame=0;frame<block->frames_count;++frame) {
        while(index<count) {
            const auto* e=block->in_events->get(block->in_events,index);
            if(e && e->time>frame) break;
            s.event(e);++index;
        }
        double x=s.next();
        if(out.data32) { float f=std::clamp(float(x),-.8399999738f,.8399999738f);out.data32[0][frame]=out.data32[1][frame]=f; }
        else out.data64[0][frame]=out.data64[1][frame]=x;
    }
    out.constant_mask=0;return CLAP_PROCESS_CONTINUE;
}
uint32_t CLAP_ABI paramCount(const clap_plugin_t*) { return Count; }
bool CLAP_ABI paramInfo(const clap_plugin_t*,uint32_t i,clap_param_info_t* out) {
    if(i>=Count || !out) return false;
    *out={};const auto& d=definitions[i];out->id=firstId+i;
    out->flags=CLAP_PARAM_IS_AUTOMATABLE|(d.stepped?CLAP_PARAM_IS_STEPPED|CLAP_PARAM_IS_ENUM:CLAP_PARAM_IS_MODULATABLE);
    std::snprintf(out->name,sizeof out->name,"%s",d.name);std::snprintf(out->module,sizeof out->module,"%s",d.group);
    out->min_value=d.min;out->max_value=d.max;out->default_value=d.initial;return true;
}
bool CLAP_ABI paramValue(const clap_plugin_t* p,clap_id id,double* out) {
    if(id<firstId || id>=firstId+Count || !out) return false;
    *out=self(p).base[id-firstId].load();return true;
}
bool CLAP_ABI paramText(const clap_plugin_t* p,clap_id id,double value,char* out,uint32_t size) {
    if(id<firstId || id>=firstId+Count || !out || !size || !std::isfinite(value)) return false;
    int i=int(id-firstId);double v=valid(i,value);
    if(i==Octave) std::snprintf(out,size,"%+d oct",int(v));
    else if(i==BasicWave) {
        const char* names[]{"Square","Sine","Saw"};std::snprintf(out,size,"%s",names[int(v)]);
    } else if(i==ComplexShape) {
        if(v==0)std::snprintf(out,size,"Sine");
        else std::snprintf(out,size,"%.1f %% %s",100*std::abs(v),v<0?"Square":"Saw");
    }
    else if(i==Highpass) std::snprintf(out,size,"%s",v>=.5?"High-pass":"Low-pass");
    else if(i==Lfo1Sync || i==Lfo2Sync) std::snprintf(out,size,"%s",v>=.5?"Sync":"Free");
    else if(i==BasicTune) std::snprintf(out,size,"%+.2f st",v);
    else if(i==Cutoff) std::snprintf(out,size,"%.0f Hz",20*std::pow(1000.,v));
    else if(i==Lfo1Rate || i==Lfo2Rate) {
        int sync=i==Lfo1Rate?Lfo1Sync:Lfo2Sync;
        if(self(p).base[sync].load()>=.5) {
            const char* names[]{"2 bars","1 bar","1/2 dotted","1/2","1/4 dotted","1/4","1/8 dotted","1/8","1/8 triplet","1/16"};
            std::snprintf(out,size,"%s",names[int(std::round(v*9))]);
        } else std::snprintf(out,size,"%.3f Hz",.01*std::pow(10000.,v));
    } else if(i==AmpAttack || i==AmpDecay || i==AmpRelease || i==ModAttack || i==ModDecay || i==ModRelease)
        std::snprintf(out,size,"%.1f ms",1000*v);
    else std::snprintf(out,size,"%.1f %%",100*v);
    return true;
}
bool CLAP_ABI parse(const clap_plugin_t*,clap_id,const char*,double*) { return false; }
void CLAP_ABI flush(const clap_plugin_t* p,const clap_input_events_t* in,const clap_output_events_t* out) {
    self(p).drain(out);
    if(in) for(uint32_t i=0;i<in->size(in);++i) {
        const auto* e=in->get(in,i);
        if(e && (e->type==CLAP_EVENT_PARAM_VALUE || e->type==CLAP_EVENT_PARAM_MOD)) self(p).event(e);
    }
}
const clap_plugin_params_t params{paramCount,paramInfo,paramValue,paramText,parse,flush};
uint32_t CLAP_ABI audioCount(const clap_plugin_t*,bool input) { return input?0:1; }
bool CLAP_ABI audioInfo(const clap_plugin_t*,uint32_t index,bool input,clap_audio_port_info_t* out) {
    if(input || index || !out) return false;
    *out={};out->id=0;std::strcpy(out->name,"Output");out->flags=CLAP_AUDIO_PORT_IS_MAIN|CLAP_AUDIO_PORT_SUPPORTS_64BITS;
    out->channel_count=2;out->port_type=CLAP_PORT_STEREO;out->in_place_pair=CLAP_INVALID_ID;return true;
}
const clap_plugin_audio_ports_t audioPorts{audioCount,audioInfo};
uint32_t CLAP_ABI noteCount(const clap_plugin_t*,bool input) { return input?1:0; }
bool CLAP_ABI noteInfo(const clap_plugin_t*,uint32_t index,bool input,clap_note_port_info_t* out) {
    if(!input || index || !out) return false;
    *out={};out->id=0;out->supported_dialects=out->preferred_dialect=CLAP_NOTE_DIALECT_MIDI;
    std::strcpy(out->name,"MIDI notes, sustain and pitch bend");return true;
}
const clap_plugin_note_ports_t notePorts{noteCount,noteInfo};
uint32_t CLAP_ABI latency(const clap_plugin_t*) { return Decimator::latency; }
const clap_plugin_latency_t latencyExtension{latency};
bool writeAll(const clap_ostream_t* s,const void* data,uint64_t size) {
    if(!s || !s->write) return false;
    auto ptr=static_cast<const char*>(data);
    while(size) { auto n=s->write(s,ptr,size);if(n<=0 || uint64_t(n)>size)return false;ptr+=n;size-=n; } return true;
}
bool readAll(const clap_istream_t* s,void* data,uint64_t size) {
    if(!s || !s->read) return false;
    auto ptr=static_cast<char*>(data);
    while(size) { auto n=s->read(s,ptr,size);if(n<=0 || uint64_t(n)>size)return false;ptr+=n;size-=n; } return true;
}
bool CLAP_ABI save(const clap_plugin_t* p,const clap_ostream_t* stream) {
    Values v{};for(int i=0;i<Count;++i)v[i]=self(p).base[i].load();
    return writeAll(stream,"IMPR0002",8) && writeAll(stream,v.data(),sizeof v);
}
bool CLAP_ABI load(const clap_plugin_t* p,const clap_istream_t* stream) {
    char magic[8];Values v=defaults();
    if(!readAll(stream,magic,8))return false;
    const bool legacy=!std::memcmp(magic,"IMPR0001",8);
    if(!legacy && std::memcmp(magic,"IMPR0002",8))return false;
    const size_t bytes=legacy?sizeof(double)*ComplexShape:sizeof v;
    if(!readAll(stream,v.data(),bytes))return false;
    for(int i=0;i<Count;++i) if(!std::isfinite(v[i]) || v[i]<definitions[i].min || v[i]>definitions[i].max) return false;
    auto& s=self(p);for(int i=0;i<Count;++i)s.base[i].store(valid(i,v[i]));s.snap.store(true);
    auto h=s.host->get_extension?static_cast<const clap_host_params_t*>(s.host->get_extension(s.host,CLAP_EXT_PARAMS)):nullptr;
    if(h && h->rescan) h->rescan(s.host,CLAP_PARAM_RESCAN_VALUES|CLAP_PARAM_RESCAN_TEXT);
    return true;
}
const clap_plugin_state_t state{save,load};
uint32_t CLAP_ABI pageCount(const clap_plugin_t*) { return 6; }
bool CLAP_ABI page(const clap_plugin_t*,uint32_t i,clap_remote_controls_page_t* out) {
    if(i>=6 || !out) return false;
    const char* names[]{"Tone","Generators","Filter and Amp","Modulation","LFOs","Waves and Audio Mod"};
    const int ids[6][8]{
        {Harmonics,Fold,Bias,PhaseMod,Cutoff,Resonance,FoldEnv,Volume},
        {Octave,BasicTune,ComplexLevel,BasicLevel,SubLevel,HarmonicMod,FoldMod,PhaseMod},
        {Cutoff,Resonance,FilterEnv,Highpass,AmpAttack,AmpDecay,AmpSustain,AmpRelease},
        {ModAttack,ModDecay,ModSustain,ModRelease,HarmonicEnv,FoldEnv,HarmonicLfo,FoldLfo},
        {Lfo1Rate,Lfo1Shape,Lfo1Sync,HarmonicLfo,Lfo2Rate,Lfo2Shape,Lfo2Sync,FoldLfo},
        {ComplexShape,BasicWave,Harmonics,PhaseMod,HarmonicMod,FoldMod,BasicTune,Fold}};
    *out={};out->page_id=i;std::strcpy(out->section_name,"Imperius");std::strcpy(out->page_name,names[i]);
    for(int j=0;j<8;++j)out->param_ids[j]=firstId+ids[i][j];
    return true;
}
const clap_plugin_remote_controls_t remotes{pageCount,page};
bool CLAP_ABI guiSupported(const clap_plugin_t*,const char* api,bool floating) {
    return api && !floating && !std::strcmp(api,CLAP_WINDOW_API_WIN32);
}
bool CLAP_ABI guiPreferred(const clap_plugin_t*,const char** api,bool* floating) {
    *api=CLAP_WINDOW_API_WIN32;*floating=false;return true;
}
bool CLAP_ABI guiCreate(const clap_plugin_t* p,const char* api,bool floating) {
    auto& s=self(p);if(s.editor || !guiSupported(p,api,floating))return false;
    ui::Hooks hooks{&s,
        [](void* ctx,int id){return static_cast<Plugin*>(ctx)->base[id].load();},
        [](void* ctx,int id){return static_cast<Plugin*>(ctx)->beginEdit(id);},
        [](void* ctx,int id,double value){static_cast<Plugin*>(ctx)->edit(id,value);},
        [](void* ctx,int id){static_cast<Plugin*>(ctx)->endEdit(id);},
        [](void* ctx,int id,double value,char* out,size_t size){paramText(&static_cast<Plugin*>(ctx)->api,firstId+id,value,out,uint32_t(size));}};
    auto editor=std::make_unique<ui::Editor>(hooks);if(!editor->create())return false;
    s.editor=std::move(editor);return true;
}
void CLAP_ABI guiDestroy(const clap_plugin_t* p){self(p).editor.reset();}
bool CLAP_ABI guiSize(const clap_plugin_t* p,uint32_t* w,uint32_t* h) {
    if(!self(p).editor)return false;
    self(p).editor->dimensions(w,h);return true;
}
bool CLAP_ABI guiScale(const clap_plugin_t* p,double scale) {
    if(!self(p).editor || !std::isfinite(scale) || scale<.5 || scale>2)return false;
    return self(p).editor->size(uint32_t(std::round(ui::preferredWidth*scale)),uint32_t(std::round(ui::preferredHeight*scale)));
}
bool CLAP_ABI guiCanResize(const clap_plugin_t*){return false;}
bool CLAP_ABI guiHints(const clap_plugin_t*,clap_gui_resize_hints_t*){return false;}
bool CLAP_ABI guiAdjust(const clap_plugin_t*,uint32_t*,uint32_t*){return false;}
bool CLAP_ABI guiSetSize(const clap_plugin_t* p,uint32_t w,uint32_t h) {
    if(!self(p).editor)return false;
    uint32_t currentW,currentH;self(p).editor->dimensions(&currentW,&currentH);return w==currentW && h==currentH;
}
bool CLAP_ABI guiParent(const clap_plugin_t* p,const clap_window_t* window) {
    return window && window->api && !std::strcmp(window->api,CLAP_WINDOW_API_WIN32) && self(p).editor && self(p).editor->parent(static_cast<HWND>(window->win32));
}
bool CLAP_ABI guiTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void CLAP_ABI guiTitle(const clap_plugin_t*,const char*){}
bool CLAP_ABI guiShow(const clap_plugin_t* p){return self(p).editor && self(p).editor->show(true);}
bool CLAP_ABI guiHide(const clap_plugin_t* p){return self(p).editor && self(p).editor->show(false);}
const clap_plugin_gui_t gui{guiSupported,guiPreferred,guiCreate,guiDestroy,guiScale,guiSize,guiCanResize,guiHints,guiAdjust,guiSetSize,guiParent,guiTransient,guiTitle,guiShow,guiHide};
const void* CLAP_ABI extension(const clap_plugin_t*,const char* id) {
    if(!id)return nullptr;
    if(!std::strcmp(id,CLAP_EXT_GUI))return &gui;
    if(!std::strcmp(id,CLAP_EXT_PARAMS))return &params;
    if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audioPorts;
    if(!std::strcmp(id,CLAP_EXT_NOTE_PORTS))return &notePorts;
    if(!std::strcmp(id,CLAP_EXT_STATE))return &state;
    if(!std::strcmp(id,CLAP_EXT_LATENCY))return &latencyExtension;
    if(!std::strcmp(id,CLAP_EXT_REMOTE_CONTROLS) || !std::strcmp(id,CLAP_EXT_REMOTE_CONTROLS_COMPAT))return &remotes;
    return nullptr;
}
uint32_t CLAP_ABI count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* CLAP_ABI describe(const clap_plugin_factory_t*,uint32_t index) { return index?nullptr:&descriptor; }
const clap_plugin_t* CLAP_ABI create(const clap_plugin_factory_t*,const clap_host_t* host,const char* id) {
    if(!host || !id || !clap_version_is_compatible(host->clap_version) || std::strcmp(id,descriptor.id))return nullptr;
    auto s=new(std::nothrow) Plugin(host);if(!s)return nullptr;
    s->api={&descriptor,s,init,destroy,activate,noop,start,noop,reset,process,extension,onMainThread};return &s->api;
}
const clap_plugin_factory_t factory{count,describe,create};
bool CLAP_ABI entryInit(const char*) { return true; }
void CLAP_ABI entryDeinit() {}
const void* CLAP_ABI getFactory(const char* id) { return id && !std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr; }
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION_INIT,entryInit,entryDeinit,getFactory};
