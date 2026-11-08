#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace imperius {
constexpr double pi = 3.14159265358979323846;
constexpr int voiceCount = 5, oversampling = 4;
inline double sine(double phase) { return std::sin(2*pi*phase); }
inline void advance(double& phase, double increment) { phase += increment; phase -= std::floor(phase); }

// Polynomial edge correction for the square/saw discontinuities. Nonlinear
// phase modulation still relies on oversampling as well as this correction.
inline double polyBlep(double phase,double increment) {
    double dt=std::clamp(std::abs(increment),1e-8,.49);
    if(phase<dt) { double x=phase/dt;return 2*x-x*x-1; }
    if(phase>1-dt) { double x=(phase-1)/dt;return x*x+2*x+1; }
    return 0;
}
inline double oscillatorWave(double phase,double increment,int shape) {
    phase-=std::floor(phase);
    if(shape==1)return sine(phase);
    if(shape==0) {
        double half=phase+.5;half-=std::floor(half);
        return (phase<.5?1.:-1.)+polyBlep(phase,increment)-polyBlep(half,increment);
    }
    // Descending saw: same upward crossing as sine/square at phase zero.
    return 1-2*phase+polyBlep(phase,increment);
}
inline double morphWave(double phase,double increment,double shape) {
    double amount=std::clamp(std::abs(shape),0.,1.);
    if(amount==0)return sine(phase);
    return sine(phase)*(1-amount)+oscillatorWave(phase,increment,shape<0?0:2)*amount;
}

// One TPT state-variable section: two integrators, 12 dB/octave LP or HP.
struct TwoPoleFilter {
    double ic1=0,ic2=0;
    double next(double input,double g,double damping,bool highpass) {
        double a1=1/(1+g*(g+damping)),a2=g*a1,a3=g*a2;
        double v3=input-ic2,band=a1*ic1+a2*v3,low=ic2+a2*ic1+a3*v3;
        ic1=2*band-ic1;ic2=2*low-ic2;
        return highpass?input-damping*band-low:low;
    }
};

// Continuous triangular fold; its piecewise-quadratic primitive allows
// first-order antiderivative antialiasing, including crossings of fold corners.
inline double fold(double x) {
    double t=x+1; t-=4*std::floor(t/4);
    return t<2 ? t-1 : 3-t;
}
inline double foldIntegral(double x) {
    double t=x+1; t-=4*std::floor(t/4);
    return t<2 ? .5*t*t-t : 3*t-.5*t*t-4;
}
struct Folder {
    double previous=0; bool ready=false;
    double next(double x) {
        if(!ready) { previous=x; ready=true; return fold(x); }
        const double delta=x-previous;
        const double y=ready && std::abs(delta)>1e-7
            ? (foldIntegral(x)-foldIntegral(previous))/delta : fold((x+previous)*.5);
        previous=x; ready=true; return y;
    }
};
struct ADSR {
    double attack=.008, decay=.35, sustain=.65, release=.4;
};
struct Envelope {
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage=Idle;
    double value=0, releaseStep=0;
    void on() { stage=Attack; }
    void off(double seconds, double fs) {
        if(stage!=Idle) { stage=Release; releaseStep=value/std::max(1.,seconds*fs); }
    }
    double next(const ADSR& p, double fs) {
        switch(stage) {
        case Idle: return value=0;
        case Attack:
            value+=1/std::max(1.,p.attack*fs);
            if(value>=1) { value=1; stage=Decay; } break;
        case Decay:
            value-=(1-p.sustain)/std::max(1.,p.decay*fs);
            if(value<=p.sustain) { value=p.sustain; stage=Sustain; } break;
        case Sustain: value=p.sustain; break;
        case Release:
            value=std::max(0.,value-releaseStep);
            if(value==0) stage=Idle;
            break;
        }
        return value;
    }
};
struct LFO {
    double rate=.5; // Hz when free, index into beat divisions when synced.
    double shape=0; // sine -> triangle
    bool sync=false;
};
inline constexpr std::array<double,10> beats{8,4,3,2,1.5,1,.75,.5,1./3,.25};
inline double lfoHz(const LFO& p, double bpm) {
    return p.sync ? std::clamp(bpm,20.,300.)/(60*beats[std::clamp(int(std::round(p.rate)),0,9)])
                  : std::clamp(p.rate,.01,100.);
}
struct Controls {
    int octave=0; // three-position switch: -1, 0, +1
    double harmonics=.2, fold=.15, bias=0, basicTune=0;
    double complexShape=0; // -1 square, 0 sine, +1 saw, before the folder
    int basicWave=1; // 0 square, 1 sine, 2 saw
    double fm=0, harmonicMod=0, foldMod=0;
    double complexLevel=.8, basicLevel=.15, subLevel=.15;
    double cutoff=7000, resonance=.15, filterEnv=0;
    bool highpass=false;
    double harmonicEnv=0, foldEnv=0, harmonicLfo=0, foldLfo=0;
    double volume=.65;
    ADSR amp{}, mod{.008,.5,.25,.5};
    std::array<LFO,2> lfos{};
};
// Shared post-mix decimator: no FIR multiplication per voice.
struct Decimator {
    static constexpr int taps=129, latency=16;
    std::array<double,taps> weights{}, history{};
    int cursor=0;
    Decimator() {
        double sum=0;
        for(int i=0;i<taps;++i) {
            double x=i-(taps-1)*.5;
            double window=.42-.5*std::cos(2*pi*i/(taps-1))+.08*std::cos(4*pi*i/(taps-1));
            weights[i]=(x==0?.225:std::sin(2*pi*.1125*x)/(pi*x))*window;
            sum+=weights[i];
        }
        for(auto& w:weights) w/=sum;
    }
    void push(double x) { history[cursor]=x; cursor=(cursor+1)%taps; }
    double output() const {
        double result=0; int j=cursor;
        for(int i=0;i<taps;++i) { j=(j+taps-1)%taps; result+=weights[i]*history[j]; }
        return result;
    }
};
struct Voice {
    int key=-1, channel=0; uint64_t age=0;
    bool held=false, sustained=false;
    double phase=.13, basicPhase=.37, subPhase=.19, velocity=1, frequency=440;
    double dcIn=0,dcOut=0,last=0,stealTail=0;
    double previousCarrierPhase=0;bool carrierReady=false;
    TwoPoleFilter filter;
    Envelope amp,mod; Folder folder;
    bool active() const { return amp.stage!=Envelope::Idle || std::abs(stealTail)>1e-8; }
};
struct Engine {
    Controls controls;
    std::array<Voice,voiceCount> voices{};
    std::array<bool,16> pedal{};
    std::array<double,16> bend{};
    std::array<double,2> lfoPhase{};
    double sampleRate=48000,bpm=120,limiter=1;
    uint64_t serial=0;
    Decimator decimator;
    void reset(double fs=48000) {
        *this=Engine{}; sampleRate=std::clamp(fs,8000.,192000.);
    }
    int activeVoices() const {
        int result=0; for(const auto& v:voices) result+=v.active(); return result;
    }
    void noteOn(int key,double velocity,int channel=0) {
        if(key<0 || key>127 || channel<0 || channel>15) return;
        if(velocity<=0) { noteOff(key,channel); return; }
        Voice* chosen=nullptr;
        for(auto& v:voices) if(!v.active()) { chosen=&v; break; }
        if(!chosen) {
            // Release tails before held notes; oldest within either category.
            chosen=&voices[0];
            for(auto& v:voices)
                if((!v.held && chosen->held) || (v.held==chosen->held && v.age<chosen->age)) chosen=&v;
        }
        double tail=chosen->last;
        const double phase=chosen->phase,basic=chosen->basicPhase,sub=chosen->subPhase;
        *chosen=Voice{};
        chosen->phase=phase; chosen->basicPhase=basic; chosen->subPhase=sub;
        chosen->stealTail=tail; chosen->key=key; chosen->channel=channel;
        chosen->velocity=std::clamp(velocity,0.,1.); chosen->age=++serial; chosen->held=true;
        chosen->frequency=440*std::exp2((key-69)/12.);
        chosen->amp.on(); chosen->mod.on();
    }
    void release(Voice& v) {
        v.sustained=false;
        v.amp.off(controls.amp.release,sampleRate*oversampling);
        v.mod.off(controls.mod.release,sampleRate*oversampling);
    }
    void noteOff(int key,int channel=0) {
        // Repeated MIDI note-ons pair with note-offs in arrival order.
        Voice* oldest=nullptr;
        for(auto& v:voices) if(v.held && v.key==key && v.channel==channel && (!oldest || v.age<oldest->age)) oldest=&v;
        if(oldest) {
            oldest->held=false;
            if(pedal[channel]) oldest->sustained=true; else release(*oldest);
        }
    }
    void sustain(bool down,int channel=0) {
        if(channel<0 || channel>15) return;
        pedal[channel]=down;
        if(!down) for(auto& v:voices) if(v.channel==channel && v.sustained) release(v);
    }
    void allOff(bool immediate,int channel=-1) {
        for(auto& v:voices) if(channel<0 || v.channel==channel) {
            v.held=false;
            if(immediate) v=Voice{};
            else if(channel>=0 && pedal[channel]) v.sustained=true;
            else release(v);
        }
        if(immediate && channel<0) { decimator.history.fill(0); limiter=1; }
    }
    double next() {
        const auto& p=controls;
        const double fs=sampleRate*oversampling;
        const double g=std::tan(pi*std::clamp(p.cutoff,20.,sampleRate*.43)/fs);
        const double k=2-1.9*std::clamp(p.resonance,0.,1.);
        const double dc=std::exp(-2*pi*12/fs), tailDecay=std::exp(-1/(.002*fs));
        for(int os=0;os<oversampling;++os) {
            std::array<double,2> lfo{};
            for(int n=0;n<2;++n) {
                advance(lfoPhase[n],lfoHz(p.lfos[n],bpm)/fs);
                double triangle=1-4*std::abs(lfoPhase[n]-.5);
                lfo[n]=sine(lfoPhase[n])*(1-p.lfos[n].shape)+triangle*p.lfos[n].shape;
            }
            double mix=0;
            for(auto& v:voices) {
                if(!v.active()) continue;
                double env=v.amp.next(p.amp,fs),mod=v.mod.next(p.mod,fs);
                double f=v.frequency*std::exp2(std::clamp(p.octave,-1,1)+bend[v.channel]/12);
                f=std::min(f,fs*.1);
                double basicIncrement=std::min(f*std::exp2(p.basicTune/12),fs*.15)/fs;
                advance(v.basicPhase,basicIncrement);
                double basic=oscillatorWave(v.basicPhase,basicIncrement,p.basicWave);
                advance(v.phase,f/fs); advance(v.subPhase,f*.5/fs);
                double h=std::clamp(p.harmonics+p.harmonicMod*basic+p.harmonicEnv*mod+p.harmonicLfo*lfo[0],0.,1.);
                double folds=std::clamp(p.fold+p.foldMod*basic+p.foldEnv*mod+p.foldLfo*lfo[1],0.,1.);
                // Continuous phase shaping adds harmonics before folding.
                double carrierPhase=v.phase+.35*h*sine(v.phase)+.5*p.fm*basic;
                double carrier;
                if(p.complexShape==0) {
                    carrier=sine(carrierPhase);v.carrierReady=false;
                } else {
                    carrierPhase-=std::floor(carrierPhase);
                    double carrierIncrement=f/fs;
                    if(v.carrierReady) {
                        double delta=carrierPhase-v.previousCarrierPhase;
                        delta-=std::round(delta);
                        carrierIncrement=std::max(f/fs,std::abs(delta));
                    }
                    v.previousCarrierPhase=carrierPhase;v.carrierReady=true;
                    carrier=morphWave(carrierPhase,carrierIncrement,p.complexShape);
                }
                double complex=v.folder.next(carrier*(1+7*folds)+p.bias);
                double input=std::tanh(1.3*(p.complexLevel*complex+p.basicLevel*basic+p.subLevel*sine(v.subPhase)));
                double ac=input-v.dcIn+dc*v.dcOut; v.dcIn=input; v.dcOut=ac;
                // TPT state-variable filter, per voice. Recompute only if modulated.
                double vg=p.filterEnv==0 ? g : std::tan(pi*std::clamp(p.cutoff*std::exp2(5*p.filterEnv*mod),20.,sampleRate*.43)/fs);
                double filtered=v.filter.next(ac,vg,k,p.highpass);
                v.last=filtered*env*v.velocity+v.stealTail;
                v.stealTail*=tailDecay;
                mix+=v.last;
            }
            decimator.push(mix*.22*p.volume);
        }
        double out=decimator.output(),peak=std::abs(out);
        double target=peak>.84?.84/peak:1;
        limiter=target<limiter?target:1-(1-limiter)*std::exp(-1/(.06*sampleRate));
        return out*limiter;
    }
};
} // namespace imperius
