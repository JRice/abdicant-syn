#include "synth.hpp"
#include "editor.hpp"
#include <clap/clap.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
constexpr clap_id firstId=1000;
const char* features[]={CLAP_PLUGIN_FEATURE_INSTRUMENT,CLAP_PLUGIN_FEATURE_SYNTHESIZER,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t descriptor={CLAP_VERSION_INIT,"local.abdicant.mono.prototype","Shibboleth","Personal instruments","","","","0.9.0","Two oscillator mono synthesizer",features};
struct Plugin {
    clap_plugin_t api{};
    const clap_host_t* host;
    syn::Engine engine;
    std::array<std::atomic<double>,syn::Count> base;
    syn::Values modulation{};
    std::unique_ptr<ui::Editor> editor;
    struct UiEvent { int id; uint16_t type; double value; };
    static constexpr uint32_t queueSize=4096;
    std::array<UiEvent,queueSize> queue{};
    std::atomic<uint32_t> writeIndex{0},readIndex{0};
    int uiGesture=-1; // main thread only; one pointer gesture at a time
    std::atomic<bool> scopesEnabled{false};
    std::array<std::atomic<float>,256> scopeData{};
    explicit Plugin(const clap_host_t* h):host(h) { for(int i=0;i<syn::Count;++i) base[i].store(syn::definitions[i].initial); }
    ~Plugin(){editor.reset();}
    void refresh(int i) { engine.values[i]=syn::validValue(i,base[i].load(std::memory_order_relaxed)+modulation[i]); }
    void requestFlush() {
        auto h=static_cast<const clap_host_params_t*>(host->get_extension(host,CLAP_EXT_PARAMS));
        if(h && h->request_flush)h->request_flush(host);else host->request_process(host);
    }
    bool enqueue(int id,uint16_t type,double value,uint32_t reserve) {
        const uint32_t w=writeIndex.load(std::memory_order_relaxed),r=readIndex.load(std::memory_order_acquire);
        if(w-r>=queueSize-reserve)return false;
        queue[w%queueSize]={id,type,value};writeIndex.store(w+1,std::memory_order_release);return true;
    }
    bool beginEdit(int id) {
        if(uiGesture>=0 || !enqueue(id,CLAP_EVENT_PARAM_GESTURE_BEGIN,0,2))return false;
        uiGesture=id;requestFlush();return true;
    }
    void edit(int id,double v) {
        if(uiGesture!=id)return;
        v=syn::validValue(id,v);
        if(enqueue(id,CLAP_EVENT_PARAM_VALUE,v,1)){base[id].store(v);requestFlush();}
    }
    void endEdit(int id) {
        if(uiGesture!=id)return;
        // A value event always reserves one slot for its gesture end.
        enqueue(id,CLAP_EVENT_PARAM_GESTURE_END,0,0);uiGesture=-1;requestFlush();
    }
    void drain(const clap_output_events_t* out) {
        if(!out)return;
        uint32_t r=readIndex.load(std::memory_order_relaxed);
        const uint32_t end=writeIndex.load(std::memory_order_acquire);
        while(r!=end){const auto& e=queue[r%queueSize];bool accepted=false;
            if(e.type==CLAP_EVENT_PARAM_VALUE){
                clap_event_param_value_t v{};v.header={sizeof v,0,CLAP_CORE_EVENT_SPACE_ID,e.type,CLAP_EVENT_IS_LIVE};
                v.param_id=firstId+e.id;v.note_id=v.port_index=v.channel=v.key=-1;v.value=e.value;
                accepted=out->try_push(out,&v.header);
                if(accepted){base[e.id].store(e.value);refresh(e.id);}
            }else{clap_event_param_gesture_t g{};g.header={sizeof g,0,CLAP_CORE_EVENT_SPACE_ID,e.type,CLAP_EVENT_IS_LIVE};g.param_id=firstId+e.id;accepted=out->try_push(out,&g.header);}
            if(!accepted)break;
            readIndex.store(++r,std::memory_order_release);
        }
    }
    void event(const clap_event_header_t* e) {
        if(!e || e->space_id!=CLAP_CORE_EVENT_SPACE_ID) return;
        if(e->type==CLAP_EVENT_PARAM_VALUE && e->size>=sizeof(clap_event_param_value_t)) {
            auto v=reinterpret_cast<const clap_event_param_value_t*>(e);
            if(v->param_id>=firstId && v->param_id<firstId+syn::Count && std::isfinite(v->value)) {
                int i=int(v->param_id-firstId); base[i].store(syn::validValue(i,v->value)); refresh(i);
            }
        } else if(e->type==CLAP_EVENT_PARAM_MOD && e->size>=sizeof(clap_event_param_mod_t)) {
            auto v=reinterpret_cast<const clap_event_param_mod_t*>(e);
            if(v->param_id>=firstId && v->param_id<firstId+syn::Count && std::isfinite(v->amount)
                && v->note_id==-1 && v->key==-1 && v->channel==-1) {
                int i=int(v->param_id-firstId); modulation[i]=v->amount; refresh(i);
            }
#ifdef SHIBOLETH_VST3
        } else if((e->type==CLAP_EVENT_NOTE_ON || e->type==CLAP_EVENT_NOTE_OFF) && e->size>=sizeof(clap_event_note_t)) {
            auto n=reinterpret_cast<const clap_event_note_t*>(e);
            if(n->port_index!=0 && n->port_index!=-1)return;
            if(e->type==CLAP_EVENT_NOTE_ON){
                if(n->key>=0 && n->key<128 && n->channel>=0 && n->channel<16 && std::isfinite(n->velocity))
                    engine.noteOn(n->key,std::clamp(n->velocity,0.0,1.0),n->channel,n->note_id);
            }else for(int i=engine.noteCount-1;i>=0;--i){const auto held=engine.notes[i];
                if((n->key<0 || n->key==held.key) && (n->channel<0 || n->channel==held.channel) && (n->note_id<0 || n->note_id==held.noteId))engine.noteOff(held.key,held.channel);
            }
        } else if(e->type==CLAP_EVENT_NOTE_EXPRESSION && e->size>=sizeof(clap_event_note_expression_t) && engine.noteCount){
            auto n=reinterpret_cast<const clap_event_note_expression_t*>(e);const auto& held=engine.notes[engine.noteCount-1];
            if(n->expression_id==CLAP_NOTE_EXPRESSION_PRESSURE && (n->port_index<0 || n->port_index==0) &&
                (n->key<0 || n->key==held.key) && (n->channel<0 || n->channel==held.channel) && (n->note_id<0 || n->note_id==held.noteId) && std::isfinite(n->value))engine.pressure=std::clamp(n->value,0.0,1.0);
#endif
        } else if(e->type==CLAP_EVENT_MIDI && e->size>=sizeof(clap_event_midi_t)) {
            auto v=reinterpret_cast<const clap_event_midi_t*>(e); if(v->port_index!=0) return;
            int command=v->data[0]&0xf0, channel=v->data[0]&15, a=v->data[1]&127,b=v->data[2]&127;
            if(command==0x90 && b) engine.noteOn(a,b/127.0,channel);
            else if(command==0x80 || command==0x90) engine.noteOff(a,channel);
            else if(command==0xe0) { int wheel=a+(b<<7)-8192; engine.bend=2.0*wheel/(wheel<0?8192:8191); }
            else if(command==0xd0) engine.pressure=a/127.0;
            else if(command==0xa0 && engine.noteCount && engine.notes[engine.noteCount-1].key==a
                    && engine.notes[engine.noteCount-1].channel==channel) engine.pressure=b/127.0;
            else if(command==0xb0) {
                if(a==1) engine.modwheel=b/127.0;
                if(a==120 || a==123) engine.allOff(a==120);
                if(a==121) { engine.modwheel=engine.pressure=engine.bend=0; }
            }
        }
    }
};
Plugin& self(const clap_plugin_t* p) { return *static_cast<Plugin*>(p->plugin_data); }
bool CLAP_ABI init(const clap_plugin_t*) { return true; }
void CLAP_ABI destroy(const clap_plugin_t* p) { delete &self(p); }
bool CLAP_ABI activate(const clap_plugin_t* p,double fs,uint32_t,uint32_t) {
    if(!std::isfinite(fs) || fs<8000 || fs>384000) return false;
    auto& s=self(p); s.engine.reset(fs); s.modulation.fill(0);
    for(int i=0;i<syn::Count;++i) { s.refresh(i); } s.engine.smooth=s.engine.values; return true;
}
void CLAP_ABI noop(const clap_plugin_t*) {}
bool CLAP_ABI start(const clap_plugin_t*) { return true; }
void CLAP_ABI reset(const clap_plugin_t* p) { auto& s=self(p); double rate=s.engine.sampleRate;
    s.engine.reset(rate); s.modulation.fill(0); for(int i=0;i<syn::Count;++i) { s.refresh(i); } s.engine.smooth=s.engine.values; }
clap_process_status CLAP_ABI process(const clap_plugin_t* p,const clap_process_t* block) {
    auto& s=self(p);
    if(block->audio_outputs_count<1 || block->audio_outputs[0].channel_count<2 || !block->audio_outputs[0].data32) return CLAP_PROCESS_ERROR;
    for(int i=0;i<syn::Count;++i) { s.refresh(i); }
    s.drain(block->out_events);
    s.engine.captureScopes=s.scopesEnabled.load(std::memory_order_relaxed);
    uint32_t eventIndex=0, count=block->in_events?block->in_events->size(block->in_events):0;
    for(uint32_t frame=0;frame<block->frames_count;++frame) {
        while(eventIndex<count) { auto e=block->in_events->get(block->in_events,eventIndex);
            if(e && e->time>frame) { break; }
            s.event(e); ++eventIndex; }
        float sample=static_cast<float>(s.engine.next());
        // Round down the representable ceiling to preserve the bound in float32.
        constexpr float limit=0.84139508f;
        sample=std::clamp(sample,-limit,limit);
        block->audio_outputs[0].data32[0][frame]=sample;
        block->audio_outputs[0].data32[1][frame]=sample;
    }
    block->audio_outputs[0].constant_mask=0;
    if(s.engine.captureScopes)for(int osc=0;osc<2;++osc)for(int i=0;i<128;++i)s.scopeData[osc*128+i].store(s.engine.scopes[osc].samples[i],std::memory_order_relaxed);
    return CLAP_PROCESS_CONTINUE;
}
uint32_t CLAP_ABI paramCount(const clap_plugin_t*) { return syn::Count; }
bool CLAP_ABI paramInfo(const clap_plugin_t*,uint32_t i,clap_param_info_t* out) {
    if(i>=syn::Count) return false;
    *out={}; const auto& d=syn::definitions[i];
    out->id=firstId+i; out->flags=CLAP_PARAM_IS_AUTOMATABLE;
    out->flags|=d.stepped?CLAP_PARAM_IS_STEPPED|CLAP_PARAM_IS_ENUM:CLAP_PARAM_IS_MODULATABLE;
    const char* prefix=i==syn::FilterAlgorithm?"FILTER":i==syn::Octave?"OSC1":i>=syn::CrushEnv2?"OSC2":i>=syn::CrushEnv1?"OSC1":i<=syn::SquishDepth1?"OSC1":i<=syn::Tune2?"OSC2":i<=syn::FilterAmount?"FILTER":i<=syn::Release?"ENV":i<=syn::AftertouchDepth?"PERF":i==syn::Volume?"OUT":"OPTIONS";
    std::snprintf(out->name,sizeof out->name,"%s: %s",prefix,d.name); std::snprintf(out->module,sizeof out->module,"%s",d.group);
    out->min_value=d.min; out->max_value=d.max; out->default_value=d.initial; return true;
}
bool CLAP_ABI paramValue(const clap_plugin_t* p,clap_id id,double* out) {
    if(id<firstId || id>=firstId+syn::Count) return false;
    *out=self(p).base[id-firstId].load(); return true;
}
bool CLAP_ABI paramText(const clap_plugin_t*,clap_id id,double v,char* out,uint32_t size) {
    if(id<firstId || id>=firstId+syn::Count || !size || !std::isfinite(v)) return false;
    int i=int(id-firstId); v=syn::validValue(i,v);
    const char* waves[]={"Triangle","Saw","Pulse","Half triangle","Square + triangle","Trapezoid","Folded triangle"};
    const char* resonances[]={"ooo (0)","*oo (0.1)","**o (0.5)","*** (0.95)"};
    const char* targets[]={"Cutoff","Resonance","Osc 2 mix","Osc 1 Alt","Osc 2 Alt","Osc 1 + 2 Alt"};
    if(i==syn::Wave1 || i==syn::Wave2) std::snprintf(out,size,"%s",waves[int(v)]);
    else if(i==syn::FilterAlgorithm) { const char* names[]={"Original","Wasp","Ladder","OTA"}; std::snprintf(out,size,"%s",names[int(v)]); }
    else if(i==syn::Octave) std::snprintf(out,size,"%+d oct",int(v));
    else if(i==syn::Resonance) std::snprintf(out,size,"%s",resonances[int(v)]);
    else if(i==syn::AftertouchTarget) std::snprintf(out,size,"%s",targets[int(v)]);
    else if(i==syn::Tune2) std::snprintf(out,size,"%+.1f cents",syn::tuning(v)*100);
    else if(i==syn::Cutoff) std::snprintf(out,size,"%.0f Hz",25*std::exp2(9*v));
    else if(i==syn::Attack || i==syn::Decay || i==syn::Release || i==syn::Glide) std::snprintf(out,size,"%.1f ms",1000*v);
    else if(i==syn::VibratoRate || i==syn::SquishRate1 || i==syn::SquishRate2 || i==syn::NoiseCutoff) std::snprintf(out,size,"%.2f Hz",v);
    else std::snprintf(out,size,"%.1f %%",100*v);
    return true;
}
// The prototype uses formatted units but deliberately declines typed text parsing.
bool CLAP_ABI parseText(const clap_plugin_t*,clap_id,const char*,double*) { return false; }
void CLAP_ABI flush(const clap_plugin_t* p,const clap_input_events_t* input,const clap_output_events_t* output) {
    self(p).drain(output);
    if(input) for(uint32_t i=0;i<input->size(input);++i) self(p).event(input->get(input,i));
}
const clap_plugin_params_t params={paramCount,paramInfo,paramValue,paramText,parseText,flush};
uint32_t CLAP_ABI audioCount(const clap_plugin_t*,bool input) { return input?0:1; }
bool CLAP_ABI audioInfo(const clap_plugin_t*,uint32_t index,bool input,clap_audio_port_info_t* out) {
    if(input || index) return false;
    *out={}; out->id=0; std::strcpy(out->name,"Output");
    out->flags=CLAP_AUDIO_PORT_IS_MAIN; out->channel_count=2; out->port_type=CLAP_PORT_STEREO; out->in_place_pair=CLAP_INVALID_ID; return true;
}
const clap_plugin_audio_ports_t audioPorts={audioCount,audioInfo};
uint32_t CLAP_ABI noteCount(const clap_plugin_t*,bool input) { return input?1:0; }
bool CLAP_ABI noteInfo(const clap_plugin_t*,uint32_t index,bool input,clap_note_port_info_t* out) {
    if(!input || index) return false;
    *out={}; out->id=0;
    out->supported_dialects=out->preferred_dialect=CLAP_NOTE_DIALECT_MIDI;
    std::strcpy(out->name,"Notes and expression"); return true;
}
const clap_plugin_note_ports_t notePorts={noteCount,noteInfo};
uint32_t CLAP_ABI latency(const clap_plugin_t*) { return 12; }
const clap_plugin_latency_t latencyExtension={latency};
bool writeAll(const clap_ostream_t* s,const void* data,uint64_t size) {
    auto ptr=static_cast<const char*>(data); while(size) { auto n=s->write(s,ptr,size); if(n<=0 || uint64_t(n)>size) return false; ptr+=n; size-=n; } return true;
}
bool readAll(const clap_istream_t* s,void* data,uint64_t size) {
    auto ptr=static_cast<char*>(data); while(size) { auto n=s->read(s,ptr,size); if(n<=0 || uint64_t(n)>size) return false; ptr+=n; size-=n; } return true;
}
bool CLAP_ABI save(const clap_plugin_t* p,const clap_ostream_t* stream) {
    syn::Values values{}; for(int i=0;i<syn::Count;++i) values[i]=self(p).base[i].load();
    return writeAll(stream,"ASYN0007",8) && writeAll(stream,values.data(),sizeof values);
}
bool CLAP_ABI load(const clap_plugin_t* p,const clap_istream_t* stream) {
    char magic[8]; syn::Values values=syn::defaults();
    if(!readAll(stream,magic,8)) return false;
    const bool legacy=std::memcmp(magic,"ASYN0001",8)==0;
    const bool version2=std::memcmp(magic,"ASYN0002",8)==0;
    const bool version3=std::memcmp(magic,"ASYN0003",8)==0;
    const bool version4=std::memcmp(magic,"ASYN0004",8)==0;
    const bool version5=std::memcmp(magic,"ASYN0005",8)==0;
    const bool version6=std::memcmp(magic,"ASYN0006",8)==0;
    if(!legacy && !version2 && !version3 && !version4 && !version5 && !version6 && std::memcmp(magic,"ASYN0007",8)) return false;
    const size_t bytes=legacy?sizeof(double)*syn::NoiseCutoff:version2?sizeof(double)*syn::CrushEnv1:version3?sizeof(double)*syn::Octave:version4?sizeof(double)*syn::FilterAlgorithm:sizeof values;
    if(!readAll(stream,values.data(),bytes)) return false;
    // v6 appended the retired Character selector after the surviving values.
    if(version6) {
        double retiredCharacter;
        if(!readAll(stream,&retiredCharacter,sizeof retiredCharacter) ||
           !(retiredCharacter==0 || retiredCharacter==1)) return false;
    }
    for(int i=0;i<syn::Count;++i) if(!std::isfinite(values[i]) || values[i]<syn::definitions[i].min || values[i]>syn::definitions[i].max) return false;
    for(int i=0;i<syn::Count;++i) self(p).base[i].store(syn::validValue(i,values[i]));
    auto h=static_cast<const clap_host_params_t*>(self(p).host->get_extension(self(p).host,CLAP_EXT_PARAMS));
    if(h) { h->rescan(self(p).host,CLAP_PARAM_RESCAN_VALUES); }
    return true;
}
const clap_plugin_state_t state={save,load};
uint32_t CLAP_ABI pageCount(const clap_plugin_t*) { return 4; }
bool CLAP_ABI page(const clap_plugin_t*,uint32_t i,clap_remote_controls_page_t* out) {
    if(i>=4) return false;
    const char* names[]={"Tone","Oscillator 1","Oscillator 2","Performance"};
    const int ids[4][8]={{syn::Cutoff,syn::Resonance,syn::FilterAmount,syn::Attack,syn::Decay,syn::Sustain,syn::Release,syn::Volume},
        {syn::Wave1,syn::Reduce1,syn::Alt1,syn::Noise1,syn::SquishRate1,syn::SquishDepth1,syn::Octave,-1},
        {syn::Wave2,syn::Reduce2,syn::Alt2,syn::Noise2,syn::SquishRate2,syn::SquishDepth2,syn::Level2,syn::Tune2},
        {syn::Glide,syn::Velocity,syn::VibratoRate,syn::AftertouchTarget,syn::AftertouchDepth,-1,-1,-1}};
    *out={}; out->page_id=i; std::strcpy(out->section_name,"Shibboleth"); std::strcpy(out->page_name,names[i]);
    for(int j=0;j<8;++j) { out->param_ids[j]=ids[i][j]<0?CLAP_INVALID_ID:firstId+ids[i][j]; }
    return true;
}
const clap_plugin_remote_controls_t remotes={pageCount,page};
bool CLAP_ABI guiSupported(const clap_plugin_t*,const char* api,bool floating){return api && !floating && std::strcmp(api,CLAP_WINDOW_API_WIN32)==0;}
bool CLAP_ABI guiPreferred(const clap_plugin_t*,const char** api,bool* floating){*api=CLAP_WINDOW_API_WIN32;*floating=false;return true;}
bool CLAP_ABI guiCreate(const clap_plugin_t* p,const char* api,bool floating){
    auto& s=self(p);if(s.editor || !guiSupported(p,api,floating))return false;
    ui::Hooks hooks{&s,
        [](void* ctx,int id){return static_cast<Plugin*>(ctx)->base[id].load();},
        [](void* ctx,int id){return static_cast<Plugin*>(ctx)->beginEdit(id);},
        [](void* ctx,int id,double v){static_cast<Plugin*>(ctx)->edit(id,v);},
        [](void* ctx,int id){static_cast<Plugin*>(ctx)->endEdit(id);},
        [](void* ctx,int id,double v,char* text,size_t size){paramText(&static_cast<Plugin*>(ctx)->api,firstId+id,v,text,uint32_t(size));},
        [](void* ctx,int osc,float* data,size_t size){auto& plugin=*static_cast<Plugin*>(ctx);for(size_t i=0;i<size && i<128;++i)data[i]=plugin.scopeData[osc*128+i].load(std::memory_order_relaxed);}};
    auto editor=std::make_unique<ui::Editor>(hooks);if(!editor->create())return false;
    s.editor=std::move(editor);return true;
}
void CLAP_ABI guiDestroy(const clap_plugin_t* p){self(p).scopesEnabled.store(false);self(p).editor.reset();}
bool CLAP_ABI guiSize(const clap_plugin_t* p,uint32_t* w,uint32_t* h){if(!self(p).editor)return false;self(p).editor->dimensions(w,h);return true;}
bool CLAP_ABI guiResize(const clap_plugin_t* p,uint32_t w,uint32_t h){if(!self(p).editor)return false;uint32_t currentW,currentH;self(p).editor->dimensions(&currentW,&currentH);return w==currentW && h==currentH;}
// The approved size is already in physical Windows pixels. Host scale hints
// are deliberately declined, so initial creation and recreation agree even
// when a host sends its DPI hint only on the first opening.
bool CLAP_ABI guiScale(const clap_plugin_t*,double){return false;}
bool CLAP_ABI guiCanResize(const clap_plugin_t*){return false;}
bool CLAP_ABI guiHints(const clap_plugin_t*,clap_gui_resize_hints_t*){return false;}
bool CLAP_ABI guiAdjust(const clap_plugin_t*,uint32_t*,uint32_t*){return false;}
bool CLAP_ABI guiParent(const clap_plugin_t* p,const clap_window_t* window){return window && window->api && !std::strcmp(window->api,CLAP_WINDOW_API_WIN32) && self(p).editor && self(p).editor->parent(static_cast<HWND>(window->win32));}
bool CLAP_ABI guiTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void CLAP_ABI guiTitle(const clap_plugin_t*,const char*){}
bool CLAP_ABI guiShow(const clap_plugin_t* p){auto& s=self(p);if(!s.editor)return false;s.scopesEnabled.store(true);return s.editor->show(true);}
bool CLAP_ABI guiHide(const clap_plugin_t* p){auto& s=self(p);if(!s.editor)return false;s.scopesEnabled.store(false);return s.editor->show(false);}
const clap_plugin_gui_t gui={guiSupported,guiPreferred,guiCreate,guiDestroy,guiScale,guiSize,guiCanResize,guiHints,guiAdjust,guiResize,guiParent,guiTransient,guiTitle,guiShow,guiHide};
const void* CLAP_ABI extension(const clap_plugin_t*,const char* id) {
    if(!std::strcmp(id,CLAP_EXT_GUI))return &gui;
    if(!std::strcmp(id,CLAP_EXT_PARAMS)) return &params;
    if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS)) return &audioPorts;
    if(!std::strcmp(id,CLAP_EXT_NOTE_PORTS)) return &notePorts;
    if(!std::strcmp(id,CLAP_EXT_STATE)) return &state;
    if(!std::strcmp(id,CLAP_EXT_LATENCY)) return &latencyExtension;
    if(!std::strcmp(id,CLAP_EXT_REMOTE_CONTROLS)) return &remotes;
    return nullptr;
}
uint32_t CLAP_ABI count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* CLAP_ABI describe(const clap_plugin_factory_t*,uint32_t i) { return i==0?&descriptor:nullptr; }
const clap_plugin_t* CLAP_ABI create(const clap_plugin_factory_t*,const clap_host_t* host,const char* id) {
    if(!host || !clap_version_is_compatible(host->clap_version) || std::strcmp(id,descriptor.id)) return nullptr;
    auto s=new(std::nothrow) Plugin(host); if(!s) return nullptr;
    s->api={&descriptor,s,init,destroy,activate,noop,start,noop,reset,process,extension,noop}; return &s->api;
}
const clap_plugin_factory_t factory={count,describe,create};
bool CLAP_ABI entryInit(const char*) { return true; }
void CLAP_ABI entryDeinit() {}
const void* CLAP_ABI getFactory(const char* id) { return std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?nullptr:&factory; }
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry={CLAP_VERSION_INIT,entryInit,entryDeinit,getFactory};
