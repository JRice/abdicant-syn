#include "../src/engine.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace imperius;
void check(bool yes,const char* message) { if(!yes) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char* message) { check(std::abs(a-b)<=tolerance,message); }
double filterGain(double cutoff,double frequency,bool highpass,double resonance=0) {
    constexpr double fs=192000;
    TwoPoleFilter filter;
    double g=std::tan(pi*cutoff/fs),energy=0;
    for(int n=0;n<int(fs*1.2);++n) {
        double y=filter.next(sine(frequency*n/fs),g,2-1.9*resonance,highpass);
        if(n>=int(fs*.2))energy+=y*y;
    }
    return std::sqrt(2*energy/fs);
}
void tests() {
    for(double phase:{.125,.375,.625,.875}) {
        near(morphWave(phase,.001,0),sine(phase),1e-12,"Morph center is sine");
        near(morphWave(phase,.001,-1),phase<.5?1:-1,1e-12,"Morph left endpoint is square");
        near(morphWave(phase,.001,1),1-2*phase,1e-12,"Morph right endpoint is saw");
        near(morphWave(phase,.001,-.5),.5*(sine(phase)+oscillatorWave(phase,.001,0)),1e-12,"Continuous square/sine midpoint");
        near(morphWave(phase,.001,.5),.5*(sine(phase)+oscillatorWave(phase,.001,2)),1e-12,"Continuous sine/saw midpoint");
    }
    const double lpSlope=20*std::log10(filterGain(250,4000,false)/filterGain(250,2000,false));
    const double hpSlope=20*std::log10(filterGain(1000,62.5,true)/filterGain(1000,125,true));
    check(lpSlope> -12.3 && lpSlope< -11.8,"Low-pass stopband is two-pole, not four-pole");
    check(hpSlope> -12.3 && hpSlope< -11.8,"High-pass stopband is two-pole, not four-pole");
    std::ofstream response("build/filter-response.txt");
    response<<"One TPT section, resonance zero, internal rate 192 kHz.\n"
        <<"LP cutoff 250 Hz; gain change 2 kHz to 4 kHz: "<<lpSlope<<" dB/oct\n"
        <<"HP cutoff 1 kHz; gain change 125 Hz to 62.5 Hz: "<<hpSlope<<" dB/oct\n";
    std::cout<<"Measured filter slopes: LP "<<lpSlope<<", HP "<<hpSlope<<" dB/oct\n";
    for(double x=-30;x<30;x+=.013) {
        near(fold(x),fold(x+4),1e-12,"Fold periodicity");
        near(fold(-x),-fold(x),1e-12,"Fold symmetry");
        near((foldIntegral(x+1e-6)-foldIntegral(x-1e-6))/2e-6,fold(x),3e-6,"Fold primitive derivative");
    }
    Folder folder;
    near(folder.next(.6),.6,1e-12,"Initial fold sample");
    for(int i=0;i<1000;++i) near(folder.next(.6),.6,1e-12,"Constant fold input");
    folder=Folder{}; folder.next(-7.3);
    double sum=0; constexpr int steps=100000;
    for(int i=0;i<steps;++i) sum+=fold(-7.3+(i+.5)*16.7/steps);
    near(folder.next(9.4),sum/steps,1e-7,"ADAA integrates across multiple corners");
    Engine e;
    for(int i=0;i<5;++i) e.noteOn(60+i,1);
    check(e.activeVoices()==5,"Five voices sound together");
    e.noteOff(62); e.noteOn(72,1);
    bool retainedOldest=false,replacedRelease=true;
    for(const auto& v:e.voices) { retainedOldest|=v.key==60; replacedRelease&=v.key!=62; }
    check(retainedOldest && replacedRelease,"Steal release voice before oldest held voice");
    e.noteOn(74,1); bool oldestGone=true;
    for(const auto& v:e.voices) oldestGone&=v.key!=60;
    check(oldestGone && e.activeVoices()==5,"Sixth held note steals oldest");
    e.reset(); e.noteOn(60,1); e.noteOn(60,.5); e.noteOff(60);
    check(!e.voices[0].held && e.voices[1].held,"Repeated notes pair in arrival order");
    e.reset(); e.sustain(true,2); e.noteOn(60,1,2);
    for(int n=0;n<2000;++n) e.next();
    e.noteOff(60,2); check(e.voices[0].sustained,"Sustain holds released key");
    e.sustain(false,1); check(e.voices[0].sustained,"Sustain is channel-specific");
    e.sustain(false,2);
    for(int n=0;n<48000;++n) e.next();
    check(e.activeVoices()==0,"Voices sleep after release");
    for(int n=0;n<500;++n) near(e.next(),0,1e-10,"Silence after release and decimator tail");
    for(int octave=-1;octave<=1;++octave) {
        Engine a,b; a.controls.octave=octave; a.noteOn(60,1); b.noteOn(60+12*octave,1);
        for(int n=0;n<2000;++n) near(a.next(),b.next(),1e-12,"Three-way octave transposes both oscillators and sub");
    }
    LFO lfo; lfo.sync=true; lfo.rate=5;
    near(lfoHz(lfo,120),2,1e-12,"Synced quarter note at 120 BPM");
    near(lfoHz(lfo,90),1.5,1e-12,"Sync follows host tempo");
    lfo.sync=false; near(lfoHz(lfo,90),5,1e-12,"Free LFO uses Hz");
    for(double fs:{44100.,48000.,96000.}) for(bool hp:{false,true}) for(int wave=0;wave<3;++wave) {
        Engine stress; stress.reset(fs);
        auto& p=stress.controls; p.fold=1; p.bias=.8; p.fm=1; p.harmonicMod=1; p.foldMod=1;
        p.resonance=1; p.cutoff=fs*.43; p.filterEnv=1; p.highpass=hp;
        p.complexLevel=p.basicLevel=p.subLevel=1; p.volume=1;
        p.complexShape=wave-1;p.basicWave=wave;
        for(int key:{36,60,84,108,127}) stress.noteOn(key,1);
        for(int n=0;n<int(fs*.3);++n) {
            double out=stress.next();
            check(std::isfinite(out) && std::abs(out)<=.84000001,"Extreme five-voice output finite and peak bounded");
        }
    }
    // Actual audible routes must affect the output, rather than just stored controls.
    for(int route=0;route<10;++route) {
        Engine a,b;
        switch(route) {
        case 0:b.controls.fold=.8;break;
        case 1:b.controls.harmonics=.9;break;
        case 2:b.controls.fm=.6;break;
        case 3:b.controls.foldMod=.7;break;
        case 4:b.controls.foldEnv=.8;break;
        case 5:b.controls.foldLfo=.8;break;
        case 6:b.controls.complexShape=-1;break;
        case 7:b.controls.complexShape=1;break;
        case 8:b.controls.basicWave=0;break;
        case 9:b.controls.basicWave=2;break;
        }
        a.noteOn(60,1); b.noteOn(60,1); double difference=0;
        for(int n=0;n<6000;++n) difference+=std::abs(a.next()-b.next());
        check(difference>1,"Shaping and modulation routes change rendered audio");
    }
    for(int wave:{0,2}) {
        Engine a,b;
        a.controls.basicLevel=b.controls.basicLevel=0;
        a.controls.fm=b.controls.fm=.5;b.controls.basicWave=wave;
        a.noteOn(60,1);b.noteOn(60,1);double difference=0;
        for(int n=0;n<6000;++n)difference+=std::abs(a.next()-b.next());
        check(difference>1,"Basic waveform also changes audio-rate modulation source");
    }
    std::cout<<"DSP checks passed: folding, voice allocation, sustain, release, octave, sync, modulation, stress.\n";
}
void u16(std::ofstream& f,uint16_t value) { f.put(char(value)); f.put(char(value>>8)); }
void u32(std::ofstream& f,uint32_t value) { u16(f,uint16_t(value)); u16(f,uint16_t(value>>16)); }
void wav(const std::string& name,const std::vector<float>& data) {
    std::ofstream f(name,std::ios::binary);
    check(bool(f),"Open WAV output");
    f.write("RIFF",4); u32(f,36+uint32_t(data.size()*2)); f.write("WAVEfmt ",8);
    u32(f,16); u16(f,1);u16(f,1);u32(f,48000);u32(f,96000);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,uint32_t(data.size()*2));
    for(float x:data) u16(f,uint16_t(int16_t(std::round(std::clamp(double(x),-1.,1.)*32767))));
    check(bool(f),"Write WAV output");
}
void render(int kind,const char* name) {
    Engine e; auto& p=e.controls;
    p.amp.release=.65;
    if(kind==0) { p.harmonics=0; p.fold=0; p.basicLevel=p.subLevel=0; p.cutoff=15000; }
    if(kind==1) { p.harmonics=.35; p.fold=.3; p.foldEnv=.5; p.cutoff=9000; p.amp={.004,.8,.15,.65}; }
    if(kind==2) { p.fold=.35; p.foldMod=.35; p.fm=.25; p.basicTune=7; p.cutoff=10000; p.amp={.008,1,.2,.8}; }
    if(kind==3) { p.fold=.3; p.harmonics=.3; p.foldLfo=.12; p.harmonicLfo=.18; p.lfos[0].rate=.23; p.lfos[1].rate=.37; p.cutoff=2400; p.filterEnv=.5; p.amp={.7,.8,.7,1.2}; }
    std::vector<float> audio; audio.reserve(48000*8);
    double peak=0,energy=0;
    for(int n=0;n<48000*8;++n) {
        if(kind==0) {
            if(n==0) e.noteOn(48,.85);
            if(n<48000*5) { p.fold=double(n)/(48000*5); p.harmonics=.6*double(n)/(48000*5); }
            if(n==48000*6) e.noteOff(48);
        } else if(kind<3) {
            if(n%(48000)==0 && n<48000*6) {
                const int keys[]{48,55,60,63,67,72}; e.noteOn(keys[n/48000],.85);
            }
            if(n%48000==36000 && n<48000*6) {
                const int keys[]{48,55,60,63,67,72};e.noteOff(keys[n/48000]);
            }
        } else {
            if(n==0) for(int key:{48,55,60,63,70}) e.noteOn(key,.8);
            if(n==48000*5) for(int key:{48,55,60,63,70}) e.noteOff(key);
        }
        double x=e.next();check(std::isfinite(x),"Render finite");audio.push_back(float(x));
        peak=std::max(peak,std::abs(x));energy+=x*x;
    }
    // Listening files share a -3 dBFS peak; engine gain is left untouched.
    const double auditionGain=peak>0?.707945784/peak:1;
    for(auto& x:audio) x=float(x*auditionGain);
    wav(name,audio);
    std::cout<<name<<" peak "<<peak<<", RMS "<<std::sqrt(energy/audio.size())<<"\n";
}
void benchmark() {
    std::ofstream report("build/benchmark.txt");
    for(double fs:{48000.,96000.}) {
        Engine e; e.reset(fs); e.controls.fold=.8; e.controls.fm=.4;
        e.controls.foldMod=.5; e.controls.filterEnv=.6;
        for(int key:{48,55,60,63,70}) e.noteOn(key,1);
        double checksum=0;constexpr double seconds=3;
        auto start=std::chrono::steady_clock::now();
        for(int n=0;n<int(fs*seconds);++n) checksum+=e.next();
        double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::string line=std::to_string(int(fs))+" Hz, 5 sustained voices, modulated filter/folder: "+std::to_string(elapsed/seconds*100)+"% of one core, "+std::to_string(seconds/elapsed)+"x realtime, checksum "+std::to_string(checksum)+"\n";
        std::cout<<line; report<<line;
    }
}
int main() {
    try {
        tests();
        render(0,"build/01-fold-sweep.wav");
        render(1,"build/02-fold-plucks.wav");
        render(2,"build/03-audio-rate-metal.wav");
        render(3,"build/04-five-voice-pad.wav");
        benchmark(); return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
