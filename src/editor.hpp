#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "synth.hpp"
#include "vector_art.hpp"

namespace ui {
inline constexpr uint32_t preferredWidth=1300,preferredHeight=765;
struct Hooks {
    void* context{};
    double (*value)(void*,int){};
    bool (*begin)(void*,int){};
    void (*change)(void*,int,double){};
    void (*end)(void*,int){};
    void (*format)(void*,int,double,char*,size_t){};
    void (*scope)(void*,int,float*,size_t){};
};
enum class Kind { Slider, Knob, Wave, Resonance, Target, Options, Noise, EnvDepth, Octave };
struct Control { int id; Kind kind; float x,y,w,h; };
inline const std::array<Control,36> controls {{
    {syn::Volume,Kind::Slider,105,108,0,260},
    {syn::Wave1,Kind::Wave,300,153,116,110},
    {syn::Reduce1,Kind::Slider,435,108,0,214}, {syn::Alt1,Kind::Slider,515,108,0,214}, {syn::Noise1,Kind::Slider,588,108,0,214},
    {syn::SquishRate1,Kind::Knob,688,146,78,78}, {syn::SquishDepth1,Kind::Knob,688,280,78,78},
    {syn::Level2,Kind::Slider,871,108,0,260}, {syn::Wave2,Kind::Wave,1115,153,116,110},
    {syn::Tune2,Kind::Knob,1115,362,88,88},
    {syn::Reduce2,Kind::Slider,1250,108,0,214}, {syn::Alt2,Kind::Slider,1326,108,0,214}, {syn::Noise2,Kind::Slider,1400,108,0,214},
    {syn::SquishRate2,Kind::Knob,1507,146,78,78}, {syn::SquishDepth2,Kind::Knob,1507,280,78,78},
    {syn::Cutoff,Kind::Knob,200,650,226,240}, {syn::Resonance,Kind::Resonance,364,751,78,78},
    {syn::FilterAmount,Kind::Slider,468,548,0,241},
    {syn::Attack,Kind::Slider,710,548,0,241}, {syn::Decay,Kind::Slider,803,548,0,241},
    {syn::Sustain,Kind::Slider,895,548,0,241}, {syn::Release,Kind::Slider,988,548,0,241},
    {syn::Glide,Kind::Knob,1180,592,86,86}, {syn::Velocity,Kind::Knob,1346,592,86,86},
    {syn::VibratoRate,Kind::Knob,1507,592,86,86},
    {syn::AftertouchDepth,Kind::Knob,1180,774,53,53}, {syn::AftertouchTarget,Kind::Target,1441,774,237,58},
    {syn::CrushEnv1,Kind::EnvDepth,435,373,60,48}, {syn::AltEnv1,Kind::EnvDepth,515,373,60,48},
    {syn::NoiseEnv1,Kind::EnvDepth,588,373,60,48}, {syn::CrushEnv2,Kind::EnvDepth,1250,373,60,48},
    {syn::AltEnv2,Kind::EnvDepth,1326,373,60,48}, {syn::NoiseEnv2,Kind::EnvDepth,1400,373,60,48},
    {syn::Octave,Kind::Octave,300,350,78,78},
    {-1,Kind::Options,1563,862,50,50}, {syn::NoiseCutoff,Kind::Noise,1390,681,250,40}
}};
inline bool logarithmic(int id) { return id==syn::Attack || id==syn::Decay || id==syn::Release || id==syn::SquishRate1 || id==syn::SquishRate2 || id==syn::NoiseCutoff; }
inline double normalized(int id,double value) {
    const auto& d=syn::definitions[id]; value=syn::validValue(id,value);
    return logarithmic(id)?std::log(value/d.min)/std::log(d.max/d.min):(value-d.min)/(d.max-d.min);
}
inline double plain(int id,double value) {
    const auto& d=syn::definitions[id]; value=std::clamp(value,0.0,1.0);
    return syn::validValue(id,logarithmic(id)?d.min*std::pow(d.max/d.min,value):d.min+value*(d.max-d.min));
}
class Editor {
    Hooks hooks;
    HWND window{}; HINSTANCE instance{}; std::wstring className;
    ULONG_PTR gdiplusToken{};
    std::unique_ptr<Gdiplus::Bitmap> background;
    uint32_t width=preferredWidth,height=preferredHeight;
    int hover=-1,drag=-1,focus=-1;
    float lastX=0,lastY=0; double dragValue=0;
    bool options=false,tracking=false;
    syn::Values observed=syn::defaults();
    static constexpr float designWidth=1635,designHeight=962;

    static std::wstring wide(const char* text) {
        int n=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0); if(n<1) return L"";
        std::wstring out(size_t(n),L'\0'); MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),n); out.pop_back(); return out;
    }
    void text(Gdiplus::Graphics& g,const std::wstring& s,float x,float y,float w,float h,float size=25,bool centered=true) {
        // GenericSansSerif caches a process-static native family across GDI+ shutdown.
        // Own the family locally so reopening the editor never reuses a stale handle.
        Gdiplus::FontFamily family(L"Arial");
        Gdiplus::Font font(&family,size,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
        Gdiplus::SolidBrush ink(Gdiplus::Color(255,233,225,207)); Gdiplus::StringFormat format;
        format.SetAlignment(centered?Gdiplus::StringAlignmentCenter:Gdiplus::StringAlignmentNear);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter); format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        g.DrawString(s.c_str(),-1,&font,Gdiplus::RectF(x,y,w,h),&format,&ink);
    }
    void roundRect(Gdiplus::Graphics& g,Gdiplus::Brush& brush,float x,float y,float w,float h,float radius) {
        Gdiplus::GraphicsPath path; float d=radius*2;
        path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);
        path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();g.FillPath(&brush,&path);
    }
    void drawKnob(Gdiplus::Graphics& g,const Control& c,double position) {
        art::knob(g,c.x,c.y,c.w,c.h,c.id==syn::Cutoff?0:c.id==syn::AftertouchDepth?2:1,position);
    }

    void drawSlider(Gdiplus::Graphics& g,const Control& c,double position) {
        using namespace Gdiplus;
        const float y=c.y+c.h*float(1-position),w=c.id==syn::Volume||c.id==syn::Level2?70.f:60.f,h=35;
        for(int i=0;i<6;++i) if(c.id==syn::modTargets[i]) {
            const double depth=hooks.value(hooks.context,syn::CrushEnv1+i);
            const float end=c.y+c.h*float(1-std::clamp(position+depth,0.0,1.0));
            Pen range(Color(255,255,255,255),2*designWidth/width);
            g.DrawLine(&range,c.x,y,c.x,end);
        }
        SolidBrush shadow(Color(100,0,0,0));roundRect(g,shadow,c.x-w/2-3,y-h/2+7,w+6,h+5,6);
        LinearGradientBrush cap(PointF(c.x,y-h/2),PointF(c.x,y+h/2),Color(255,71,73,75),Color(255,18,21,25));
        roundRect(g,cap,c.x-w/2,y-h/2,w,h,5);
        Pen highlight(Color(130,180,180,175),1.4f);g.DrawLine(&highlight,c.x-w/2+5,y-h/2+2,c.x+w/2-5,y-h/2+2);
        Pen line(Color(255,241,230,209),3);line.SetStartCap(LineCapRound);line.SetEndCap(LineCapRound);g.DrawLine(&line,c.x,y-12,c.x,y+9);
    }
    void drawScope(Gdiplus::Graphics& g,int osc,float x) {
        using namespace Gdiplus;
        std::array<float,128> samples{}; hooks.scope(hooks.context,osc,samples.data(),samples.size());
        auto state=g.Save();g.SetClip(RectF(x-52,109,104,85),CombineModeIntersect);
        std::array<PointF,128> points{};
        for(size_t i=0;i<samples.size();++i)points[i]=PointF(x-49+98*float(i)/127,153-33*std::clamp(samples[i],-1.2f,1.2f));
        Pen glow(Color(28,237,224,197),5);Pen line(Color(255,237,224,197),2.2f);
        g.DrawLines(&glow,points.data(),int(points.size()));g.DrawLines(&line,points.data(),int(points.size()));g.Restore(state);
    }
    void paint(HDC dc) {
        using namespace Gdiplus;
        if(!width || !height) return;
        if(!background || background->GetWidth()!=width || background->GetHeight()!=height) {
            background=std::make_unique<Bitmap>(width,height,PixelFormat32bppPARGB);Graphics bg(background.get());
            bg.ScaleTransform(width/designWidth,height/designHeight);art::panel(bg);
        }
        Bitmap buffer(width,height,PixelFormat32bppPARGB);Graphics g(&buffer);g.DrawImage(background.get(),0,0);
        g.ScaleTransform(width/designWidth,height/designHeight);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        for(size_t i=0;i<controls.size();++i) {
            const auto& c=controls[i];if(c.id<0)continue;double v=hooks.value(hooks.context,c.id);
            if(c.kind==Kind::Knob)drawKnob(g,c,normalized(c.id,v));
            if(c.kind==Kind::Slider)drawSlider(g,c,normalized(c.id,v));
            if(c.kind==Kind::Resonance || c.kind==Kind::Octave) {
                art::button(g,c.x,c.y,78);
                int n=int(v)+(c.kind==Kind::Octave?2:0);const PointF pips[]={PointF(c.x,c.y-15),PointF(c.x-14,c.y+8),PointF(c.x+14,c.y+8)};
                for(int j=0;j<3;++j){Pen edge(Color(255,231,222,202),2);SolidBrush fill(j<n?Color(255,236,225,202):Color(255,28,31,36));g.FillEllipse(&fill,pips[j].X-7,pips[j].Y-7,14.f,14.f);g.DrawEllipse(&edge,pips[j].X-7,pips[j].Y-7,14.f,14.f);}
            }
            if(c.kind==Kind::Target) {
                const wchar_t* labels[]={L"CUTOFF",L"RESONANCE",L"OSC II MIX",L"OSC I ALT",L"OSC II ALT",L"I + II ALT"};
                text(g,labels[std::clamp(int(v),0,5)],1328,748,223,48,26);
            }
        }
        drawScope(g,0,300);drawScope(g,1,1115);
        if(options) {
            SolidBrush shadow(Color(120,0,0,0));roundRect(g,shadow,1197,541,387,271,16);
            SolidBrush panel(Color(255,22,29,43));roundRect(g,panel,1189,532,387,271,12);
            text(g,L"OPTIONS",1210,548,345,35,24,false);
            text(g,L"Noise cutoff · both oscillators",1210,592,345,38,22,false);
            const auto& c=controls.back();double n=normalized(c.id,hooks.value(hooks.context,c.id));
            Pen track(Color(255,68,76,92),7);g.DrawLine(&track,1265.f,681.f,1515.f,681.f);
            SolidBrush cap(Color(255,211,205,185));g.FillEllipse(&cap,1254+float(n)*250,670.f,22.f,22.f);
            char value[80];hooks.format(hooks.context,c.id,hooks.value(hooks.context,c.id),value,sizeof value);text(g,wide(value),1230,707,310,40,25);
            text(g,L"Shift: fine · Double-click: reset",1210,758,345,25,17,false);
        }
        const int selected=drag>=0?drag:hover;
        if(selected>=0 && controls[size_t(selected)].id>=0) {
            int id=controls[size_t(selected)].id;char value[100];hooks.format(hooks.context,id,hooks.value(hooks.context,id),value,sizeof value);
            std::wstring name=wide(syn::definitions[id].name);
            if((id==syn::Reduce1||id==syn::Reduce2) && int(hooks.value(hooks.context,id==syn::Reduce1?syn::Wave1:syn::Wave2))==2) {
                name=L"Pulse detune";std::snprintf(value,sizeof value,"+%.1f cents",100*hooks.value(hooks.context,id));
            }
            SolidBrush bubble(Color(230,17,22,33));roundRect(g,bubble,535,440,560,55,9);
            text(g,name+L"  ·  "+wide(value),545,443,540,48,24);
        }
        if(GetFocus()==window && focus>=0 && drag<0) {
            const auto& c=controls[size_t(focus)];Pen ring(Color(170,229,220,192),1.5f);ring.SetDashStyle(DashStyleDot);
            if(c.kind==Kind::Knob||c.kind==Kind::Options||c.kind==Kind::Resonance||c.kind==Kind::Octave)g.DrawEllipse(&ring,c.x-c.w/2-6,c.y-c.h/2-6,c.w+12,c.h+12);
        }
        Graphics target(dc);target.DrawImage(&buffer,0,0);
    }
    int hit(float x,float y) const {
        if(options && x>=1189 && x<=1576 && y>=532 && y<=803) {
            if(x>=1240 && x<=1540 && y>=653 && y<=714)return int(controls.size()-1);
            return -1;
        }
        for(int i=int(controls.size())-2;i>=0;--i) {
            const auto& c=controls[size_t(i)];
            if(c.kind==Kind::Slider) { if(std::abs(x-c.x)<=38 && y>=c.y-24 && y<=c.y+c.h+24)return i; }
            else if(c.kind==Kind::Wave) { if(std::abs(x-c.x)<c.w/2 && y>100 && y<270)return i; }
            else if(c.kind==Kind::Target) { if((x>1320 && x<1560 && y>741 && y<806) || (std::abs(x-1270)<28 && std::abs(y-775)<28))return i; }
            else if(std::abs(x-c.x)<c.w/2+8 && std::abs(y-c.y)<c.h/2+8)return i;
        }
        return -1;
    }
    void changeOnce(int i,double value) { int id=controls[size_t(i)].id;if(id>=0 && hooks.begin(hooks.context,id)){hooks.change(hooks.context,id,value);hooks.end(hooks.context,id);} }
    void stopDrag() { if(drag>=0){int id=controls[size_t(drag)].id;drag=-1;hooks.end(hooks.context,id);}if(GetCapture()==window)ReleaseCapture(); }
    void cycle(int i,int direction=1) {
        const auto& c=controls[size_t(i)];if(c.kind==Kind::Options){options=!options;InvalidateRect(window,nullptr,FALSE);return;}
        int min=int(syn::definitions[c.id].min),count=int(syn::definitions[c.id].max)-min+1;
        int v=int(hooks.value(hooks.context,c.id));changeOnce(i,double(min+(v-min+direction+count)%count));
    }
    void menu(int i,POINT pt) {
        const auto& c=controls[size_t(i)];if(c.kind!=Kind::Wave && c.kind!=Kind::Target)return;
        const wchar_t* waves[]={L"Triangle",L"Saw",L"Pulse",L"Half triangle",L"Square + triangle",L"Trapezoid",L"Folded triangle"};
        const wchar_t* targets[]={L"Cutoff",L"Resonance",L"Oscillator II mix",L"Oscillator I Alt",L"Oscillator II Alt",L"Both oscillators' Alt"};
        HMENU m=CreatePopupMenu();int count=c.kind==Kind::Wave?7:6;
        for(int j=0;j<count;++j)AppendMenuW(m,MF_STRING|(j==int(hooks.value(hooks.context,c.id))?MF_CHECKED:0),j+1,c.kind==Kind::Wave?waves[j]:targets[j]);
        int chosen=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,window,nullptr);DestroyMenu(m);if(chosen)changeOnce(i,chosen-1);
    }
    LRESULT message(UINT msg,WPARAM wp,LPARAM lp) {
        float x=GET_X_LPARAM(lp)*designWidth/width,y=GET_Y_LPARAM(lp)*designHeight/height;
        switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);paint(dc);EndPaint(window,&ps);return 0;}
        case WM_PRINTCLIENT:paint(reinterpret_cast<HDC>(wp));return 0;
        case WM_TIMER:if(IsWindowVisible(window))InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_SIZE:width=LOWORD(lp);height=HIWORD(lp);background.reset();return 0;
        case WM_GETDLGCODE:return DLGC_WANTARROWS|DLGC_WANTTAB|DLGC_WANTCHARS;
        case WM_MOUSEMOVE: {
            if(!tracking){TRACKMOUSEEVENT t{sizeof t,TME_LEAVE,window,0};TrackMouseEvent(&t);tracking=true;}
            hover=hit(x,y);
            if(drag>=0){const auto& c=controls[size_t(drag)];double factor=(wp&MK_SHIFT)?0.1:1;
                double delta=c.kind==Kind::Noise?(x-lastX)/250:(lastY-y)/(c.kind==Kind::Slider?c.h:220);
                dragValue=std::clamp(dragValue+delta*factor,0.0,1.0);hooks.change(hooks.context,c.id,plain(c.id,dragValue));lastX=x;lastY=y;}
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_MOUSELEAVE:tracking=false;hover=-1;InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_LBUTTONDOWN: {
            focus=hover=hit(x,y);SetFocus(window);if(focus<0){if(options){options=false;InvalidateRect(window,nullptr,FALSE);}return 0;}
            const auto& c=controls[size_t(focus)];if(c.kind==Kind::Options||syn::definitions[c.id].stepped){cycle(focus);InvalidateRect(window,nullptr,FALSE);return 0;}
            if(!hooks.begin(hooks.context,c.id))return 0;
            drag=focus;lastX=x;lastY=y;dragValue=normalized(c.id,hooks.value(hooks.context,c.id));SetCapture(window);
            if(!(wp&MK_SHIFT) && (c.kind==Kind::Slider||c.kind==Kind::Noise)){
                double clicked=c.kind==Kind::Noise?(x-1265)/250:1-(y-c.y)/c.h;
                if(std::abs(clicked-dragValue)>.09){dragValue=std::clamp(clicked,0.0,1.0);hooks.change(hooks.context,c.id,plain(c.id,dragValue));}}
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_LBUTTONUP:stopDrag();InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_CAPTURECHANGED:if(drag>=0){int id=controls[size_t(drag)].id;drag=-1;hooks.end(hooks.context,id);}return 0;
        case WM_LBUTTONDBLCLK:{stopDrag();int i=hit(x,y);if(i>=0 && controls[size_t(i)].id>=0)changeOnce(i,syn::definitions[controls[size_t(i)].id].initial);InvalidateRect(window,nullptr,FALSE);return 0;}
        case WM_RBUTTONUP:{int i=hit(x,y);if(i>=0){POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(window,&pt);menu(i,pt);}return 0;}
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&pt);int i=hit(pt.x*designWidth/width,pt.y*designHeight/height);
            if(i>=0 && controls[size_t(i)].id>=0){int id=controls[size_t(i)].id;double step=(GET_KEYSTATE_WPARAM(wp)&MK_SHIFT)?0.001:0.01;
                if(syn::definitions[id].stepped)cycle(i,GET_WHEEL_DELTA_WPARAM(wp)>0?1:-1);
                else changeOnce(i,plain(id,normalized(id,hooks.value(hooks.context,id))+step*GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA));}
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_KEYDOWN: {
            if(wp==VK_ESCAPE){stopDrag();options=false;InvalidateRect(window,nullptr,FALSE);return 0;}
            if(wp==VK_TAB){int count=options?int(controls.size()):int(controls.size())-1;focus=(focus+((GetKeyState(VK_SHIFT)<0)?count-1:1)+count)%count;hover=focus;InvalidateRect(window,nullptr,FALSE);return 0;}
            if(focus<0)return 0;
            const auto& c=controls[size_t(focus)];
            if(wp==VK_SPACE||wp==VK_RETURN){if(c.kind==Kind::Options||syn::definitions[c.id].stepped)cycle(focus);}
            else if(c.id>=0){if(wp==VK_HOME)changeOnce(focus,syn::definitions[c.id].initial);
                else if(wp==VK_UP||wp==VK_RIGHT||wp==VK_DOWN||wp==VK_LEFT){int sign=(wp==VK_UP||wp==VK_RIGHT)?1:-1;
                    if(syn::definitions[c.id].stepped)cycle(focus,sign);
                    else changeOnce(focus,plain(c.id,normalized(c.id,hooks.value(hooks.context,c.id))+sign*(GetKeyState(VK_SHIFT)<0?.001:.01)));}}
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_DESTROY:stopDrag();KillTimer(window,1);return 0;
        case WM_KILLFOCUS:stopDrag();InvalidateRect(window,nullptr,FALSE);return 0;
        default:return DefWindowProcW(window,msg,wp,lp);
        }
    }
    static LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        Editor* self=reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE){self=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(msg==WM_NCDESTROY && self){
            LRESULT result=DefWindowProcW(hwnd,msg,wp,lp);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);self->window=nullptr;return result;
        }
        return self?self->message(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
    }
public:
    explicit Editor(Hooks h):hooks(h) {}
    ~Editor(){if(window){stopDrag();KillTimer(window,1);DestroyWindow(window);}if(!className.empty())UnregisterClassW(className.c_str(),instance);background.reset();if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);}
    bool create() {
        Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&gdiplusToken,&input,nullptr)!=Gdiplus::Ok)return false;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&wndProc),&instance);
        className=L"Shiboleth.Editor."+std::to_wstring(reinterpret_cast<uintptr_t>(this));
        WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=className.c_str();
        if(!RegisterClassW(&wc))return false;
        window=CreateWindowExW(0,className.c_str(),L"Shiboleth",WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,width,height,nullptr,nullptr,instance,this);
        return window!=nullptr;
    }
    bool parent(HWND p){if(!window||!IsWindow(p))return false;SetWindowLongPtrW(window,GWL_STYLE,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);SetLastError(0);SetParent(window,p);if(GetLastError())return false;SetWindowPos(window,nullptr,0,0,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);return true;}
    bool size(uint32_t w,uint32_t h){if(!window||w<654||w>2453||h<385||h>1443)return false;width=w;height=h;SetWindowPos(window,nullptr,0,0,w,h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);background.reset();InvalidateRect(window,nullptr,FALSE);return true;}
    void dimensions(uint32_t* w,uint32_t* h)const{*w=width;*h=height;}
    bool show(bool visible){if(!window)return false;if(!visible){stopDrag();KillTimer(window,1);}else SetTimer(window,1,33,nullptr);ShowWindow(window,visible?SW_SHOWNOACTIVATE:SW_HIDE);return true;}
};
} // namespace ui
