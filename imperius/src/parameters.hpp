#pragma once
#include "engine.hpp"

namespace imperius {
enum Param {
    Octave, Harmonics, Fold, Bias, BasicTune, PhaseMod, HarmonicMod, FoldMod,
    ComplexLevel, BasicLevel, SubLevel, Cutoff, Resonance, FilterEnv, Highpass,
    HarmonicEnv, FoldEnv, HarmonicLfo, FoldLfo, Volume,
    AmpAttack, AmpDecay, AmpSustain, AmpRelease, ModAttack, ModDecay, ModSustain, ModRelease,
    Lfo1Rate, Lfo1Shape, Lfo1Sync, Lfo2Rate, Lfo2Shape, Lfo2Sync,
    ComplexShape, BasicWave, Count // Append only: preserve existing host IDs.
};
struct Definition { const char* name; const char* group; double min,max,initial; bool stepped=false; };
inline const std::array<Definition,Count> definitions{{
    {"OSC: Octave","Generators",-1,1,0,true},
    {"OSC: Harmonics","Generators",0,1,.2}, {"FOLD: Amount","Folder",0,1,.15},
    {"FOLD: Bias","Folder",-1,1,0}, {"BASIC: Tune","Generators",-24,24,0},
    {"OSC: Phase modulation","Audio-rate modulation",0,1,0},
    {"OSC: Harmonic modulation","Audio-rate modulation",-1,1,0},
    {"FOLD: Audio-rate modulation","Audio-rate modulation",-1,1,0},
    {"MIX: Complex","Mixer",0,1,.8}, {"MIX: Basic","Mixer",0,1,.15}, {"MIX: Sub","Mixer",0,1,.15},
    {"FILTER: Cutoff","Filter",0,1,std::log(350.)/std::log(1000.)},
    {"FILTER: Resonance","Filter",0,1,.15}, {"FILTER: Envelope","Filter",-1,1,0},
    {"FILTER: Mode","Filter",0,1,0,true},
    {"OSC: Envelope amount","Modulation",-1,1,0}, {"FOLD: Envelope amount","Modulation",-1,1,0},
    {"OSC: LFO 1 amount","Modulation",-1,1,0}, {"FOLD: LFO 2 amount","Modulation",-1,1,0},
    {"OUT: Volume","Output",0,1,.65},
    {"AMP: Attack","Amp envelope",.001,5,.008}, {"AMP: Decay","Amp envelope",.005,5,.35},
    {"AMP: Sustain","Amp envelope",0,1,.65}, {"AMP: Release","Amp envelope",.005,5,.4},
    {"MOD: Attack","Mod envelope",.001,5,.008}, {"MOD: Decay","Mod envelope",.005,5,.5},
    {"MOD: Sustain","Mod envelope",0,1,.25}, {"MOD: Release","Mod envelope",.005,5,.5},
    {"LFO1: Rate","LFO 1",0,1,std::log(50.)/std::log(10000.)},
    {"LFO1: Shape","LFO 1",0,1,0}, {"LFO1: Sync","LFO 1",0,1,0,true},
    {"LFO2: Rate","LFO 2",0,1,std::log(50.)/std::log(10000.)},
    {"LFO2: Shape","LFO 2",0,1,0}, {"LFO2: Sync","LFO 2",0,1,0,true},
    {"OSC: Shape","Generators",-1,1,0}, {"BASIC: Wave","Generators",0,2,1,true}
}};
using Values=std::array<double,Count>;
inline Values defaults() { Values v{}; for(int i=0;i<Count;++i) v[i]=definitions[i].initial; return v; }
inline double valid(int i,double v) {
    const auto& d=definitions[i];
    if(!std::isfinite(v)) return d.initial;
    v=std::clamp(v,d.min,d.max); return d.stepped?std::round(v):v;
}
inline void apply(Controls& p,const Values& v) {
    p.octave=int(v[Octave]); p.harmonics=v[Harmonics]; p.fold=v[Fold]; p.bias=v[Bias];
    p.basicTune=v[BasicTune]; p.fm=v[PhaseMod]; p.harmonicMod=v[HarmonicMod]; p.foldMod=v[FoldMod];
    p.complexShape=v[ComplexShape];p.basicWave=int(v[BasicWave]);
    p.complexLevel=v[ComplexLevel]; p.basicLevel=v[BasicLevel]; p.subLevel=v[SubLevel];
    p.cutoff=20*std::pow(1000.,v[Cutoff]); p.resonance=v[Resonance];
    p.filterEnv=v[FilterEnv]; p.highpass=v[Highpass]>=.5;
    p.harmonicEnv=v[HarmonicEnv]; p.foldEnv=v[FoldEnv];
    p.harmonicLfo=v[HarmonicLfo]; p.foldLfo=v[FoldLfo]; p.volume=v[Volume];
    p.amp={v[AmpAttack],v[AmpDecay],v[AmpSustain],v[AmpRelease]};
    p.mod={v[ModAttack],v[ModDecay],v[ModSustain],v[ModRelease]};
    for(int n=0;n<2;++n) {
        int offset=n?Lfo2Rate:Lfo1Rate;
        p.lfos[n].sync=v[offset+2]>=.5;
        p.lfos[n].rate=p.lfos[n].sync?std::round(v[offset]*9):.01*std::pow(10000.,v[offset]);
        p.lfos[n].shape=v[offset+1];
    }
}
} // namespace imperius
