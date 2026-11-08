#define NOMINMAX
#include <windows.h>
#undef near
#include <clap/clap.h>
#include "../src/parameters.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace imperius;
void check(bool yes,const char* message) { if(!yes) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char* message) { check(std::abs(a-b)<=tolerance,message); }
constexpr clap_id firstId=2000;
const void* CLAP_ABI hostExtension(const clap_host_t*,const char*) { return nullptr; }
void CLAP_ABI request(const clap_host_t*) {}
struct Events {
    struct alignas(8) Entry { std::array<unsigned char,128> data{}; };
    std::vector<Entry> entries;
    clap_input_events_t api{this,
        [](const clap_input_events_t* p)->uint32_t { return uint32_t(static_cast<Events*>(p->ctx)->entries.size()); },
        [](const clap_input_events_t* p,uint32_t i)->const clap_event_header_t* {
            return reinterpret_cast<const clap_event_header_t*>(static_cast<Events*>(p->ctx)->entries.at(i).data.data()); }};
    template<class T> void add(const T& e) { static_assert(sizeof(T)<=128);Entry entry;std::memcpy(entry.data.data(),&e,sizeof e);entries.push_back(entry); }
    void midi(uint32_t time,uint8_t status,uint8_t a,uint8_t b=0,uint16_t port=0) {
        clap_event_midi_t e{};e.header={sizeof e,time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_MIDI,0};
        e.port_index=port;e.data[0]=status;e.data[1]=a;e.data[2]=b;add(e);
    }
    void value(uint32_t time,int id,double value,int channel=-1) {
        clap_event_param_value_t e{};e.header={sizeof e,time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};
        e.param_id=firstId+id;e.note_id=e.port_index=e.key=-1;e.channel=int16_t(channel);e.value=value;add(e);
    }
    void mod(uint32_t time,int id,double amount) {
        clap_event_param_mod_t e{};e.header={sizeof e,time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_MOD,0};
        e.param_id=firstId+id;e.note_id=e.port_index=e.key=e.channel=-1;e.amount=amount;add(e);
    }
};
struct Memory {
    std::vector<char> bytes;size_t cursor=0;
    clap_ostream_t out{this,[](const clap_ostream_t* s,const void* data,uint64_t size)->int64_t {
        auto& m=*static_cast<Memory*>(s->ctx);size_t n=size_t(std::min<uint64_t>(size,7));auto ptr=static_cast<const char*>(data);
        m.bytes.insert(m.bytes.end(),ptr,ptr+n);return n; }};
    clap_istream_t in{this,[](const clap_istream_t* s,void* data,uint64_t size)->int64_t {
        auto& m=*static_cast<Memory*>(s->ctx);size_t n=std::min({size_t(size),size_t(5),m.bytes.size()-m.cursor});
        std::memcpy(data,m.bytes.data()+m.cursor,n);m.cursor+=n;return n; }};
};
struct Audio {
    std::array<float,512> left{},right{};
    float* channels[2]{left.data(),right.data()};
    clap_audio_buffer_t output{};
    clap_process_t block{};
    Audio() { output.data32=channels;output.channel_count=2;block.audio_outputs=&output;block.audio_outputs_count=1;block.frames_count=512; }
    void run(const clap_plugin_t* p,Events* events=nullptr) {
        block.in_events=events?&events->api:nullptr;
        check(p->process(p,&block)==CLAP_PROCESS_CONTINUE,"Process returns continue");
        for(uint32_t n=0;n<block.frames_count;++n) {
            check(std::isfinite(left[n]) && std::abs(left[n])<=.84,"Finite peak-bounded plugin output");
            near(left[n],right[n],0,"Dual-mono outputs match");
        }
    }
    double energy() const { double sum=0;for(uint32_t n=0;n<block.frames_count;++n) sum+=left[n]*left[n];return sum; }
};
int main() {
    try {
        auto dll=LoadLibraryW(L"build/Imperius.clap");check(dll!=nullptr,"Load self-contained Imperius DLL");
        auto entry=reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(dll,"clap_entry"));check(entry,"Entry export");
        check(entry->init("build/Imperius.clap"),"Entry init");
        auto factory=static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        check(factory && factory->get_plugin_count(factory)==1,"Discovery");
        auto descriptor=factory->get_plugin_descriptor(factory,0);
        check(descriptor && !std::strcmp(descriptor->id,"local.abdicant.imperius"),"Independent Imperius identity");
        check(!factory->get_plugin_descriptor(factory,1),"Invalid descriptor index");
        clap_host_t host{CLAP_VERSION_INIT,nullptr,"Imperius test host","Local","","1",hostExtension,request,request,request};
        check(!factory->create_plugin(factory,&host,"local.abdicant.mono.prototype"),"Reject Shiboleth identity");
        auto p=factory->create_plugin(factory,&host,descriptor->id);check(p && p->init(p),"Instantiate");
        auto q=factory->create_plugin(factory,&host,descriptor->id);check(q && q->init(q),"Second instance");
        auto params=static_cast<const clap_plugin_params_t*>(p->get_extension(p,CLAP_EXT_PARAMS));
        auto state=static_cast<const clap_plugin_state_t*>(p->get_extension(p,CLAP_EXT_STATE));
        auto remotes=static_cast<const clap_plugin_remote_controls_t*>(p->get_extension(p,CLAP_EXT_REMOTE_CONTROLS));
        auto latency=static_cast<const clap_plugin_latency_t*>(p->get_extension(p,CLAP_EXT_LATENCY));
        auto ports=static_cast<const clap_plugin_note_ports_t*>(p->get_extension(p,CLAP_EXT_NOTE_PORTS));
        check(params && state && remotes && latency && ports,"Required extensions");
        check(params->count(p)==Count && latency->get(p)==16,"Parameter count and latency");
        clap_note_port_info_t notePort{};check(ports->get(p,0,true,&notePort) && notePort.preferred_dialect==CLAP_NOTE_DIALECT_MIDI,"MIDI input dialect");
        for(int i=0;i<Count;++i) {
            clap_param_info_t info{};check(params->get_info(p,i,&info),"Parameter metadata");
            double value=0;check(params->get_value(p,info.id,&value),"Parameter readback");near(value,definitions[i].initial,0,"Initial values");
            char text[128];check(params->value_to_text(p,info.id,value,text,sizeof text) && text[0],"Readable parameter text");
        }
        check(remotes->count(p)==6,"Six remote pages");
        for(uint32_t i=0;i<6;++i) {
            clap_remote_controls_page_t page{};check(remotes->get(p,i,&page),"Remote page");
            for(auto id:page.param_ids)check(id>=firstId && id<firstId+Count,"Every remote refers to a real parameter");
        }
        Events set;set.value(0,Fold,.72);set.value(0,Bias,-.3);set.value(0,Octave,-1);
        params->flush(p,&set.api,nullptr);
        Memory saved;check(state->save(p,&saved.out),"Save using partial writes");
        Events change;change.value(0,Fold,.1);params->flush(p,&change.api,nullptr);
        check(state->load(p,&saved.in),"Restore using partial reads");
        // Version 1 stored exactly the original 34 parameters, before the two
        // appended wave controls. It must recall with the original sine sources.
        Memory legacy;legacy.bytes=saved.bytes;std::memcpy(legacy.bytes.data(),"IMPR0001",8);
        legacy.bytes.resize(8+sizeof(double)*ComplexShape);
        Events waves;waves.value(0,ComplexShape,-.8);waves.value(0,BasicWave,2);params->flush(p,&waves.api,nullptr);
        Memory waveState;check(state->save(p,&waveState.out),"Save wave controls");
        check(state->load(p,&legacy.in),"Load version 1 state");
        double shape=0;params->get_value(p,firstId+ComplexShape,&shape);near(shape,0,0,"Legacy complex wave defaults to sine");
        params->get_value(p,firstId+BasicWave,&shape);near(shape,1,0,"Legacy basic wave defaults to sine");
        check(state->load(p,&waveState.in),"Load new wave state");
        params->get_value(p,firstId+ComplexShape,&shape);near(shape,-.8,0,"Complex shape survives recall");
        params->get_value(p,firstId+BasicWave,&shape);near(shape,2,0,"Basic wave survives recall");
        saved.cursor=0;check(state->load(p,&saved.in),"Restore test baseline");
        double value=0;params->get_value(p,firstId+Fold,&value);near(value,.72,0,"Restore fold");
        params->get_value(q,firstId+Fold,&value);near(value,.15,0,"Instance isolation");
        Memory bad;bad.bytes=saved.bytes;bad.bytes.resize(30);
        check(!state->load(p,&bad.in),"Reject truncated state atomically");
        params->get_value(p,firstId+Fold,&value);near(value,.72,0,"Failed load preserves existing state");
        Memory nonfinite;nonfinite.bytes=saved.bytes;double nan=std::nan("");
        std::memcpy(nonfinite.bytes.data()+8+sizeof(double)*Fold,&nan,sizeof nan);
        check(!state->load(p,&nonfinite.in),"Reject nonfinite state");
        Events invalid;invalid.value(0,Fold,std::nan(""));invalid.value(0,Fold,.4,1);
        params->flush(p,&invalid.api,nullptr);params->get_value(p,firstId+Fold,&value);near(value,.72,0,"Reject nonfinite/per-note parameter events");
        check(!p->activate(p,384000,1,512),"Reject unsupported rate");
        check(p->activate(p,48000,1,512) && q->activate(q,48000,1,512),"Activate");
        check(p->start_processing(p) && q->start_processing(q),"Start");
        Audio audio;Events notes;
        notes.midi(64,0x90,60,100);audio.run(p,&notes);
        for(int n=0;n<64;++n)near(audio.left[n],0,0,"Sample-accurate note onset");
        check(audio.energy()>1e-4,"MIDI keyboard produces audio");
        Audio other;other.run(q);near(other.energy(),0,0,"Idle instance remains silent");
        Events chord;for(int key:{48,55,60,63,70,74})chord.midi(0,0x90,uint8_t(key),110);
        audio.run(p,&chord);check(audio.energy()>1e-4,"Six-note input is handled by five-voice stealing");
        Events off;off.midi(0,0xb0,120);audio.run(p,&off);
        for(int n=32;n<512;++n)near(audio.left[n],0,0,"CC120 kills channel after FIR tail");
        p->reset(p);notes.entries.clear();notes.midi(0,0x90,60,100);audio.run(p,&notes);
        Events down;down.midi(0,0xb0,64,127);down.midi(0,0x80,60);audio.run(p,&down);
        for(int n=0;n<70;++n)audio.run(p);
        check(audio.energy()>1e-4,"Pedal sustains released note");
        Events up;up.midi(0,0xb0,64,0);audio.run(p,&up);
        for(int n=0;n<70;++n)audio.run(p);
        near(audio.energy(),0,1e-15,"Pedal up releases voice to silence");
        // Identical event positions must produce identical samples across block sizes.
        p->reset(p);q->reset(q);
        saved.cursor=0;check(state->load(q,&saved.in),"Load same state into second instance");
        notes.entries.clear();notes.midi(0,0x90,60,100);notes.value(256,Fold,.1);
        notes.mod(256,Harmonics,.3);audio.run(p,&notes);
        Audio split;split.block.frames_count=256;
        Events first;first.midi(0,0x90,60,100);split.run(q,&first);
        for(int n=0;n<256;++n)near(audio.left[n],split.left[n],1e-7,"Block-independent note rendering");
        Events second;second.value(0,Fold,.1);second.mod(0,Harmonics,.3);split.run(q,&second);
        for(int n=0;n<256;++n)near(audio.left[n+256],split.left[n],1e-7,"Sample-timed smoothed automation and modulation");
        // A fresh activation removes temporary modulation; state only saves base values.
        Memory modState;check(state->save(p,&modState.out),"Save with temporary modulation");
        params->get_value(p,firstId+Harmonics,&value);near(value,.2,0,"Modulation preserves base value");
        // Check 64-bit audio path against identical 32-bit instance output.
        p->reset(p);q->reset(q);
        Memory current;check(state->save(p,&current.out) && state->load(q,&current.in),"Match float/double state");
        std::array<double,512> dl{},dr{};double* dchannels[]{dl.data(),dr.data()};
        clap_audio_buffer_t dout{};dout.data64=dchannels;dout.channel_count=2;
        clap_process_t db{};db.audio_outputs=&dout;db.audio_outputs_count=1;db.frames_count=512;
        Events start;start.midi(0,0x90,60,100);db.in_events=&start.api;
        check(p->process(p,&db)==CLAP_PROCESS_CONTINUE,"Float64 processing");other.run(q,&start);
        for(int n=0;n<512;++n) { near(dl[n],dr[n],0,"Float64 channel agreement");near(dl[n],other.left[n],1e-7,"Float32/64 agreement"); }
        // Host tempo controls the same LFO rate parameter when sync is on.
        p->reset(p);q->reset(q);Events sync;
        sync.value(0,Lfo1Sync,1);sync.value(0,Lfo1Rate,5./9);sync.value(0,HarmonicLfo,.9);sync.midi(0,0x90,60,100);
        clap_event_transport_t tempo{};tempo.flags=CLAP_TRANSPORT_HAS_TEMPO;tempo.tempo=90;
        audio.block.transport=&tempo;audio.run(p,&sync);
        clap_event_transport_t faster=tempo;faster.tempo=150;other.block.transport=&faster;other.run(q,&sync);
        double difference=0;
        for(int block=0;block<40;++block) {
            audio.run(p);other.run(q);
            for(int n=0;n<512;++n)difference+=std::abs(audio.left[n]-other.left[n]);
        }
        check(difference>1,"Host tempo changes audible synced modulation");
        audio.block.transport=nullptr;other.block.transport=nullptr;
        // Listen through the actual binary to both new host controls, with
        // equal initial states and identical note events.
        for(int wave:{0,2}) {
            saved.cursor=0;check(state->load(p,&saved.in),"Shape test initial state");
            saved.cursor=0;check(state->load(q,&saved.in),"Shape test reference state");
            p->reset(p);q->reset(q);
            Events shaped;shaped.value(0,ComplexShape,wave==0?-1:1);shaped.value(0,BasicWave,wave);shaped.midi(0,0x90,60,100);
            audio.run(p,&shaped);other.run(q,&start);double delta=0;
            for(int block=0;block<10;++block) {
                audio.run(p);other.run(q);
                for(int n=0;n<512;++n)delta+=std::abs(audio.left[n]-other.left[n]);
            }
            check(delta>1,"New wave controls affect compiled plugin audio");
        }
        // Benchmark the actual DLL adapter including smoothing and parameter mapping.
        p->reset(p);Events bench;bench.value(0,Fold,.8);bench.value(0,PhaseMod,.4);bench.value(0,FilterEnv,.6);
        for(int key:{48,55,60,63,70})bench.midi(0,0x90,uint8_t(key),127);
        audio.run(p,&bench);
        const auto begin=std::chrono::steady_clock::now();
        constexpr int blocks=282;
        for(int n=0;n<blocks;++n)audio.run(p);
        double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
        double percent=elapsed/(blocks*512./48000)*100;
        std::ofstream report("build/plugin-benchmark.txt");report<<"48 kHz, 5 voices, 512-frame blocks, adapter + audio validation: "<<percent<<"% of one core\n";
        std::cout<<"CLAP checks passed: discovery, parameters, state, timing, automation, modulation, sustain, stealing, float32/64, tempo, instance isolation.\n";
        std::cout<<"Actual DLL offline benchmark at 48 kHz, five voices: "<<percent<<"% of one core\n";
        p->stop_processing(p);q->stop_processing(q);p->deactivate(p);q->deactivate(q);p->destroy(p);q->destroy(q);
        entry->deinit();FreeLibrary(dll);return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
