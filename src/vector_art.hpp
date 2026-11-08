#pragma once
#include <gdiplus.h>
#include <cmath>
#include <algorithm>
#include <cstdint>

// Vector-only artwork. Paths and gradients are rendered at the host's pixel
// scale; no source bitmaps, sampled textures, or rotary image frames are used.
namespace ui::art {
using namespace Gdiplus;
inline void rounded(GraphicsPath& p,float x,float y,float w,float h,float r){
    r=std::min(r,std::min(w,h)*.5f);
    float d=2*r;p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);
    p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();
}
inline void plate(Graphics& g,float x,float y,float w,float h,float r,Color a,Color b){
    GraphicsPath p;rounded(p,x,y,w,h,r);LinearGradientBrush brush(PointF(x,y),PointF(x+w,y+h),a,b);g.FillPath(&brush,&p);
}
inline void label(Graphics& g,const wchar_t* s,float x,float y,float w,float size=29){
    FontFamily family(L"Arial");Font font(&family,size,FontStyleRegular,UnitPixel);
    SolidBrush ink(Color(255,233,225,207));StringFormat f;f.SetAlignment(StringAlignmentCenter);f.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(s,-1,&font,RectF(x-w/2,y-22,w,44),&f,&ink);
}
inline void button(Graphics& g,float x,float y,float diameter){
    float r=diameter/2;SolidBrush shadow(Color(110,0,0,0));g.FillEllipse(&shadow,x-r+1,y-r+5,diameter+3,diameter+3);
    LinearGradientBrush rim(PointF(x-r,y-r),PointF(x+r,y+r),Color(255,144,146,148),Color(255,15,18,24));
    g.FillEllipse(&rim,x-r,y-r,diameter,diameter);
    SolidBrush black(Color(255,8,11,17));g.FillEllipse(&black,x-r+2,y-r+2,diameter-4,diameter-4);
    LinearGradientBrush face(PointF(x-r,y-r),PointF(x+r,y+r),Color(255,61,65,70),Color(255,25,29,35));
    g.FillEllipse(&face,x-r+5,y-r+5,diameter-10,diameter-10);
}
inline void well(Graphics& g,float x,float y,float w,float h,float r){
    plate(g,x-2,y-1,w+4,h+4,r+2,Color(255,11,14,20),Color(255,130,132,129));
    plate(g,x,y,w,h,r,Color(255,4,7,11),Color(255,32,36,42));
    GraphicsPath p;rounded(p,x+2,y+2,w-4,h-4,r-1);Pen edge(Color(110,0,0,0),3);g.DrawPath(&edge,&p);
}
inline void track(Graphics& g,float x,float y,float h){well(g,x-9,y-18,18,h+36,9);}
inline void mods(Graphics& g,float crush,float alt,float noise){
    Pen ink(Color(255,233,225,207),3);ink.SetLineJoin(LineJoinRound);
    PointF stairs[]={{crush-17,359},{crush-7,359},{crush-7,368},{crush+3,368},{crush+3,377},{crush+13,377},{crush+13,388}};g.DrawLines(&ink,stairs,6);
    for(int i=0;i<5;++i){float h=32*std::exp(-i*.5f);g.DrawLine(&ink,alt-17+i*8,373-h/2,alt-17+i*8,373+h/2);}
    SolidBrush dots(Color(255,233,225,207));
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){float x=noise-18+col*12+(row%2)*3;float y=357+row*10;g.FillEllipse(&dots,x,y,2.6f,2.6f);}
}
inline void squish(Graphics& g,float x){
    Pen ink(Color(255,233,225,207),3);ink.SetLineJoin(LineJoinRound);
    PointF hour[]={{x-9,192},{x+9,192},{x-9,216},{x+9,216},{x-9,192}};g.DrawLines(&ink,hour,5);
    g.DrawLine(&ink,x-29,373.f,x-7,373.f);g.DrawLine(&ink,x+7,373.f,x+29,373.f);
    g.DrawLine(&ink,x-17,364.f,x-7,373.f);g.DrawLine(&ink,x-17,382.f,x-7,373.f);
    g.DrawLine(&ink,x+17,364.f,x+7,373.f);g.DrawLine(&ink,x+17,382.f,x+7,373.f);
}
inline void panel(Graphics& g){
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(255,12,16,25));
    plate(g,2,2,1631,958,30,Color(255,160,162,170),Color(255,28,32,43));
    plate(g,5,5,1625,952,28,Color(255,38,44,60),Color(255,6,10,19));
    plate(g,10,10,1615,942,25,Color(255,102,109,128),Color(255,26,31,45));
    plate(g,13,14,1609,934,23,Color(255,55,63,85),Color(255,32,40,61));
    auto state=g.Save();GraphicsPath face;rounded(face,14,15,1607,932,22);g.SetClip(&face,CombineModeIntersect);
    // Sparse deterministic vector stippling suggests powder-coated metal.
    uint32_t seed=0x7193ab;auto random=[&](){seed=1664525*seed+1013904223;return float(seed&0xffffff)/16777215.f;};
    SolidBrush light(Color(9,210,219,238)),dark(Color(10,4,9,20));
    for(int i=0;i<19000;++i){float x=15+random()*1605,y=16+random()*930;g.FillEllipse(i%2?&light:&dark,x,y,.7f,.7f);}
    g.Restore(state);
    track(g,105,108,260);track(g,871,108,260);
    for(float x:{435.f,515.f,588.f,1250.f,1326.f,1400.f})track(g,x,108,214);
    for(float x:{468.f,710.f,803.f,895.f,988.f})track(g,x,548,241);
    well(g,242,102,116,100,12);well(g,1057,102,116,100,12);
    button(g,300,246,39);button(g,1115,246,39);button(g,1270,775,39);
    well(g,1320,744,241,58,8);button(g,1563,862,50);
    Pen ink(Color(255,233,225,207),3);ink.SetStartCap(LineCapRound);ink.SetEndCap(LineCapRound);
    for(float y:{854.f,862.f,870.f})g.DrawLine(&ink,1551.f,y,1575.f,y);
    Pen dotted(Color(150,225,222,207),2);dotted.SetDashStyle(DashStyleDot);g.DrawLine(&dotted,805.f,75.f,805.f,397.f);
    for(float x:{205.f,1013.f,1027.f}){g.DrawLine(&ink,x,97.f,x,125.f);g.DrawLine(&ink,x-5,97.f,x+5,97.f);g.DrawLine(&ink,x-5,125.f,x+5,125.f);}
    label(g,L"ALG",364,617,95);label(g,L"VOL",105,418,110);label(g,L"CUT",200,850,110);label(g,L"RES",364,850,95);label(g,L"ENV",468,850,95);
    label(g,L"A",710,850,60);label(g,L"D",803,850,60);label(g,L"S",895,850,60);label(g,L"R",988,850,60);
    label(g,L"PORT",1180,665,110);label(g,L"VEL",1346,665,95);label(g,L"VIB",1507,665,95);label(g,L"ATOUCH",1320,850,215);
    mods(g,435,515,588);mods(g,1250,1326,1400);squish(g,688);squish(g,1507);
    label(g,L"\x266d",1066,315,35,35);label(g,L"\x266f",1164,315,35,35);g.DrawLine(&ink,1115.f,300.f,1115.f,311.f);
}
inline void knob(Graphics& g,float x,float y,float w,float h,int style,double position){
    auto saved=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(w/2,h/2);
    SolidBrush shadow(Color(95,0,0,0));g.FillEllipse(&shadow,-.99f,-.92f,2.05f,2.07f);
    LinearGradientBrush rim(PointF(-1,-1),PointF(1,1),Color(255,64,72,89),Color(255,5,9,16));g.FillEllipse(&rim,-1.f,-1.f,2.f,2.f);
    bool blue=style==0;
    LinearGradientBrush bevel(PointF(-.8f,-1),PointF(.8f,1),blue?Color(255,73,93,137):Color(255,45,50,59),blue?Color(255,18,28,48):Color(255,9,13,20));
    g.FillEllipse(&bevel,-.96f,-.96f,1.92f,1.92f);
    double angle=(-135+270*position)*3.14159265358979323846/180;int ribs=blue?64:style==1?40:20;
    for(int i=0;i<ribs;++i){double a=angle+i*2*3.14159265358979323846/ribs;float sx=float(std::sin(a)),sy=float(-std::cos(a));
        Pen groove(Color(85,3,8,18),blue?.025f:.03f);g.DrawLine(&groove,.84f*sx,.84f*sy,.94f*sx,.94f*sy);}
    LinearGradientBrush face(PointF(-.8f,-.9f),PointF(.7f,.9f),blue?Color(255,58,76,116):Color(255,35,40,47),blue?Color(255,33,45,73):Color(255,19,24,31));
    g.FillEllipse(&face,-.82f,-.82f,1.64f,1.64f);
    Pen lip(Color(35,180,192,213),.012f);g.DrawArc(&lip,-.82f,-.82f,1.64f,1.64f,195,130);
    float sx=float(std::sin(angle)),sy=float(-std::cos(angle));
    Pen pointer(Color(255,239,230,209),blue?.039f:style==1?.06f:.065f);pointer.SetStartCap(LineCapRound);pointer.SetEndCap(LineCapRound);
    g.DrawLine(&pointer,.3f*sx,.3f*sy,.73f*sx,.73f*sy);g.Restore(saved);
}
}
