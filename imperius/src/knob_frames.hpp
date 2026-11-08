#pragma once
#include <gdiplus.h>
#include <array>
#include <memory>
#include <cmath>
#include <algorithm>

namespace imperius::ui {
// The metal, bevel normals and studio illumination stay in panel coordinates.
// Only the skirt ribs and ivory pointer move. Frames are generated on the UI thread.
class KnobFrames {
    static constexpr int count=257;
    std::array<std::array<std::unique_ptr<Gdiplus::Bitmap>,count>,2> frames;
    static std::unique_ptr<Gdiplus::Bitmap> render(bool large,int frame) {
        using namespace Gdiplus;
        const int size=large?224:112;
        auto result=std::make_unique<Bitmap>(size,size,PixelFormat32bppARGB);
        Rect rect(0,0,size,size);BitmapData data{};
        if(result->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppARGB,&data)!=Ok)return result;
        const double angle=((large?-90:-135)+(large?180.:270.)*frame/(count-1))*pi/180;
        for(int y=0;y<size;++y) {
            auto row=reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(data.Scan0)+y*data.Stride);
            for(int x=0;x<size;++x) {
                const double nx=(x+.5-size*.5)/(size*.495),ny=(y+.5-size*.5)/(size*.495);
                const double r=std::hypot(nx,ny),alpha=std::clamp((1-r)*size*.495,0.,1.);
                if(alpha==0) { row[x]=0;continue; }
                double theta=std::atan2(nx,-ny),brightness;
                if(r<.595) {
                    // Radial brushed-aluminum cone, with fixed specular lobes.
                    brightness=174+53*std::cos(3*theta+.4)-26*ny+12*std::cos(theta+.5);
                    brightness+=2*std::sin(r*size*17)+1.2*std::sin(x*19.1+y*7.3);
                    if(r>.55)brightness*=.78+.22*(.595-r)/.045;
                } else if(r<.66) {
                    brightness=18+70*std::pow(std::max(0.,std::cos(theta+.8)),6);
                    if(r>.645)brightness+=60;
                } else {
                    double rib=.65+.35*std::pow(.5+.5*std::cos((theta-angle)*(large?76:48)),2);
                    brightness=(20+18*std::max(0.,-nx*.45-ny*.6))*rib;
                    if(r<.72)brightness+=12;
                    if(r>.96)brightness*=.55;
                }
                double along=nx*std::sin(angle)-ny*std::cos(angle);
                double across=nx*std::cos(angle)+ny*std::sin(angle);
                double pointer=std::clamp((.027-std::hypot(across,along-std::clamp(along,.76,.95)))*size*.495+.5,0.,1.);
                double red=brightness*(1-pointer)+244*pointer;
                double green=brightness*(1-pointer)+237*pointer;
                double blue=brightness*.97*(1-pointer)+216*pointer;
                auto byte=[](double v){return uint32_t(std::clamp(v,0.,255.));};
                row[x]=(byte(alpha*255)<<24)|(byte(red)<<16)|(byte(green)<<8)|byte(blue);
            }
        }
        result->UnlockBits(&data);return result;
    }
public:
    Gdiplus::Bitmap* get(bool large,double position) {
        int frame=int(std::round(std::clamp(position,0.,1.)*(count-1)));
        auto& p=frames[large?1:0][frame];if(!p)p=render(large,frame);return p.get();
    }
};
}
