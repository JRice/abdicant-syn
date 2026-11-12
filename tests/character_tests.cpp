#include "../src/synth.hpp"
#include <complex>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double raw(double p,double width,double cycles,int shape){
    p-=std::floor(p);return p<width?syn::wave(p/width*cycles,shape):(shape==1||shape==3||shape==syn::osc2Saw?0.:-1.);
}
void fft(std::vector<std::complex<double>>& a){
    const int n=int(a.size());
    for(int i=1,j=0;i<n;++i){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
    for(int len=2;len<=n;len*=2){auto root=std::polar(1.,-2*syn::pi/len);
        for(int i=0;i<n;i+=len){std::complex<double>w=1;for(int j=0;j<len/2;++j){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=root;}}}
}
double aliasRatio(int shape,bool corrected){
    constexpr int n=4096,bin=317;constexpr double step=double(bin)/n;
    std::vector<std::complex<double>> a(n);
    for(int i=0;i<n;++i){double p=i*step;p-=std::floor(p);a[i]=corrected?syn::analogueWave(p,step,1,1,shape):syn::wave(p,shape);}
    fft(a);double aliases=0;
    for(int i=1;i<n/2;++i)if(i%bin!=0)aliases+=std::norm(a[i]);
    return aliases/std::norm(a[bin]);
}
void tests(){
    // Independent numerical convolution checks all knots, including partial Alt
    // cycles, wrap and Squish edges. This catches sign/height/slope mistakes.
    double worst=0;
    for(int shape=0;shape<int(syn::waveEdges.size());++shape)for(double width:{1.,.63,.08})for(double cycles:{1.,2.37,16.})
        for(double phase:{0.,.013,.249,.499,.632,.999}){
            constexpr double dt=.017;constexpr int points=12000;double reference=0;
            for(int j=0;j<points;++j){double u=-1+(j+.5)*2/points;reference+=raw(phase+u*dt,width,cycles,shape)*(1-std::abs(u))*2/points;}
            const double value=syn::analogueWave(phase,dt,width,cycles,shape);
            worst=std::max(worst,std::abs(value-reference));
            require(std::abs(value-reference)<.002,"Edge correction disagrees with numerical convolution");
        }
    std::cout<<"Maximum edge/convolution error: "<<worst<<'\n';
    for(int shape:{0,1,2,4,5,6,syn::osc2Saw,syn::osc2Pulse}){
        const double before=aliasRatio(shape,false),after=aliasRatio(shape,true);
        std::cout<<"Wave "<<shape<<" alias/fundamental dB: "<<10*std::log10(before)<<" -> "<<10*std::log10(after)<<'\n';
        require(after<before*.3,"Transition corrections must reduce oscillator alias energy by over 5 dB");
    }
    syn::MemoryDrive cold,warm;
    for(int i=0;i<48000;++i)warm.next(.7,48000);
    require(std::abs(cold.next(.2,48000)-warm.next(.2,48000))>.005,"Drive must respond to signal history");
    for(int i=0;i<48000;++i)warm.next(0,48000);
    require(std::abs(warm.next(0,48000))<1e-10,"Drive must settle to zero after DC excitation");
    syn::MemoryDrive small;double inputEnergy=0,outputEnergy=0;
    for(int i=0;i<48000;++i){double x=.0001*std::sin(2*syn::pi*997*i/48000),y=small.next(x,48000);inputEnergy+=x*x;outputEnergy+=y*y;}
    require(std::abs(std::sqrt(outputEnergy/inputEnergy)-.8)<1e-4,"Drive preserves small-signal mixer gain");
    for(double rate:{8000.,44100.,48000.,96000.,384000.})for(int mode=0;mode<4;++mode){
        syn::Engine e;e.reset(rate);e.values[syn::FilterAlgorithm]=mode;
        e.values[syn::Alt1]=1;e.values[syn::SquishDepth1]=1;e.values[syn::CrushEnv2]=1;e.values[syn::Resonance]=3;e.noteOn(108,1);
        for(int n=0;n<4000;++n){e.values[syn::Wave1]=(n/500)%7;
            const double y=e.next();require(std::isfinite(y)&&std::abs(y)<=syn::ceiling,"Prototype stress must remain finite and limited");}
        e.allOff(true);for(int n=0;n<100;++n)require(e.next()==0,"Hard silence must clear character histories");
    }
}
int main(){
    try { tests(); std::cout<<"PASS: oscillator edge correction, memory drive and voice stress\n"; }
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
