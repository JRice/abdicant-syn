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
    Velocity, Glide, VibratoRate, AftertouchTarget, AftertouchDepth, Volume, NoiseCutoff, CrushEnv1, AltEnv1, NoiseEnv1, CrushEnv2, AltEnv2, NoiseEnv2, Octave, FilterAlgorithm, Count };
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
    {"Octave", "Oscillator 1",-2,1,0,true},
    {"Algorithm", "Filter",0,3,0,true}
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

// Internal rendering profiles for the existing Saw and Pulse selections.
inline constexpr int osc2Saw=7, osc2Pulse=8;
inline constexpr double osc2Duty=.485;
inline double wave(double p,int shape) {
    p-=std::floor(p);
    const auto tri=[](double x){ return 1-4*std::abs(x-0.5); };
    switch(shape) {
    case osc2Saw: {
        constexpr std::array<double,5> ramp{-1.,-.41,.12,.59,1.};
        const double position=4*p;
        const int segment=int(position);
        return ramp[segment]+(position-segment)*(ramp[segment+1]-ramp[segment]);
    }
    case osc2Pulse: return p<osc2Duty?1:-1;
    case 0: return tri(p);
    case 1: return 2*p-1;
    case 2: return p<0.5?1:-1;
    case 3: return p<0.5?1-std::abs(4*p-1):0;
    case 4: return p<0.5?(p<0.25?1:-1):tri(2*p-1);
    case 5: return std::clamp(1.7*tri(p),-1.0,1.0);
    default: { double x=1.6*tri(p)+0.25; return x>1?2-x:x< -1?-2-x:x; }
    }
}
// Piecewise-linear waveform edge data: value jumps and derivative jumps.
// Convolution with a unit-area triangular kernel yields compact polynomial
// BLEP/BLAMP corrections. Edges are positioned fractionally, including Alt's
// partial final cycle and Squish's tail. No additional audio delay is required.
struct WaveEdge { double phase,jump,slope; };
struct WaveEdges { std::array<WaveEdge,6> edge; int count; double startSlope; };
inline constexpr std::array<WaveEdges,9> waveEdges{{
    {{{{0,0,8},{.5,0,-8}}},2,4},
    {{{{0,-2,0}}},1,2},
    {{{{0,2,0},{.5,-2,0}}},2,0},
    {{{{0,0,4},{.25,0,-8},{.5,0,4}}},3,4},
    {{{{0,2,8},{.25,-2,0},{.5,0,8},{.75,0,-16}}},4,0},
    {{{{(1-1/1.7)/4,0,6.8},{(1+1/1.7)/4,0,-6.8},
        {(3-1/1.7)/4,0,-6.8},{(3+1/1.7)/4,0,6.8}}},4,0},
    {{{{0,0,-12.8},{.0546875,0,12.8},{.3671875,0,-12.8},
        {.5,0,12.8},{.6328125,0,-12.8},{.9453125,0,12.8}}},6,-6.4},
    {{{{0,-2,.72},{.25,0,-.24},{.5,0,-.24},{.75,0,-.24}}},4,2.36},
    {{{{0,2,0},{osc2Duty,-2,0}}},2,0}
}};
inline double edgeCorrection(double distance,double dt,double jump,double slope) {
    if(distance>.5)distance-=1;else if(distance<-.5)distance+=1;
    const double t=1-std::abs(distance)/dt;
    if(t<=0)return 0;
    return jump*(distance<0?.5:-.5)*t*t+slope*dt*t*t*t/6;
}
inline double analogueWave(double phase,double dt,double width,double cycles,int shape) {
    const double tail=(shape==1 || shape==3 || shape==osc2Saw)?0.:-1.;
    double result=phase<width?wave(phase/width*cycles,shape):tail;
    if(dt<=0)return result;
    const auto& table=waveEdges[shape];const double scale=cycles/width;
    const double end=std::nextafter(cycles,0.),endPhase=end-std::floor(end);
    double endSlope=table.startSlope;
    for(int j=0;j<table.count;++j)if(table.edge[j].phase>0 && table.edge[j].phase<=endPhase)endSlope+=table.edge[j].slope;
    const double endValue=wave(end,shape);
    const bool gap=width<1;
    result+=edgeCorrection(phase,dt,wave(0,shape)-(gap?tail:endValue),
                           scale*(table.startSlope-(gap?0:endSlope)));
    if(gap)result+=edgeCorrection(phase-width,dt,tail-endValue,-endSlope*scale);
    // Only visit slave cycles intersecting the correction window. Adjacent
    // master periods supply edges close to the wrap, without duplicating them.
    for(int image=-1;image<=1;++image) {
        const double lo=(phase+image-dt)*scale,hi=(phase+image+dt)*scale;
        const int first=std::max(0,int(std::floor(lo)));
        const int last=std::min(int(std::ceil(cycles))-1,int(std::floor(hi)));
        for(int cycle=first;cycle<=last;++cycle)for(int j=0;j<table.count;++j) {
            const auto& e=table.edge[j];const double position=cycle+e.phase;
            if(position>0 && position<cycles && position>=lo && position<=hi)
                result+=edgeCorrection(phase-position/scale,dt,e.jump,e.slope*scale);
        }
    }
    return result;
}
struct MemoryDrive {
    double fast=0,slow=0;
    static double bend(double x,double drive,double bias) {
        const double b=std::tanh(bias);
        return (std::tanh(drive*x+bias)-b)/(drive*(1-b*b));
    }
    double next(double input,double fs) {
        fast+=(input-fast)*(1-std::exp(-2*pi*4000/fs));
        slow+=(input-slow)*(1-std::exp(-2*pi*120/fs));
        // Complementary histories sum to the input at small signal levels.
        // Distinct nonlinear laws make transient and sustained responses differ.
        const double high=input-fast,mid=fast-slow,low=slow;
        return .6*bend(.8*input,1,.08)+.4*(bend(.8*high,.9,.05)
            +bend(.8*mid,1.25,-.08)+bend(.8*low,1.5,.12));
    }
};
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
        const double cycles=1+15*p[offset+2];
        const double amount=p[offset+1];
        const double hiddenStep=frequency*std::exp2(amount/12)/fs;
        int profile=shape;
        if(offset==Wave2 && shape==1)profile=osc2Saw;
        if(offset==Wave2 && shape==2)profile=osc2Pulse;
        double value=analogueWave(phase,frequency/fs,width,cycles,profile);
        if(shape==2) {
            const double blend=.5*std::min(1.,amount/.03);
            if(blend>0) value+=(analogueWave(hiddenPhase,hiddenStep,width,cycles,profile)-value)*blend;
        } else {
            value=crush(value,amount);
        }
        hiddenPhase+=hiddenStep; hiddenPhase-=std::floor(hiddenPhase);
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
// Oscillator edge correction runs before the nonlinear voice and decimation.
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
    std::array<float,128> samples{},pending{};
    double previousPhase=0,previousSample=0;
    bool ready=false,collecting=false;
    void push(double phase,double sample) {
        if(!ready){previousPhase=phase;previousSample=sample;ready=true;return;}
        double travel=phase-previousPhase;const bool wrapped=travel<0;if(wrapped)travel+=1;
        if(travel>0 && travel<0.5){
            if(wrapped){
                // Never interpolate through the cycle's discontinuity: retain
                // the last measured value on the left side and the first new
                // sample on the right. Publish only a completed captured cycle.
                for(int i=int(std::floor(previousPhase*128))+1;i<128;++i)pending[i]=float(previousSample);
                if(collecting)samples=pending;
                collecting=true;
                for(int i=0;i<=int(std::floor(phase*128));++i)pending[i]=float(sample);
            }else if(collecting){
                int first=int(std::floor(previousPhase*128))+1,last=int(std::floor(phase*128));
                for(int i=first;i<=last;++i){double t=(i/128.0-previousPhase)/travel;pending[i]=float(previousSample+(sample-previousSample)*t);}
            }
        }
        previousPhase=phase;previousSample=sample;
    }
};
// Character models, not component-calibrated hardware emulations. All branches
// run continuously so switching never exposes stale integrator state.
struct CharacterFilters {
    double wasp1=0,wasp2=0,waspDcInput=0,waspDcOutput=0;
    std::array<double,4> ladder{},ota{};
    std::array<double,4> weights{1,0,0,0};
    static double inverter(double x) {
        // Unequal positive/negative headroom; exactly zero at zero input.
        return (std::tanh(1.7*x+0.18)-std::tanh(0.18))/1.646;
    }
    // A bounded loss driven by the resonant signal itself. The squared control
    // concentrates the effect at the upper RES settings. Referencing stage
    // differences in the cascades keeps steady bass from driving the loss.
    static double resonantLoad(double band,double knee) {
        const double power=band*band;
        return power/(power+knee*knee);
    }
    static double fourPole(double input,double g,double resonance,
                           std::array<double,4>& state,bool isOta) {
        const double G=g/(1+g);
        const double load=resonantLoad(state[1]-state[3],isOta?.28:.18);
        const double compression=(isOta?.20:.48)*resonance*load;
        const double feedback=(isOta?4.05:4.15)*resonance*(1-compression);
        const double drive=isOta?1.45:1.8;
        // Solve current-sample global feedback, including stage saturation.
        // Monotone transfer gives a positive Newton denominator at every step.
        const auto cascade=[&](double u,std::array<double,4>& y,double& derivative) {
            derivative=1;
            for(int j=0;j<4;++j) {
                const double t=std::tanh(u*(isOta?0.65:1.0));
                const double shaped=isOta?t/0.65:t;
                derivative*=G*(1-t*t);
                y[j]=G*shaped+(1-G)*state[j];u=y[j];
            }
            return u;
        };
        double u=input*drive;std::array<double,4> y{};
        for(int n=0;n<6;++n) {
            double derivative;const double output=cascade(u,y,derivative);
            u-=(u+feedback*output-input*drive)/(1+feedback*derivative);
        }
        double derivative;const double output=cascade(u,y,derivative);
        for(int j=0;j<4;++j)state[j]=2*y[j]-state[j];
        // Partial bass compensation on ladder; stronger compensation on OTA.
        const double gain=(1+(isOta?0.72:0.40)*feedback)/drive;
        return isOta?inverter(output*gain*0.9)/0.9:output*gain;
    }
    double wasp(double input,double g,double res,double dcPole) {
        // Apply asymmetric coloration to audio, never to stored integrator state:
        // repeatedly biasing the state creates spurious DC attractors at low cutoff.
        const double loss=.95*res*res*resonantLoad(wasp1,.22);
        const double k=2-1.82*res+loss,a=1/(1+g*(g+k));
        const double v1=a*(wasp1+g*(inverter(input*1.2)-wasp2));
        const double v2=wasp2+g*v1;
        wasp1=2*v1-wasp1;
        wasp2=2*v2-wasp2;
        const double colored=inverter(v2*1.35)/1.2;
        // Asymmetric audio distortion still rectifies; remove its DC locally so
        // it cannot consume limiter headroom or affect the other algorithms.
        const double result=colored-waspDcInput+dcPole*waspDcOutput;
        waspDcInput=colored;waspDcOutput=result;
        return result;
    }
    double next(double input,double original,double g,double res,int mode,double fade,
                double sampleRate=192000) {
        const double waspOutput=wasp(input,g,res,std::exp(-2*pi*5/sampleRate));
        const std::array<double,4> output{original,waspOutput,
            fourPole(input,g,res,ladder,false),fourPole(input,g,res,ota,true)};
        double sum=0;
        for(int i=0;i<4;++i) {
            weights[i]+=fade*((i==mode?1.:0.)-weights[i]);
            sum+=weights[i]*output[i];
        }
        return sum;
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
    CharacterFilters filters;
    MemoryDrive memoryDrive;
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
        if(immediate) { filterEnvelope={}; ampEnvelope={}; ic1=ic2=0; filters={}; memoryDrive={}; decimator.history.fill(0); dcInput=dcOutput=0; }
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
            x=memoryDrive.next(x,fs);
            double dc=x-dcInput+std::exp(-2*pi*5/fs)*dcOutput; dcInput=x; dcOutput=dc;
            double cutoff=std::clamp(25*std::exp2(effective[Cutoff]*9+smooth[FilterAmount]*7*fe*sensitivity),20.0,std::min(20000.0,sampleRate*0.44));
            const double g=std::tan(pi*cutoff/fs);
            const double loss=.35*resonantValue*resonantValue*CharacterFilters::resonantLoad(ic1,.22);
            const double k=2-1.97*(0.08+0.92*resonantValue)+loss;
            double a1=1/(1+g*(g+k));
            double v1=a1*(ic1+g*(dc-ic2)), v2=ic2+g*v1;
            ic1=4*std::tanh((2*v1-ic1)/4); ic2=4*std::tanh((2*v2-ic2)/4);
            const double filtered=filters.next(dc,v2,g,resonantValue,int(smooth[FilterAlgorithm]),1-std::exp(-1/(0.005*fs)),fs);
            decimator.push(std::tanh(filtered*1.25)*ae*sensitivity*smooth[Volume]*1.4);
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
