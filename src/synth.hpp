#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace syn {
constexpr double pi = 3.14159265358979323846;
constexpr double ceiling = 0.8413951416451951; // -1.5 dBFS, sample peak
enum Param { Wave1, Reduce1, Alt1, Noise1, SquishRate1, SquishDepth1,
    Wave2, Reduce2, Alt2, Noise2, SquishRate2, SquishDepth2, Level2, Tune2,
    Cutoff, Resonance, FilterAmount, Attack, Decay, Sustain, Release,
    Velocity, Glide, VibratoRate, AftertouchTarget, AftertouchDepth, Volume, NoiseCutoff, CrushEnv1, AltEnv1, NoiseEnv1, CrushEnv2, AltEnv2, NoiseEnv2, Octave, Count };
struct Definition { const char* name; const char* group; double min, max, initial; bool stepped; };
inline constexpr std::array<Definition, Count> definitions {{
    {"Wave", "Oscillator 1",0,6,1,true}, {"Crush", "Oscillator 1",0,1,0,false},
    {"Alt", "Oscillator 1",0,1,0,false}, {"Noise AM", "Oscillator 1",0,1,0,false},
    {"Squish rate", "Oscillator 1",0.05,12,0.4,false}, {"Squish depth", "Oscillator 1",0,1,0,false},
    {"Wave", "Oscillator 2",0,6,0,true}, {"Crush", "Oscillator 2",0,1,0,false},
    {"Alt", "Oscillator 2",0,1,0,false}, {"Noise AM", "Oscillator 2",0,1,0,false},
    {"Squish rate", "Oscillator 2",0.05,12,0.61,false}, {"Squish depth", "Oscillator 2",0,1,0,false},
    {"Level", "Oscillator 2",0,1,0.65,false}, {"Tune", "Oscillator 2",-1,1,0,false},
    {"Cutoff", "Filter",0,1,0.58,false}, {"Resonance", "Filter",0,3,1,true},
    {"Envelope amount", "Filter",0,1,0.45,false},
    {"Attack", "Envelope",0.001,3,0.006,false}, {"Decay", "Envelope",0.005,5,0.25,false},
    {"Sustain", "Envelope",0,1,0.3,false}, {"Release", "Envelope",0.005,5,0.18,false},
    {"Velocity", "Performance",0,1,0.5,false}, {"Glide", "Performance",0,0.1,0.025,false},
    {"Vibrato rate", "Performance",2,6,4,false},
    {"Aftertouch target", "Performance",0,5,0,true}, {"Aftertouch depth", "Performance",-1,1,0,false},
    {"Volume", "Output",0,1,0.65,false},
    {"Noise cutoff", "Options",10,1000,80,false},
    {"Crush envelope", "Oscillator 1",-1,1,0,false}, {"Alt envelope", "Oscillator 1",-1,1,0,false},
    {"Noise AM envelope", "Oscillator 1",-1,1,0,false},
    {"Crush envelope", "Oscillator 2",-1,1,0,false}, {"Alt envelope", "Oscillator 2",-1,1,0,false},
    {"Noise AM envelope", "Oscillator 2",-1,1,0,false},
    {"Octave", "Oscillator 1",-2,1,0,true}
}};
using Values = std::array<double, Count>;
inline Values defaults() { Values v{}; for (int i=0;i<Count;++i) v[i]=definitions[i].initial; return v; }
inline double validValue(int id, double value) {
    const auto& d=definitions[id];
    if (!std::isfinite(value)) return d.initial;
    value=std::clamp(value,d.min,d.max); return d.stepped ? std::round(value) : value;
}
inline double tuning(double knob) {
    double a=std::abs(std::clamp(knob,-1.0,1.0));
    double steps=a<=0.5 ? a*2 : a>=0.999999 ? 24 : std::min(12.0,2+std::floor((a-0.5)*22));
    return std::copysign(steps,knob);
}
inline double resonance(int state) { return std::array<double,4>{0,0.1,0.5,0.95}[std::clamp(state,0,3)]; }
inline double crush(double value,double amount) {
    if(amount<=0) return value;
    // Symmetric amplitude quantization retains both zero and -1 Squish tails.
    const double steps=std::exp2(15*(1-std::clamp(amount,0.0,1.0)));
    return std::clamp(std::round(value*steps)/steps,-1.0,1.0);
}
inline double ampSustain(double sustain) {
    const double t=std::clamp(6*sustain,0.0,1.0);
    return (1+sustain)*0.5*t*t*(3-2*t);
}
inline constexpr std::array<int,6> modTargets{Reduce1,Alt1,Noise1,Reduce2,Alt2,Noise2};
inline Values envelopeMods(const Values& base,double envelope) {
    Values result=base;
    for(int i=0;i<6;++i) result[modTargets[i]]=std::clamp(base[modTargets[i]]+base[CrushEnv1+i]*envelope,0.0,1.0);
    return result;
}
inline double hz(double note) { return 440*std::exp2((note-69)/12); }

struct Envelope {
    enum Stage { Idle, AttackStage, DecayStage, SustainStage, ReleaseStage } stage=Idle;
    double value=0, start=0, progress=0;
    void enter(Stage s) { stage=s; start=value; progress=0; }
    void on() { enter(AttackStage); }
    void off() { if(stage!=Idle) enter(ReleaseStage); }
    double next(double dt,double a,double d,double s,double r) {
        if(stage==Idle) return value=0;
        if(stage==SustainStage) return value=s;
        double duration=stage==AttackStage?a:stage==DecayStage?d:r;
        progress=std::min(1.0,progress+dt/std::max(0.0001,duration));
        double curve=(1-std::exp(-5*progress))/(1-std::exp(-5.0));
        double target=stage==AttackStage?1:stage==DecayStage?s:0;
        value=start+(target-start)*curve;
        if(progress>=1) {
            if(stage==AttackStage) enter(DecayStage);
            else if(stage==DecayStage) enter(SustainStage);
            else enter(Idle);
        }
        return value;
    }
};

inline double wave(double p,int shape) {
    p-=std::floor(p);
    const auto tri=[](double x){ return 1-4*std::abs(x-0.5); };
    switch(shape) {
    case 0: return tri(p);
    case 1: return 2*p-1;
    case 2: return p<0.5?1:-1;
    case 3: return p<0.5?1-std::abs(4*p-1):0;
    case 4: return p<0.5?(p<0.25?1:-1):tri(2*p-1);
    case 5: return std::clamp(1.7*tri(p),-1.0,1.0);
    default: { double x=1.6*tri(p)+0.25; return x>1?2-x:x< -1?-2-x:x; }
    }
}
struct Oscillator {
    double phase=0.137, hiddenPhase=0.137, lfo=0, noise=0, drift=0, driftTarget=0;
    int driftCountdown=0;
    uint32_t rng=0x735a2d91;
    double random() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return double(rng)/2147483648.0-1; }
    double next(double frequency,double fs,const Values& p,int offset) {
        if(--driftCountdown<=0) { driftTarget=random()*0.018; driftCountdown=int(fs*0.37); }
        drift+=(driftTarget-drift)*(1-std::exp(-1/(fs*0.8)));
        frequency=std::min(fs*0.2,frequency*std::exp2(drift/12));
        lfo+=p[offset+4]/fs; lfo-=std::floor(lfo);
        const double width=1-0.92*p[offset+5]*(0.5+0.5*std::sin(2*pi*lfo));
        const int shape=int(p[offset]);
        // Saw and half-triangle retain a zero tail; the other shapes rest at -1.
        const double tail=(shape==1 || shape==3)?0.0:-1.0;
        // Phase warping implements an internal slave reset at each master period.
        double value=phase<width?wave(phase/width*(1+15*p[offset+2]),shape):tail;
        const double amount=p[offset+1];
        // The hidden pulse oscillator keeps its phase even when its mix is zero.
        if(int(p[offset])==2) {
            const double hidden=hiddenPhase<width?wave(hiddenPhase/width*(1+15*p[offset+2]),2):tail;
            // Blend in the equal-weight pair close to zero to avoid an abrupt switch.
            const double blend=0.5*std::min(1.0,amount/0.03);
            value=value*(1-blend)+hidden*blend;
        } else value=crush(value,amount);
        hiddenPhase+=frequency*std::exp2(amount/12)/fs; hiddenPhase-=std::floor(hiddenPhase);
        const double noiseAlpha=1-std::exp(-2*pi*p[NoiseCutoff]/fs);
        noise+=(random()-noise)*noiseAlpha;
        // Maintain modulation strength as bandwidth/sample rate change. Double
        // the old noise excursion about its 0.5 mean, not the oscillator volume.
        const double referenceAlpha=1-std::exp(-2*pi*150/192000.0);
        const double noiseScale=std::sqrt((referenceAlpha/(2-referenceAlpha))/(noiseAlpha/(2-noiseAlpha)));
        const double gain=std::clamp(0.5+noise*5*noiseScale,0.0,1.0);
        value*=1+p[offset+3]*(gain-1);
        phase+=frequency/fs; phase-=std::floor(phase);
        return value;
    }
};

// Windowed-sinc decimator: synthesis and nonlinear processing run at 4x.
// This reduces aliasing but hard-sync/squish still need dedicated edge correction.
struct Decimator {
    static constexpr int taps=97;
    std::array<double,taps> weights{}, history{}; int cursor=0;
    Decimator() {
        double sum=0;
        for(int i=0;i<taps;++i) { double x=i-(taps-1)*0.5;
            double w=0.42-0.5*std::cos(2*pi*i/(taps-1))+0.08*std::cos(4*pi*i/(taps-1));
            weights[i]=(x==0?0.225:std::sin(2*pi*0.1125*x)/(pi*x))*w; sum+=weights[i]; }
        for(auto& w:weights) w/=sum;
    }
    void push(double x) { history[cursor]=x; cursor=(cursor+1)%taps; }
    double output() const { double y=0; int j=cursor; for(int i=0;i<taps;++i) { j=(j+taps-1)%taps; y+=weights[i]*history[j]; } return y; }
};
struct ScopeTrace {
    std::array<float,128> samples{};
    double previousPhase=0,previousSample=0;
    bool ready=false;
    void push(double phase,double sample) {
        if(!ready){previousPhase=phase;previousSample=sample;ready=true;return;}
        double travel=phase-previousPhase;if(travel<0)travel+=1;
        if(travel>0 && travel<0.5){
            int first=int(std::floor(previousPhase*128))+1,last=int(std::floor((previousPhase+travel)*128));
            for(int i=first;i<=last;++i){double t=(i/128.0-previousPhase)/travel;samples[size_t(i%128)]=float(previousSample+(sample-previousSample)*t);}
        }
        previousPhase=phase;previousSample=sample;
    }
};
struct Engine {
    Values values=defaults(), smooth=defaults();
    double sampleRate=48000, pitch=60, targetPitch=60, glideIncrement=0;
    int glideRemaining=0;
    struct Note { int key,channel; double velocity; int noteId=-1; };
    std::array<Note,128> notes{}; int noteCount=0;
    double velocity=1, smoothVelocity=1, bend=0, vibratoPhase=0, modwheel=0, pressure=0, smoothWheel=0, smoothPressure=0;
    Envelope filterEnvelope, ampEnvelope;
    Oscillator osc1,osc2;
    ScopeTrace scopes[2];
    bool captureScopes=false;
    Decimator decimator;
    double ic1=0,ic2=0,dcInput=0,dcOutput=0,limiterGain=1;
    Engine() { osc2.rng=0x12bd871f; osc2.phase=osc2.hiddenPhase=0.43; }
    void reset(double rate) { *this=Engine(); sampleRate=rate; }
    void movePitch(int key,bool legato) {
        targetPitch=key;
        glideRemaining=legato?int(values[Glide]*sampleRate):0;
        if(glideRemaining>0) glideIncrement=(targetPitch-pitch)/glideRemaining;
        else pitch=targetPitch;
    }
    void noteOn(int key,double vel,int channel=0,int noteId=-1) {
        bool legato=noteCount>0;
        // MIDI repeated keys replace the previous held instance.
        for(int i=0;i<noteCount;++i) if(notes[i].key==key && notes[i].channel==channel) {
            for(int j=i;j<noteCount-1;++j) { notes[j]=notes[j+1]; }
            --noteCount; break;
        }
        if(noteCount==128) { for(int j=0;j<127;++j) notes[j]=notes[j+1]; --noteCount; }
        notes[noteCount++]={key,channel,vel,noteId}; velocity=vel;
        // A silent attack can start at the requested velocity immediately. While
        // sounding (including release tails), preserve gain/control continuity.
        if(!legato && ampEnvelope.value==0) smoothVelocity=vel;
        movePitch(key,legato);
        // Exact control endpoint selects retrigger mode; a short nonzero
        // portamento still means legato, even below one sample of glide.
        if(!legato || values[Glide]==0) { filterEnvelope.on(); ampEnvelope.on(); }
    }
    void noteOff(int key,int channel=0) {
        bool wasCurrent=noteCount && notes[noteCount-1].key==key && notes[noteCount-1].channel==channel;
        bool found=false;
        for(int i=0;i<noteCount;++i) if(notes[i].key==key && notes[i].channel==channel) {
            for(int j=i;j<noteCount-1;++j) { notes[j]=notes[j+1]; }
            --noteCount; found=true; break;
        }
        if(!found) return;
        if(!noteCount) { filterEnvelope.off(); ampEnvelope.off(); }
        else if(wasCurrent) { velocity=notes[noteCount-1].velocity; movePitch(notes[noteCount-1].key,true); }
    }
    void allOff(bool immediate) {
        noteCount=0; filterEnvelope.off(); ampEnvelope.off();
        if(immediate) { filterEnvelope={}; ampEnvelope={}; ic1=ic2=0; decimator.history.fill(0); dcInput=dcOutput=0; }
    }
    double next() {
        double smoothing=1-std::exp(-1/(0.004*sampleRate));
        for(int i=0;i<Count;++i) smooth[i]=definitions[i].stepped?values[i]:smooth[i]+smoothing*(values[i]-smooth[i]);
        if(std::abs(smooth[Sustain]-values[Sustain])<1e-12)smooth[Sustain]=values[Sustain];
        // Tune is mapped before pitch glide; never glide through unwanted knob steps.
        smooth[Tune2]=values[Tune2];
        smoothWheel+=smoothing*(modwheel-smoothWheel);
        smoothPressure+=smoothing*(pressure-smoothPressure);
        Values effective=smooth;
        double expression=smoothPressure*smooth[AftertouchDepth];
        double resonantValue=resonance(int(smooth[Resonance]));
        switch(int(smooth[AftertouchTarget])) {
        case 0: effective[Cutoff]=std::clamp(smooth[Cutoff]+expression,0.0,1.0); break;
        case 1: resonantValue=std::clamp(resonantValue+expression,0.0,0.95); break;
        case 2: effective[Level2]=std::clamp(smooth[Level2]+expression,0.0,1.0); break;
        case 3: effective[Alt1]=std::clamp(smooth[Alt1]+expression,0.0,1.0); break;
        case 4: effective[Alt2]=std::clamp(smooth[Alt2]+expression,0.0,1.0); break;
        case 5: effective[Alt1]=std::clamp(smooth[Alt1]+expression,0.0,1.0);
                effective[Alt2]=std::clamp(smooth[Alt2]+expression,0.0,1.0); break;
        }
        if(glideRemaining>0) { pitch+=glideIncrement; if(--glideRemaining==0) pitch=targetPitch; }
        vibratoPhase+=smooth[VibratoRate]/sampleRate; vibratoPhase-=std::floor(vibratoPhase);
        double vibrato=std::sin(2*pi*vibratoPhase)*smoothWheel;
        double freq1=hz(pitch+12*smooth[Octave]+bend+vibrato),freq2=freq1*std::exp2(tuning(smooth[Tune2])/12);
        double fs=sampleRate*4;
        const double velocitySmoothing=1-std::exp(-1/(0.004*fs));
        for(int sub=0;sub<4;++sub) {
            smoothVelocity+=velocitySmoothing*(velocity-smoothVelocity);
            double fe=filterEnvelope.next(1/fs,smooth[Attack],smooth[Decay],smooth[Sustain],smooth[Release]);
            double ae=ampEnvelope.next(1/fs,smooth[Attack],2*smooth[Decay],ampSustain(smooth[Sustain]),smooth[Release]);
            double sensitivity=1-smooth[Velocity]+smooth[Velocity]*smoothVelocity;
            double phase1=osc1.phase,phase2=osc2.phase;
            const Values modulated=envelopeMods(effective,fe*sensitivity);
            double source1=osc1.next(freq1,fs,modulated,Wave1),source2=osc2.next(freq2,fs,modulated,Wave2);
            if(captureScopes){scopes[0].push(phase1,source1);scopes[1].push(phase2,source2);}
            double x=source1+effective[Level2]*source2;
            x=std::tanh(x*0.8);
            double dc=x-dcInput+std::exp(-2*pi*5/fs)*dcOutput; dcInput=x; dcOutput=dc;
            double cutoff=std::clamp(25*std::exp2(effective[Cutoff]*9+smooth[FilterAmount]*7*fe*sensitivity),20.0,std::min(20000.0,sampleRate*0.44));
            double g=std::tan(pi*cutoff/fs), k=2-1.97*(0.08+0.92*resonantValue);
            double a1=1/(1+g*(g+k));
            double v1=a1*(ic1+g*(dc-ic2)), v2=ic2+g*v1;
            ic1=4*std::tanh((2*v1-ic1)/4); ic2=4*std::tanh((2*v2-ic2)/4);
            decimator.push(std::tanh(v2*1.25)*ae*sensitivity*smooth[Volume]*1.4);
        }
        double y=decimator.output();
        double needed=std::abs(y)>ceiling?ceiling/std::abs(y):1;
        if(needed<limiterGain) limiterGain=needed;
        else limiterGain=std::min(needed,1-(1-limiterGain)*std::exp(-1/(0.06*sampleRate)));
        y*=limiterGain;
        if(!std::isfinite(y)) { allOff(true); limiterGain=1; return 0; }
        return std::clamp(y,-ceiling,ceiling);
    }
};
} // namespace syn
