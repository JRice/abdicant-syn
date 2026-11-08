#pragma once
#include <array>
#include <memory>
#include <cmath>
#include <algorithm>
#include <gdiplus.h>

namespace ui {
// Rasterized physical knobs: the body normals and studio light stay fixed in
// panel space. Only the indicator and molded ribs turn between renderings.
class KnobFrames {
    static constexpr int count=129;
    std::array<std::array<std::unique_ptr<Gdiplus::Bitmap>,count>,3> frames;
    static std::unique_ptr<Gdiplus::Bitmap> render(int style,int frame) {
        using namespace Gdiplus;
        const int size=style==0?256:style==1?112:80;
        auto bitmap=std::make_unique<Bitmap>(size,size,PixelFormat32bppARGB);
        Rect rect(0,0,size,size);BitmapData data{};
        if(bitmap->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppARGB,&data)!=Ok)return bitmap;
        const double angle=(-135+270.0*frame/(count-1))*3.14159265358979323846/180;
        const double sa=std::sin(angle),ca=std::cos(angle),radius=size*.49;
        for(int py=0;py<size;++py) {
            auto* row=reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(data.Scan0)+py*data.Stride);
            for(int px=0;px<size;++px) {
                const double x=(px+.5-size*.5)/radius,y=(py+.5-size*.5)/radius,r=std::hypot(x,y);
                const double alpha=std::clamp((1-r)*radius,0.0,1.0);
                if(alpha==0){row[px]=0;continue;}
                const double theta=std::atan2(x,-y);
                const double rib=std::cos((theta-angle)*(style==0?64:style==1?40:20));
                double slope=r<.78?.16*r:r<.9?.25+(r-.78)*4:.82;
                const double nx=r>0?slope*x/r:0,ny=r>0?slope*y/r:0,nz=std::sqrt(1-slope*slope);
                const double diffuse=std::max(0.0,-.45*nx-.6*ny+.66*nz);
                const double spec=std::pow(std::max(0.0,-.245*nx-.327*ny+.913*nz),32);
                const double ribs=r>.81?.86+.14*rib:1;
                const double rim=r>.96?.38:1;
                const double shade=(.42+.72*diffuse)*ribs*rim;
                const double base[3]={style==0?50.:29.,style==0?66.:32.,style==0?103.:39.};
                // Slight molded grain is fixed in panel space, never flashing.
                const double grain=std::sin(px*19.31+py*73.17)*.65;
                double rgb[3];for(int k=0;k<3;++k)rgb[k]=base[k]*shade+spec*(style==0?34:22)+grain;
                const double along=x*sa-y*ca,across=x*ca+y*sa;
                const double pointerRadius=style==0?.021:style==1?.035:.03;
                const double distance=std::hypot(across,along-std::clamp(along,.30,.73));
                const double ink=std::clamp((pointerRadius-distance)*radius+.5,0.0,1.0);
                for(int k=0;k<3;++k)rgb[k]=rgb[k]*(1-ink)+(k==0?239:k==1?230:209)*ink;
                auto byte=[](double v){return static_cast<unsigned int>(std::clamp(v,0.0,255.0));};
                row[px]=(byte(alpha*255)<<24)|(byte(rgb[0])<<16)|(byte(rgb[1])<<8)|byte(rgb[2]);
            }
        }
        bitmap->UnlockBits(&data);return bitmap;
    }
public:
    // Lazy caching avoids rendering unused positions or allocating on audio threads.
    Gdiplus::Bitmap* get(int style,double position) {
        int frame=int(std::round(std::clamp(position,0.0,1.0)*(count-1)));
        auto& result=frames[style][frame];if(!result)result=render(style,frame);
        return result.get();
    }
};
}
