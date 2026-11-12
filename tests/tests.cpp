#define NOMINMAX
#include <windows.h>
#include "../src/synth.hpp"
#include <clap/clap.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void check(bool condition,const char* why) { if(!condition) throw std::runtime_error(why); }
void checkNear(double a,double b,double error,const char* why) { check(std::abs(a-b)<=error,why); }
void filterTests() {
    // A driven low-cutoff Wasp used to latch around -0.5 even after input stopped.
    // Test the internal branch as well as audible output: the limiter can hide DC.
    for(double fs:{32000.,176400.,192000.,384000.})for(double cutoff:{20.,60.,120.,500.})for(double res:{0.,.5,.95}) {
        syn::CharacterFilters f;
        const double g=std::tan(syn::pi*cutoff/fs),pole=std::exp(-2*syn::pi*5/fs);
        for(int n=0;n<int(fs);++n)f.wasp(.7*syn::wave(n*110./fs,1),g,res,pole);
        for(int n=0;n<int(fs*2);++n)f.wasp(0,g,res,pole);
        check(std::abs(f.wasp1)<1e-6 && std::abs(f.wasp2)<1e-6,"Wasp state returns to zero after driven low-cutoff input");
        check(std::abs(f.waspDcOutput)<1e-6,"Wasp output returns to silence without a DC latch");
    }
    // Sweep through the reported range in both directions under continuous drive.
    for(double res:{.5,.95}) {
        syn::CharacterFilters f;constexpr double fs=48000;
        double mean=0,energy=0;int count=0;
        for(int n=0;n<int(fs*4);++n) {
            double t=n/(fs*4),cutoff=20*std::pow(100.,std::abs(2*t-1));
            double y=f.wasp(.7*syn::wave(n*110./fs,1),std::tan(syn::pi*cutoff/fs),res,std::exp(-2*syn::pi*5/fs));
            if(n>=int(fs*3.5)){mean+=y;energy+=y*y;++count;}
        }
        check(std::abs(mean/count)<.02,"Wasp sweep does not accumulate output DC");
        check(std::sqrt(energy/count)>.1,"Wasp recovers audible signal after low-cutoff sweep");
    }
    // Small-signal frequency response: same cutoff, different slopes.
    const auto response=[](int mode,double frequency) {
        syn::CharacterFilters f;double a=0,b=0,energy=0;
        const double fs=192000,g=std::tan(syn::pi*1000/fs);
        for(int n=0;n<40000;++n) {
            const double input=.001*std::sin(2*syn::pi*frequency*n/fs);
            const double v1=(a+g*(input-b))/(1+g*(g+2)),v2=b+g*v1;
            a=2*v1-a;b=2*v2-b;
            const double y=f.next(input,v2,g,0,mode,.01);
            if(n>=20000)energy+=y*y;
        }
        return std::sqrt(energy/20000);
    };
    for(int mode=0;mode<4;++mode) {
        const double low=response(mode,200),high=response(mode,8000);
        check(low>0.0001,"Each algorithm passes bass");
        check(high<low*(mode<2?.04:.002),"Each algorithm has expected low-pass slope");
    }
    std::array<std::vector<double>,4> audio;
    for(int mode=0;mode<4;++mode) {
        syn::Engine e;e.values[syn::FilterAlgorithm]=mode;e.values[syn::Resonance]=3;e.noteOn(42,1);
        for(int n=0;n<6000;++n)audio[mode].push_back(e.next());
    }
    for(int a=0;a<4;++a)for(int b=a+1;b<4;++b) {
        double difference=0;for(int n=1000;n<6000;++n)difference+=std::abs(audio[a][n]-audio[b][n]);
        check(difference>1,"Filter modes produce different audio");
    }
    for(double fs:{8000.,44100.,48000.,96000.,384000.}) {
        syn::Engine e;e.reset(fs);e.noteOn(100,1);e.values[syn::Resonance]=3;
        for(int n=0;n<12000;++n) {
            e.values[syn::FilterAlgorithm]=(n/101)%4;e.values[syn::Cutoff]=(n%1000)/999.;
            const double y=e.next();check(std::isfinite(y)&&std::abs(y)<=syn::ceiling,"Filter switching and sweeps remain bounded");
            for(double state:e.filters.ladder)check(std::isfinite(state)&&std::abs(state)<10,"Ladder internal stability");
            for(double state:e.filters.ota)check(std::isfinite(state)&&std::abs(state)<10,"OTA internal stability");
        }
    }
    syn::CharacterFilters fade;double previous=0;
    for(int n=0;n<3000;++n) {
        double y=fade.next(0,1,.1,0,n<1000?0:1,.001);
        if(n>0)check(std::abs(y-previous)<.002,"Algorithm change crossfades without a step");
        previous=y;
    }
}
void coreTests() {
    for(int octave=-2;octave<=1;++octave){syn::Engine shifted,reference;
        shifted.values[syn::Octave]=octave;shifted.values[syn::Tune2]=reference.values[syn::Tune2]=.7;
        shifted.noteOn(60,1);reference.noteOn(60+12*octave,1);
        for(int i=0;i<3000;++i)checkNear(shifted.next(),reference.next(),1e-12,"Octave transposes both oscillators while retaining relative tune");}
    checkNear(syn::ampSustain(0),0,0,"Amp sustain closes completely");
    double previous=0;
    for(int i=0;i<=6000;++i) {
        double sus=i/6000.0,amp=syn::ampSustain(sus);
        check(amp>=previous,"Amp sustain taper is monotonic");previous=amp;
        if(i>=1000)checkNear(amp,(1+sus)*.5,1e-12,"Upper five sixths sustain unchanged");
    }
    checkNear(syn::ampSustain(1./6-1e-7),7./12,1e-6,"Sustain join continuous");
    for(int route=0;route<6;++route) for(double sign:{-1.,1.}) {
        auto base=syn::defaults();for(int target:syn::modTargets)base[target]=.5;
        base[syn::CrushEnv1+route]=sign;
        const auto result=syn::envelopeMods(base,.25);
        for(int j=0;j<6;++j)checkNear(result[syn::modTargets[j]],.5+(j==route?sign*.25:0),1e-12,"Independent bipolar envelope route");
        checkNear(syn::envelopeMods(base,1)[syn::modTargets[route]],sign>0?1:0,0,"Modulation clips at slider limits");
        check(syn::envelopeMods(base,0)==base,"Zero envelope preserves base values");
        syn::Engine plain,mod;plain.values=mod.values=base;plain.values[syn::CrushEnv1+route]=0;
        plain.noteOn(48,1);mod.noteOn(48,1);double difference=0;
        for(int n=0;n<6000;++n)difference+=std::abs(plain.next()-mod.next());
        check(difference>.01,"Every envelope route changes actual rendered audio");
    }
    {syn::Engine silent;silent.values[syn::Sustain]=0;silent.noteOn(48,1);
     for(int n=0;n<48000;++n)silent.next();
     checkNear(silent.ampEnvelope.value,0,0,"Held zero sustain reaches zero after twice decay");
     checkNear(silent.next(),0,1e-10,"Held zero sustain is silent");}

    // Incommensurate sample/pitch ratios move each cycle crossing between
    // samples. A discontinuity must not produce a varying interpolated start.
    for(double fs:{176400.,192000.,384000.})for(double hz:{55.123,440.31,3500.7}){
        syn::ScopeTrace trace;double phase=.137;int completed=0;
        for(int i=0;i<int(fs*.2);++i){auto before=trace.samples;double old=phase;
            phase+=hz/fs;phase-=std::floor(phase);trace.push(phase,phase<.5?1:-1);
            if(phase<old && ++completed>2){checkNear(trace.samples[0],1,0,"Square scope start remains stable across fractional crossings");checkNear(trace.samples[127],-1,0,"Scope captures end of cycle");}
            else if(phase>=old)check(trace.samples==before,"Displayed scope changes only on completed cycles");
        }
        for(int i=0;i<int(fs/hz*3);++i){phase+=hz/fs;phase-=std::floor(phase);trace.push(phase,phase<.5?.3:-.7);}
        checkNear(trace.samples[0],.3,1e-6,"Scope remains live when oscillator amplitude changes");
    }
    { syn::Engine scoped; scoped.captureScopes=true; scoped.noteOn(48,1);
      for(int i=0;i<6000;++i) scoped.next();
      const auto original=scoped.scopes[0].samples;
      double range=*std::max_element(original.begin(),original.end())-*std::min_element(original.begin(),original.end());
      check(range>1.5,"Live scope captures a full oscillator cycle");
      scoped.values[syn::Alt1]=0.6; scoped.values[syn::SquishDepth2]=1;
      for(int i=0;i<12000;++i) scoped.next();
      double change=0;for(size_t i=0;i<original.size();++i){change+=std::abs(scoped.scopes[0].samples[i]-original[i]);check(std::isfinite(scoped.scopes[1].samples[i]),"Scope samples remain finite");}
      check(change>10,"Scope reflects post-treatment waveform changes"); }
    checkNear(syn::tuning(0),0,0,"Tune center"); checkNear(syn::tuning(0.25),0.5,1e-12,"Fine cents");
    checkNear(syn::tuning(0.5),1,0,"Half-step boundary"); checkNear(syn::tuning(0.5001),2,0,"First snapped step");
    checkNear(syn::tuning(0.999),12,0,"Octave before endpoint"); checkNear(syn::tuning(1),24,0,"Two-octave endpoint");
    for(int i=0;i<=1000;++i) checkNear(syn::tuning(-i/1000.0),-syn::tuning(i/1000.0),1e-12,"Tune symmetry");
    for(int offset:{int(syn::Wave1),int(syn::Wave2)}) for(int shape=0;shape<7;++shape) {
        for(double amount:{0.,.7,1.}) {
            syn::Oscillator osc;auto p=syn::defaults();p[offset]=shape;p[offset+1]=amount;
            p[offset+2]=.6;p[offset+5]=1;osc.phase=osc.hiddenPhase=.9;osc.lfo=.25;
            checkNear(osc.next(220,192000,p,offset),(shape==1 || shape==3)?0:-1,0,"Squish tail per waveform, including hidden pulse and Crush");
        }
        syn::Oscillator osc;auto p=syn::defaults();p[offset]=shape;osc.phase=.37;
        checkNear(osc.next(220,192000,p,offset),(offset==syn::Wave2 && shape==1 ? -.41+.12*2.12 : syn::wave(.37,shape)),1e-12,"Squish bypass preserves original waveform");
    }
    {syn::Oscillator osc;auto p=syn::defaults();p[syn::Wave1]=2;p[syn::Reduce1]=1;p[syn::SquishDepth1]=1;
     osc.phase=.02;osc.hiddenPhase=.9;osc.lfo=.25;
     checkNear(osc.next(220,192000,p,0),0,0,"Hidden pulse has its own negative Squish tail before mixing");}
    checkNear(syn::crush(0.325,0),0.325,0,"Crush bypass");
    checkNear(syn::crush(0.325,1),0,0,"Maximum crush center level");
    checkNear(syn::crush(0.8,1),1,0,"Maximum crush positive level");
    checkNear(syn::crush(-0.8,1),-1,0,"Maximum crush negative level");
    checkNear(syn::crush(0,0.73),0,0,"Crushing preserves squish silence");
    { syn::Oscillator oscillator; auto p=syn::defaults(); p[syn::Wave1]=2; p[syn::Noise1]=1; p[syn::NoiseCutoff]=150;
      double oldEnergy=0,newEnergy=0;
      for(int i=0;i<192000;++i) { double gain=std::abs(oscillator.next(0,192000,p,0));
          if(i>2000) { double oldGain=std::clamp(0.5+oscillator.noise*2.5,0.0,1.0); oldEnergy+=(oldGain-.5)*(oldGain-.5); newEnergy+=(gain-.5)*(gain-.5); } }
      const double ratio=std::sqrt(newEnergy/oldEnergy);
      check(ratio>1.95 && ratio<=2.001,"Noise AM excursion doubles at reference bandwidth"); }
    { syn::Oscillator slow,fast; auto low=syn::defaults(),high=low;
      low[syn::NoiseCutoff]=20; high[syn::NoiseCutoff]=800; low[syn::Wave1]=high[syn::Wave1]=2; low[syn::Noise1]=high[syn::Noise1]=1;
      double previousSlow=.5,previousFast=.5,slowMovement=0,fastMovement=0;
      for(int i=0;i<48000;++i) { double a=std::abs(slow.next(0,48000,low,0)),b=std::abs(fast.next(0,48000,high,0));
          if(i>2000) { slowMovement+=(a-previousSlow)*(a-previousSlow); fastMovement+=(b-previousFast)*(b-previousFast); } previousSlow=a;previousFast=b; }
      check(slowMovement<fastMovement*0.1,"Shared noise cutoff changes modulation smoothness"); }
    checkNear(syn::resonance(0),0,0,"Resonance 0"); checkNear(syn::resonance(1),0.1,0,"Resonance 1");
    checkNear(syn::resonance(2),0.5,0,"Resonance 2"); checkNear(syn::resonance(3),0.95,0,"Resonance 3");
    syn::Envelope filter,amp; filter.on(); amp.on();
    for(int i=0;i<12000;++i) { filter.next(1.0/48000,0.01,0.1,0.2,0.1); amp.next(1.0/48000,0.01,0.2,0.6,0.1);
        if(i==6000) { check(filter.stage==syn::Envelope::SustainStage,"Filter decay time"); check(amp.stage==syn::Envelope::DecayStage,"Amp decay must be twice as long"); } }
    checkNear(filter.value,0.2,1e-12,"Filter sustain"); checkNear(amp.value,0.6,1e-12,"Amp sustain mapping");
    filter.off(); amp.off(); for(int i=0;i<5000;++i) { filter.next(1.0/48000,0.01,0.1,0.2,0.1); amp.next(1.0/48000,0.01,0.2,0.6,0.1); }
    checkNear(amp.value,0,0,"Amp silence after release");
    // Note events preserve every free-running source and both legato envelopes.
    for(double fs:{44100.,48000.,96000.}) {
        syn::Engine running;running.reset(fs);running.values[syn::Reduce1]=.8;running.values[syn::Reduce2]=.6;
        running.noteOn(48,1);for(int i=0;i<2000;++i)running.next();
        auto before=running;running.noteOn(72,.1);
        auto sameSources=[](const syn::Engine& a,const syn::Engine& b){
            return a.osc1.phase==b.osc1.phase && a.osc2.phase==b.osc2.phase &&
                a.osc1.hiddenPhase==b.osc1.hiddenPhase && a.osc2.hiddenPhase==b.osc2.hiddenPhase &&
                a.osc1.lfo==b.osc1.lfo && a.osc2.lfo==b.osc2.lfo && a.vibratoPhase==b.vibratoPhase &&
                a.osc1.noise==b.osc1.noise && a.osc2.noise==b.osc2.noise &&
                a.osc1.drift==b.osc1.drift && a.osc2.drift==b.osc2.drift;
        };
        check(sameSources(before,running),"Legato note never resets phase/noise/drift");
        checkNear(running.smoothVelocity,1,0,"Legato velocity has no immediate jump");
        checkNear(running.filterEnvelope.progress,before.filterEnvelope.progress,0,"Legato filter envelope not restarted");
        running.next();check(running.smoothVelocity>.99 && running.smoothVelocity<1,"First velocity change is bounded");
        for(int i=1;i<int(fs*.004);++i)running.next();
        checkNear(running.smoothVelocity,.1+.9*std::exp(-int(fs*.004)/(.004*fs)),1e-10,"Velocity smoothing is sample-rate independent");
        before=running;running.noteOff(72);check(sameSources(before,running),"Note fallback preserves running sources");
        checkNear(running.smoothVelocity,before.smoothVelocity,0,"Fallback velocity has no jump");
        running.noteOff(48);running.next();before=running;running.noteOn(60,.5);
        check(sameSources(before,running),"Detached note also preserves oscillator and LFO phases");
        checkNear(running.smoothVelocity,before.smoothVelocity,0,"Retrigger over release tail preserves gain");
    }
    {syn::Oscillator osc;auto p=syn::defaults();p[syn::Wave1]=2;p[syn::Reduce1]=1;
     for(int i=0;i<1000;++i)osc.next(220,192000,p,0);
     double phase=osc.hiddenPhase;p[syn::Reduce1]=0;osc.next(220,192000,p,0);
     double advance=osc.hiddenPhase-phase;if(advance<0)advance+=1;
     check(advance>.001 && advance<.002,"Hidden pulse oscillator runs continuously when Crush is bypassed");}
    // On a steady pulse, isolate velocity from pitch/phase changes and check
    // the first millisecond against an otherwise identical continuing voice.
    {syn::Engine changed;changed.values[syn::Wave1]=2;changed.values[syn::Level2]=0;
     changed.values[syn::Velocity]=1;changed.values[syn::FilterAmount]=0;changed.values[syn::Volume]=.3;
     changed.values[syn::Sustain]=1;changed.noteOn(36,1);for(int i=0;i<24000;++i)changed.next();
     auto reference=changed;changed.noteOn(36,.1);double error=0;
     for(int i=0;i<48;++i)error=std::max(error,std::abs(changed.next()-reference.next()));
     check(error<.06,"Legato velocity no longer produces an abrupt audio gain step");}
    // Port's exact zero endpoint selects per-note retrigger, independently of
    // parameter smoothing and whether a tiny glide rounds down to zero samples.
    for(double port:{0.,1e-9,.025}) for(int key:{60,72}) {
        syn::Engine retrigger;retrigger.noteOn(60,1);
        for(int i=0;i<2000;++i)retrigger.next();
        retrigger.values[syn::Glide]=port;
        const auto before=retrigger;retrigger.noteOn(key,.7);
        checkNear(retrigger.osc1.phase,before.osc1.phase,0,"Retrigger preserves oscillator phase");
        checkNear(retrigger.osc2.lfo,before.osc2.lfo,0,"Retrigger preserves LFO phase");
        checkNear(retrigger.smoothVelocity,before.smoothVelocity,0,"Retrigger preserves velocity continuity");
        if(port==0){
            check(retrigger.filterEnvelope.stage==syn::Envelope::AttackStage && retrigger.ampEnvelope.stage==syn::Envelope::AttackStage,"Zero Port retriggers both envelopes on new and repeated keys");
            checkNear(retrigger.filterEnvelope.start,before.filterEnvelope.value,0,"Filter attack restarts from current level");
            checkNear(retrigger.ampEnvelope.start,before.ampEnvelope.value,0,"Amp attack restarts from current level");
            checkNear(retrigger.ampEnvelope.progress,0,0,"Retrigger restarts attack timing");
            check(retrigger.glideRemaining==0,"Zero Port has no glide");
            if(key!=60){retrigger.next();auto progress=retrigger.ampEnvelope.progress;retrigger.noteOff(key);
                checkNear(retrigger.ampEnvelope.progress,progress,0,"Note-off fallback does not retrigger");}
        }else{
            check(retrigger.ampEnvelope.stage==before.ampEnvelope.stage && retrigger.filterEnvelope.stage==before.filterEnvelope.stage,"Every positive Port value preserves legato stages");
            checkNear(retrigger.ampEnvelope.progress,before.ampEnvelope.progress,0,"Positive Port preserves envelope progress");
        }
    }
    syn::Engine e; e.noteOn(60,1); for(int i=0;i<1000;++i) e.next();
    auto stage=e.ampEnvelope.stage; double value=e.ampEnvelope.value; e.noteOn(72,0.7);
    check(e.ampEnvelope.stage==stage && e.ampEnvelope.value==value,"Legato must not retrigger");
    check(e.glideRemaining>0,"Legato glide"); e.noteOff(72); checkNear(e.targetPitch,60,0,"Last held note fallback");
    e.noteOff(60); e.next(); value=e.ampEnvelope.value; e.noteOn(67,1);
    checkNear(e.ampEnvelope.start,value,0,"Detached retrigger from current level"); check(e.glideRemaining==0,"Detached note must not glide");
    e.noteOff(44); check(e.noteCount==1,"Unrelated note-off");
    for(int target=0;target<6;++target) {
        syn::Engine dry,wet;
        dry.values[syn::Cutoff]=0.25; dry.values[syn::Level2]=0.4;
        dry.values[syn::Alt1]=dry.values[syn::Alt2]=0.2;
        dry.values[syn::AftertouchTarget]=target; dry.values[syn::AftertouchDepth]=0.3;
        dry.smooth=dry.values; wet=dry; wet.pressure=1;
        dry.noteOn(48,1); wet.noteOn(48,1);
        double difference=0;
        for(int i=0;i<4096;++i) difference+=std::abs(dry.next()-wet.next());
        check(difference>0.01,"Every aftertouch target must affect audio");
    }
    { syn::Engine dry,wet; wet.modwheel=1; dry.noteOn(60,1); wet.noteOn(60,1); double difference=0;
      for(int i=0;i<8192;++i) difference+=std::abs(dry.next()-wet.next());
      check(difference>1,"Modwheel must drive vibrato"); }
    { syn::Oscillator oscillator; oscillator.driftCountdown=1000000;
      auto p=syn::defaults(); p[syn::Wave1]=2; p[syn::Reduce1]=1;
      double distance=0; int cancellations=0;
      for(int i=0;i<4800;++i) { double previous=oscillator.hiddenPhase; double sample=oscillator.next(440,48000,p,0);
          distance+=std::fmod(oscillator.hiddenPhase-previous+1,1);
          check(std::abs(sample)<=1,"Pulse pair peak normalization"); if(sample==0) ++cancellations; }
      checkNear(distance,44*std::exp2(1.0/12),1e-9,"Pulse hidden oscillator detunes exactly one semitone");
      check(cancellations>100,"Detuned pulse pair creates moving cancellation"); }
    // Exercise every waveform, all resonance states, extreme transforms and pressure routes.
    for(double fs:{44100.,48000.,96000.}) {
        e.reset(fs); e.values[syn::Volume]=1; e.values[syn::Level2]=1; e.values[syn::Cutoff]=1; e.modwheel=1;
        for(int shape=0;shape<7;++shape) for(int res=0;res<4;++res) {
            e.values[syn::Wave1]=shape; e.values[syn::Wave2]=6-shape; e.values[syn::Resonance]=res;
            e.values[syn::Reduce1]=(shape%2); e.values[syn::Reduce2]=1;
            e.values[syn::Alt1]=1; e.values[syn::Alt2]=0.72; e.values[syn::SquishDepth1]=1;
            e.values[syn::AftertouchTarget]=shape%6; e.values[syn::AftertouchDepth]=(res%2)?1:-1; e.pressure=1;
            e.noteOn(24+shape*14,1);
            for(int i=0;i<2048;++i) { double x=e.next(); check(std::isfinite(x),"Finite output"); check(std::abs(x)<=syn::ceiling,"Limiter ceiling"); }
            e.allOff(false);
        }
        e.allOff(false); for(int i=0;i<int(fs*0.5);++i) e.next(); checkNear(e.next(),0,1e-10,"Released engine must be silent");
    }
}

const void* CLAP_ABI hostExtension(const clap_host_t*,const char*) { return nullptr; }
void CLAP_ABI hostRequest(const clap_host_t*) {}
struct EventList {
    std::vector<clap_event_midi_t> events;
    clap_input_events_t api{this,[](const clap_input_events_t* p)->uint32_t { return uint32_t(static_cast<EventList*>(p->ctx)->events.size()); },
        [](const clap_input_events_t* p,uint32_t i)->const clap_event_header_t* { return &static_cast<EventList*>(p->ctx)->events.at(i).header; }};
    void add(uint32_t time,uint8_t status,uint8_t a,uint8_t b=0) {
        clap_event_midi_t e{}; e.header={sizeof e,time,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_MIDI,0}; e.data[0]=status;e.data[1]=a;e.data[2]=b; events.push_back(e);
    }
};
struct Memory {
    std::vector<char> bytes; size_t cursor=0;
    clap_ostream_t out{this,[](const clap_ostream_t* s,const void* data,uint64_t size)->int64_t {
        auto& m=*static_cast<Memory*>(s->ctx); size_t n=size_t(std::min<uint64_t>(size,7)); auto p=static_cast<const char*>(data); m.bytes.insert(m.bytes.end(),p,p+n); return n; }};
    clap_istream_t in{this,[](const clap_istream_t* s,void* data,uint64_t size)->int64_t {
        auto& m=*static_cast<Memory*>(s->ctx); size_t n=std::min({size_t(size),size_t(5),m.bytes.size()-m.cursor}); std::memcpy(data,m.bytes.data()+m.cursor,n); m.cursor+=n; return n; }};
};
void pluginTests() {
    auto dll=LoadLibraryW(L"build/Shibboleth.clap"); check(dll!=nullptr,"Load CLAP binary");
    auto entry=reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(dll,"clap_entry")); check(entry!=nullptr,"CLAP entry export");
    check(entry->init("build/Shibboleth.clap"),"Entry init");
    auto factory=static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID)); check(factory && factory->get_plugin_count(factory)==1,"Plugin discovery");
    clap_host_t host{CLAP_VERSION_INIT,nullptr,"Test host","Local","","1",hostExtension,hostRequest,hostRequest,hostRequest};
    auto desc=factory->get_plugin_descriptor(factory,0); auto p=factory->create_plugin(factory,&host,desc->id); check(p && p->init(p),"Plugin init");
    auto params=static_cast<const clap_plugin_params_t*>(p->get_extension(p,CLAP_EXT_PARAMS));
    check(params && params->count(p)==syn::Count,"Parameter enumeration");
    for(uint32_t i=0;i<params->count(p);++i) { clap_param_info_t info{}; check(params->get_info(p,i,&info),"Param info");
        check(std::strstr(info.name,": ")!=nullptr,"Every parameter needs a classification prefix");
        if(i==syn::Wave1) check(std::strcmp(info.name,"OSC1: Wave")==0,"OSC1 naming");
        if(i==syn::NoiseCutoff) check(std::strcmp(info.name,"OPTIONS: Noise cutoff")==0,"Shared noise cutoff naming");
        char text[128]; check(params->value_to_text(p,info.id,info.default_value,text,128),"Param display"); }
    auto state=static_cast<const clap_plugin_state_t*>(p->get_extension(p,CLAP_EXT_STATE)); Memory memory;
    clap_event_param_value_t change{}; change.header={sizeof change,0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};
    change.param_id=1000+syn::Cutoff; change.note_id=change.port_index=change.channel=change.key=-1; change.value=0.23;
    clap_input_events_t single{&change,[](const clap_input_events_t*)->uint32_t { return 1; },
        [](const clap_input_events_t* s,uint32_t)->const clap_event_header_t* { return &static_cast<clap_event_param_value_t*>(s->ctx)->header; }};
    params->flush(p,&single,nullptr);
    check(state->save(p,&memory.out),"Partial stream save");
    change.value=0.87; params->flush(p,&single,nullptr);
    check(state->load(p,&memory.in),"Partial stream load");
    double restored=0; check(params->get_value(p,change.param_id,&restored),"Read restored parameter");
    checkNear(restored,0.23,1e-12,"State must restore changed values");
    change.param_id=1000+syn::NoiseEnv2;change.value=-.7;params->flush(p,&single,nullptr);
    Memory current;check(state->save(p,&current.out),"Save modulation depths");
    change.value=0;params->flush(p,&single,nullptr);
    check(state->load(p,&current.in),"Restore modulation depths");params->get_value(p,change.param_id,&restored);
    checkNear(restored,-.7,1e-12,"Signed modulation survives state round trip");
    change.param_id=1000+syn::Octave;change.value=-2;params->flush(p,&single,nullptr);
    Memory octaveState;check(state->save(p,&octaveState.out),"Save octave");change.value=1;params->flush(p,&single,nullptr);
    check(state->load(p,&octaveState.in),"Load octave");params->get_value(p,change.param_id,&restored);checkNear(restored,-2,0,"Octave saved with patch");
    change.param_id=1000+syn::FilterAlgorithm;change.value=3;params->flush(p,&single,nullptr);
    Memory algorithmState;check(state->save(p,&algorithmState.out),"Save algorithm");change.value=1;params->flush(p,&single,nullptr);
    check(state->load(p,&algorithmState.in),"Load algorithm");params->get_value(p,change.param_id,&restored);checkNear(restored,3,0,"Algorithm survives state round trip");
    for(double selection:{0.,1.}) {
        Memory old;old.bytes=algorithmState.bytes;std::memcpy(old.bytes.data(),"ASYN0006",8);
        const auto offset=old.bytes.size();old.bytes.resize(offset+sizeof selection);
        std::memcpy(old.bytes.data()+offset,&selection,sizeof selection);
        check(state->load(p,&old.in),"Load both 0.7 character selections");
        params->get_value(p,1000+syn::FilterAlgorithm,&restored);
        checkNear(restored,3,0,"Migrated patch retains selected filter");
        old.cursor=0;old.bytes.pop_back();check(!state->load(p,&old.in),"Reject truncated legacy character");
    }
    Memory version4;version4.bytes=algorithmState.bytes;std::memcpy(version4.bytes.data(),"ASYN0004",8);version4.bytes.resize(8+sizeof(double)*syn::FilterAlgorithm);
    check(state->load(p,&version4.in),"Load pre-algorithm patch");params->get_value(p,1000+syn::FilterAlgorithm,&restored);checkNear(restored,0,0,"Old patches select original filter");
    Memory version3;version3.bytes=current.bytes;std::memcpy(version3.bytes.data(),"ASYN0003",8);version3.bytes.resize(8+sizeof(double)*syn::Octave);
    check(state->load(p,&version3.in),"Load pre-octave patch");params->get_value(p,1000+syn::Octave,&restored);checkNear(restored,0,0,"Old patch retains original pitch");
    params->get_value(p,1000+syn::NoiseEnv2,&restored);checkNear(restored,-.7,1e-12,"Version 3 retains modulation depth");
    Memory version2;version2.bytes=current.bytes;std::memcpy(version2.bytes.data(),"ASYN0002",8);version2.bytes.resize(8+sizeof(double)*syn::CrushEnv1);
    check(state->load(p,&version2.in),"Load previous GUI version state");
    for(int id=syn::CrushEnv1;id<syn::Count;++id){params->get_value(p,1000+id,&restored);checkNear(restored,0,0,"Old patches have zero modulation");}
    Memory legacy; legacy.bytes=memory.bytes;
    std::memcpy(legacy.bytes.data(),"ASYN0001",8); legacy.bytes.resize(8+sizeof(double)*syn::NoiseCutoff);
    check(state->load(p,&legacy.in),"Read original prototype state");
    check(params->get_value(p,1000+syn::NoiseCutoff,&restored),"Read migrated noise cutoff");
    checkNear(restored,80,0,"Old patches receive the shared noise-cutoff default");
    memory.cursor=0; memory.bytes.resize(10); check(!state->load(p,&memory.in),"Reject truncated state");
    check(p->activate(p,48000,1,512),"Activate"); check(p->start_processing(p),"Start processing");
    EventList events; events.add(64,0x90,60,110); events.add(128,0xb0,1,127); events.add(192,0xd0,90);
    float left[512]{},right[512]{}; float* channels[]={left,right}; clap_audio_buffer_t out{}; out.data32=channels; out.channel_count=2;
    clap_process_t block{}; block.frames_count=512; block.audio_outputs=&out; block.audio_outputs_count=1; block.in_events=&events.api;
    check(p->process(p,&block)!=CLAP_PROCESS_ERROR,"Process"); double energy=0;
    for(int i=0;i<512;++i) { if(i<64) checkNear(left[i],0,0,"Sample accurate note start"); checkNear(left[i],right[i],0,"Dual mono output"); energy+=left[i]*left[i]; }
    check(energy>0.0001,"Audible output");
    events.events.clear(); events.add(0,0xb0,120,0); p->process(p,&block);
    for(float x:left) checkNear(x,0,0,"All sound off");
    p->stop_processing(p); p->deactivate(p); p->destroy(p); entry->deinit(); FreeLibrary(dll);
}
void renderDemo() {
    constexpr int fs=48000, seconds=8, count=fs*seconds;
    std::ofstream file("build/first-study.wav",std::ios::binary);
    auto u16=[&](uint16_t x){ file.write(reinterpret_cast<char*>(&x),2); };
    auto u32=[&](uint32_t x){ file.write(reinterpret_cast<char*>(&x),4); };
    file.write("RIFF",4); u32(36+count*2); file.write("WAVEfmt ",8); u32(16); u16(1);u16(1);u32(fs);u32(fs*2);u16(2);u16(16);file.write("data",4);u32(count*2);
    syn::Engine e; e.values[syn::AftertouchDepth]=0.3;
    for(int i=0;i<count;++i) {
        int section=i/(fs*2), within=i%(fs*2);
        if(within==0) {
            e.values[syn::Wave1]=section==1?4:1; e.values[syn::Alt2]=section==2?0.4:0;
            e.values[syn::Reduce1]=section==3?0.8:0; e.values[syn::SquishDepth2]=section==3?0.7:0;
            e.values[syn::Resonance]=section; e.noteOn(36+section*2,0.9);
        }
        if(within==fs) e.noteOn(43+section*2,0.85);
        if(within==fs+fs/2) { e.noteOff(43+section*2); e.noteOff(36+section*2); }
        e.modwheel=section==2?0.12:0;
        int16_t sample=int16_t(std::round(e.next()*32767)); file.write(reinterpret_cast<char*>(&sample),2);
    }
    check(bool(file),"Write audition WAV");
}
void renderFilters() {
    constexpr int fs=48000,length=fs*4,count=length*4;
    std::ofstream file("build/filter-algorithms.wav",std::ios::binary);
    auto u16=[&](uint16_t x){file.write(reinterpret_cast<char*>(&x),2);};
    auto u32=[&](uint32_t x){file.write(reinterpret_cast<char*>(&x),4);};
    file.write("RIFF",4);u32(36+count*2);file.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(fs);u32(fs*2);u16(2);u16(16);file.write("data",4);u32(count*2);
    for(int mode=0;mode<4;++mode) {
        syn::Engine e;e.values[syn::FilterAlgorithm]=mode;e.values[syn::Resonance]=2;
        e.values[syn::Cutoff]=.35;e.values[syn::FilterAmount]=.5;
        for(int n=0;n<length;++n) {
            if(n%fs==0 && n<fs*3)e.noteOn(36+(n/fs)*7,.9);
            if(n%fs==fs*3/4)e.allOff(false);
            const int16_t sample=int16_t(std::round(e.next()*32767));file.write(reinterpret_cast<const char*>(&sample),2);
        }
    }
    check(bool(file),"Write filter audition WAV");
}
int main() { try { filterTests(); coreTests(); pluginTests(); renderDemo(); renderFilters(); std::cout<<"PASS: DSP contracts, stress cases, CLAP binary loading, events, state streams; rendered first-study.wav\n"; return 0; }
    catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; } }
