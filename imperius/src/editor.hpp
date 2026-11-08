#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <string>
#include <cstring>
#include "parameters.hpp"
#include "knob_frames.hpp"

namespace imperius::ui {
constexpr uint32_t preferredWidth=1300,preferredHeight=780;
constexpr float designWidth=1620,designHeight=972;
struct Hooks {
    void* context;
    double (*value)(void*,int);
    bool (*begin)(void*,int);
    void (*change)(void*,int,double);
    void (*end)(void*,int);
    void (*format)(void*,int,double,char*,size_t);
};
enum class Kind { Knob, Hero, Toggle, Sync };
struct Control { int id;Kind kind;float x,y,size; };
inline constexpr std::array<Control,Count> controls{{
    {ComplexShape,Kind::Hero,213,300,146},{Harmonics,Kind::Hero,424,300,146},{Fold,Kind::Hero,638,300,146},
    {PhaseMod,Kind::Knob,213,470,70},{HarmonicMod,Kind::Knob,424,470,70},{Bias,Kind::Knob,638,470,70},
    {HarmonicEnv,Kind::Knob,213,595,70},{FoldEnv,Kind::Knob,424,595,70},{FoldMod,Kind::Knob,638,595,70},
    {BasicWave,Kind::Toggle,870,312,70},{BasicTune,Kind::Knob,1053,296,76},
    {ComplexLevel,Kind::Knob,823,489,76},{BasicLevel,Kind::Knob,938,489,76},{SubLevel,Kind::Knob,1056,489,76},
    {Cutoff,Kind::Hero,1320,320,180},{Resonance,Kind::Knob,1225,497,76},{FilterEnv,Kind::Knob,1383,497,76},
    {Highpass,Kind::Toggle,1508,320,60},
    {Lfo1Rate,Kind::Knob,130,794,64},{Lfo1Shape,Kind::Knob,226,794,64},{HarmonicLfo,Kind::Knob,324,794,64},
    {Lfo1Sync,Kind::Sync,117,893,34},
    {Lfo2Rate,Kind::Knob,453,794,64},{Lfo2Shape,Kind::Knob,550,794,64},{FoldLfo,Kind::Knob,647,794,64},
    {Lfo2Sync,Kind::Sync,443,893,34},
    {ModAttack,Kind::Knob,779,805,64},{ModDecay,Kind::Knob,874,805,64},{ModSustain,Kind::Knob,970,805,64},{ModRelease,Kind::Knob,1065,805,64},
    {AmpAttack,Kind::Knob,1200,805,64},{AmpDecay,Kind::Knob,1296,805,64},{AmpSustain,Kind::Knob,1393,805,64},{AmpRelease,Kind::Knob,1490,805,64},
    {Octave,Kind::Toggle,1170,93,70},{Volume,Kind::Knob,1478,64,76}
}};
inline bool logarithmic(int id) {
    return id==AmpAttack || id==AmpDecay || id==AmpRelease || id==ModAttack || id==ModDecay || id==ModRelease;
}
inline double normalized(int id,double v) {
    const auto& d=definitions[id];v=valid(id,v);
    return logarithmic(id)?std::log(v/d.min)/std::log(d.max/d.min):(v-d.min)/(d.max-d.min);
}
inline double plain(int id,double n) {
    const auto& d=definitions[id];n=std::clamp(n,0.,1.);
    return valid(id,logarithmic(id)?d.min*std::pow(d.max/d.min,n):d.min+n*(d.max-d.min));
}
class Editor {
    Hooks hooks;
    HWND window{};HINSTANCE instance{};std::wstring className;
    ULONG_PTR gdiplusToken{};
    std::unique_ptr<Gdiplus::Bitmap> faceplate,background;
    std::unique_ptr<KnobFrames> frames;
    uint32_t width=preferredWidth,height=preferredHeight;
    int hover=-1,drag=-1,focus=-1;float lastY=0;double dragValue=0;
    bool tracking=false;
    Values observed=defaults();
    bool syncedRate(int id) const {
        return (id==Lfo1Rate && hooks.value(hooks.context,Lfo1Sync)>=.5)
            || (id==Lfo2Rate && hooks.value(hooks.context,Lfo2Sync)>=.5);
    }
    double position(int id,double value) const {
        double n=normalized(id,value);return syncedRate(id)?std::round(n*9)/9:n;
    }
    double controlValue(int id,double n) const {
        if(syncedRate(id))n=std::round(n*9)/9;
        return plain(id,n);
    }
    static std::unique_ptr<Gdiplus::Bitmap> resource(HINSTANCE module) {
        HRSRC r=FindResourceW(module,MAKEINTRESOURCEW(101),MAKEINTRESOURCEW(10));if(!r)return {};
        auto loaded=LoadResource(module,r);DWORD length=SizeofResource(module,r);void* bytes=LockResource(loaded);
        if(!bytes || !length)return {};
        HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,length);if(!memory)return {};
        void* dest=GlobalLock(memory);if(!dest){GlobalFree(memory);return {};}
        std::memcpy(dest,bytes,length);GlobalUnlock(memory);
        IStream* stream=nullptr;if(CreateStreamOnHGlobal(memory,TRUE,&stream)!=S_OK){GlobalFree(memory);return {};}
        auto decoded=std::unique_ptr<Gdiplus::Bitmap>(Gdiplus::Bitmap::FromStream(stream));
        std::unique_ptr<Gdiplus::Bitmap> result;
        if(decoded && decoded->GetLastStatus()==Gdiplus::Ok) {
            result=std::make_unique<Gdiplus::Bitmap>(decoded->GetWidth(),decoded->GetHeight(),PixelFormat32bppPARGB);
            Gdiplus::Graphics copy(result.get());copy.DrawImage(decoded.get(),0,0);
        }
        decoded.reset();stream->Release();return result;
    }
    static std::wstring wide(const char* text) {
        int count=MultiByteToWideChar(CP_UTF8,0,text,-1,nullptr,0);if(count<1)return {};
        std::wstring out(size_t(count),L'\0');MultiByteToWideChar(CP_UTF8,0,text,-1,out.data(),count);out.pop_back();return out;
    }
    static void rounded(Gdiplus::Graphics& g,Gdiplus::Brush& brush,float x,float y,float w,float h,float radius) {
        Gdiplus::GraphicsPath p;float d=radius*2;
        p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);
        p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();g.FillPath(&brush,&p);
    }
    void knob(Gdiplus::Graphics& g,const Control& c,double n) {
        using namespace Gdiplus;
        const bool hero=c.kind==Kind::Hero;const float radius=c.size*.5f;
        if(hero) {
            float r=radius+11;
            Pen border(Color(255,59,55,46),14),track(Color(255,102,96,83),10);
            border.SetStartCap(LineCapRound);border.SetEndCap(LineCapRound);track.SetStartCap(LineCapRound);track.SetEndCap(LineCapRound);
            g.DrawArc(&border,c.x-r,c.y-r,2*r,2*r,180.f,180.f);g.DrawArc(&track,c.x-r,c.y-r,2*r,2*r,180.f,180.f);
            if(n>.001) {
                Pen glow(Color(45,255,162,0),23),amber(Color(255,246,160,0),10),light(Color(255,255,202,58),4);
                glow.SetStartCap(LineCapRound);glow.SetEndCap(LineCapRound);amber.SetStartCap(LineCapRound);amber.SetEndCap(LineCapRound);light.SetStartCap(LineCapRound);light.SetEndCap(LineCapRound);
                g.DrawArc(&glow,c.x-r,c.y-r,2*r,2*r,180.f,float(180*n));
                g.DrawArc(&amber,c.x-r,c.y-r,2*r,2*r,180.f,float(180*n));
                g.DrawArc(&light,c.x-r,c.y-r-1,2*r,2*r,180.f,float(180*n));
            }
        }
        for(int s=6;s>0;--s) {
            SolidBrush shadow(Color(BYTE(5+s*2),0,0,0));
            g.FillEllipse(&shadow,c.x-radius-s,c.y-radius+5-s,c.size+2*s,c.size+2*s);
        }
        auto* frame=frames->get(hero,n);
        g.DrawImage(frame,RectF(c.x-radius,c.y-radius,c.size,c.size),0,0,float(frame->GetWidth()),float(frame->GetHeight()),UnitPixel);
    }
    void toggle(Gdiplus::Graphics& g,const Control& c,double value) {
        using namespace Gdiplus;
        const bool vertical=c.id==Highpass;
        double n=normalized(c.id,value);float offset=float((n-.5)*24);
        const float w=vertical?25.f:64.f,h=vertical?46.f:25.f;
        SolidBrush shadow(Color(80,0,0,0));rounded(g,shadow,c.x-w/2-2,c.y-h/2+3,w+4,h+4,12);
        LinearGradientBrush rim(PointF(c.x-30,c.y-22),PointF(c.x+30,c.y+22),Color(255,226,217,196),Color(255,79,73,61));
        rounded(g,rim,c.x-w/2,c.y-h/2,w,h,11);
        SolidBrush black(Color(255,22,21,18));rounded(g,black,c.x-w/2+3,c.y-h/2+3,w-6,h-6,9);
        float x=c.x+(vertical?0:offset),y=c.y+(vertical?offset:0);
        Pen stemShade(Color(255,51,49,42),10),stem(Color(255,183,180,167),6);
        g.DrawLine(&stemShade,c.x,c.y,x,y-15);g.DrawLine(&stem,c.x,c.y,x,y-15);
        LinearGradientBrush cap(PointF(x-7,y-22),PointF(x+6,y-7),Color(255,255,253,239),Color(255,93,91,81));
        g.FillEllipse(&cap,x-7,y-23,14.f,16.f);Pen edge(Color(255,69,65,55),1);g.DrawEllipse(&edge,x-7,y-23,14.f,16.f);
    }
    void syncButton(Gdiplus::Graphics& g,const Control& c,bool on) {
        using namespace Gdiplus;
        SolidBrush shade(Color(65,0,0,0));rounded(g,shade,c.x-19,c.y-16,39,39,4);
        SolidBrush rim(Color(255,54,49,39));rounded(g,rim,c.x-18,c.y-18,36,36,4);
        LinearGradientBrush cap(PointF(c.x,c.y-15),PointF(c.x,c.y+15),on?Color(255,255,228,133):Color(255,128,122,107),on?Color(255,245,158,5):Color(255,85,80,69));
        rounded(g,cap,c.x-14,c.y-14,28,28,2);
        if(on){SolidBrush center(Color(150,255,222,91));rounded(g,center,c.x-10,c.y-10,20,20,3);}
    }
    void paint(HDC dc) {
        using namespace Gdiplus;
        if(!width || !height || !faceplate)return;
        if(!background || background->GetWidth()!=width || background->GetHeight()!=height) {
            background=std::make_unique<Bitmap>(width,height,PixelFormat32bppPARGB);Graphics bg(background.get());
            bg.SetInterpolationMode(InterpolationModeHighQualityBicubic);bg.DrawImage(faceplate.get(),Rect(0,0,int(width),int(height)));
        }
        Bitmap bitmap(width,height,PixelFormat32bppPARGB);Graphics g(&bitmap);g.DrawImage(background.get(),0,0);
        g.ScaleTransform(width/designWidth,height/designHeight);g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        for(const auto& c:controls) {
            double v=hooks.value(hooks.context,c.id);
            if(c.kind==Kind::Knob || c.kind==Kind::Hero)knob(g,c,position(c.id,v));
            else if(c.kind==Kind::Toggle)toggle(g,c,v);
            else syncButton(g,c,v>=.5);
        }
        if(GetFocus()==window && focus>=0) {
            const auto& c=controls[size_t(focus)];Pen pen(Color(165,112,78,9),2);pen.SetDashStyle(DashStyleDot);
            if(c.kind==Kind::Knob || c.kind==Kind::Hero)g.DrawEllipse(&pen,c.x-c.size/2-5,c.y-c.size/2-5,c.size+10,c.size+10);
            else g.DrawRectangle(&pen,c.x-38,c.y-29,76.f,58.f);
        }
        int selected=drag>=0?drag:hover;
        if(selected>=0) {
            int id=controls[size_t(selected)].id;char value[100]{};hooks.format(hooks.context,id,hooks.value(hooks.context,id),value,sizeof value);
            std::wstring label=wide(definitions[id].name)+L"  ·  "+wide(value);
            SolidBrush bubble(Color(240,31,29,24));rounded(g,bubble,600,76,490,42,7);
            FontFamily family(L"Arial");Font font(&family,19,FontStyleRegular,UnitPixel);SolidBrush ink(Color(255,248,239,215));
            StringFormat fmt;fmt.SetAlignment(StringAlignmentCenter);fmt.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(label.c_str(),-1,&font,RectF(610,78,470,38),&fmt,&ink);
        }
        Graphics target(dc);target.DrawImage(&bitmap,0,0);
    }
    int hit(float x,float y) const {
        for(int i=int(controls.size())-1;i>=0;--i) {
            const auto& c=controls[size_t(i)];float r=c.size/2+6;
            if(c.kind==Kind::Toggle) { if(std::abs(x-c.x)<40 && std::abs(y-c.y)<32)return i; }
            else if(c.kind==Kind::Sync) { if(std::abs(x-c.x)<21 && std::abs(y-c.y)<21)return i; }
            else if(std::hypot(x-c.x,y-c.y)<r)return i;
        }
        return -1;
    }
    void changeOnce(int i,double v) {
        int id=controls[size_t(i)].id;
        if(hooks.begin(hooks.context,id)){hooks.change(hooks.context,id,v);hooks.end(hooks.context,id);}
    }
    void stopDrag() {
        if(drag>=0){int id=controls[size_t(drag)].id;drag=-1;hooks.end(hooks.context,id);}
        if(GetCapture()==window)ReleaseCapture();
    }
    void step(int i,int direction) {
        const auto& c=controls[size_t(i)];const auto& d=definitions[c.id];
        double v=hooks.value(hooks.context,c.id);
        if(d.stepped)changeOnce(i,valid(c.id,v+direction));
        else if(syncedRate(c.id))changeOnce(i,plain(c.id,position(c.id,v)+direction/9.));
        else changeOnce(i,plain(c.id,normalized(c.id,v)+direction*(GetKeyState(VK_SHIFT)<0?.001:.01)));
    }
    void select(int i,float x,float y) {
        const auto& c=controls[size_t(i)];double v=hooks.value(hooks.context,c.id);
        if(c.kind==Kind::Sync || c.id==Highpass)changeOnce(i,1-v);
        else {
            const auto& d=definitions[c.id];
            // Click either end to select it, center to cycle all three positions.
            double selected=std::abs(x-c.x)>12?(x<c.x?d.min:d.max):v+1;
            if(selected>d.max)selected=d.min;
            changeOnce(i,selected);
        }
        (void)y;
    }
    void menu(int i,POINT pt) {
        const auto& c=controls[size_t(i)];if(!definitions[c.id].stepped)return;
        HMENU m=CreatePopupMenu();const wchar_t* oct[]{L"-1 octave",L"0 octaves",L"+1 octave"};
        const wchar_t* waves[]{L"Square",L"Sine",L"Saw"};const wchar_t* modes[]{L"Low-pass",L"High-pass"};const wchar_t* sync[]{L"Free",L"Sync"};
        const wchar_t* const* labels=c.id==Octave?oct:c.id==BasicWave?waves:c.id==Highpass?modes:sync;
        int min=int(definitions[c.id].min),count=int(definitions[c.id].max)-min+1;
        for(int n=0;n<count;++n)AppendMenuW(m,MF_STRING|(n+min==int(hooks.value(hooks.context,c.id))?MF_CHECKED:0),n+1,labels[n]);
        int chosen=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,window,nullptr);DestroyMenu(m);
        if(chosen)changeOnce(i,chosen-1+min);
    }
    LRESULT message(UINT msg,WPARAM wp,LPARAM lp) {
        float x=GET_X_LPARAM(lp)*designWidth/width,y=GET_Y_LPARAM(lp)*designHeight/height;
        switch(msg) {
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);paint(dc);EndPaint(window,&ps);return 0;}
        case WM_PRINTCLIENT:paint(reinterpret_cast<HDC>(wp));return 0;
        case WM_SIZE:width=LOWORD(lp);height=HIWORD(lp);background.reset();return 0;
        case WM_TIMER: {
            bool changed=false;for(int i=0;i<Count;++i){double v=hooks.value(hooks.context,i);changed|=v!=observed[i];observed[i]=v;}
            if(changed && IsWindowVisible(window))InvalidateRect(window,nullptr,FALSE);
            return 0;
        }
        case WM_GETDLGCODE:return DLGC_WANTARROWS|DLGC_WANTTAB|DLGC_WANTCHARS;
        case WM_MOUSEMOVE: {
            if(!tracking){TRACKMOUSEEVENT t{sizeof t,TME_LEAVE,window,0};TrackMouseEvent(&t);tracking=true;}
            hover=hit(x,y);
            if(drag>=0) {
                const auto& c=controls[size_t(drag)];double factor=(wp&MK_SHIFT)?.1:1;
                dragValue=std::clamp(dragValue+(lastY-y)/220*factor,0.,1.);
                hooks.change(hooks.context,c.id,controlValue(c.id,dragValue));lastY=y;
            }
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_MOUSELEAVE:hover=-1;tracking=false;InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_LBUTTONDOWN: {
            focus=hover=hit(x,y);SetFocus(window);if(focus<0)return 0;
            const auto& c=controls[size_t(focus)];
            if(definitions[c.id].stepped)select(focus,x,y);
            else if(hooks.begin(hooks.context,c.id)) {
                drag=focus;lastY=y;dragValue=position(c.id,hooks.value(hooks.context,c.id));SetCapture(window);
            }
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_LBUTTONUP:stopDrag();InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_CAPTURECHANGED:if(drag>=0){int id=controls[size_t(drag)].id;drag=-1;hooks.end(hooks.context,id);}return 0;
        case WM_LBUTTONDBLCLK: {
            stopDrag();int i=hit(x,y);if(i>=0)changeOnce(i,definitions[controls[size_t(i)].id].initial);
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_RBUTTONUP:{int i=hit(x,y);if(i>=0){POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ClientToScreen(window,&pt);menu(i,pt);}return 0;}
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&pt);
            int i=hit(pt.x*designWidth/width,pt.y*designHeight/height);
            if(i>=0) {
                int id=controls[size_t(i)].id;double clicks=double(GET_WHEEL_DELTA_WPARAM(wp))/WHEEL_DELTA;
                if(definitions[id].stepped || syncedRate(id))step(i,clicks>0?1:-1);
                else changeOnce(i,plain(id,normalized(id,hooks.value(hooks.context,id))+clicks*((GET_KEYSTATE_WPARAM(wp)&MK_SHIFT)?.001:.01)));
            }
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_KEYDOWN: {
            if(wp==VK_ESCAPE){stopDrag();return 0;}
            if(wp==VK_TAB){int count=int(controls.size());focus=(focus+((GetKeyState(VK_SHIFT)<0)?count-1:1)+count)%count;hover=focus;}
            else if(focus>=0) {
                int id=controls[size_t(focus)].id;
                if(wp==VK_HOME)changeOnce(focus,definitions[id].initial);
                else if(wp==VK_UP || wp==VK_RIGHT || wp==VK_DOWN || wp==VK_LEFT)step(focus,(wp==VK_UP || wp==VK_RIGHT)?1:-1);
                else if((wp==VK_SPACE || wp==VK_RETURN) && definitions[id].stepped)select(focus,controls[size_t(focus)].x,controls[size_t(focus)].y);
            }
            InvalidateRect(window,nullptr,FALSE);return 0;
        }
        case WM_KILLFOCUS:stopDrag();InvalidateRect(window,nullptr,FALSE);return 0;
        case WM_DESTROY:stopDrag();KillTimer(window,1);return 0;
        default:return DefWindowProcW(window,msg,wp,lp);
        }
    }
    static LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
        auto* self=reinterpret_cast<Editor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE){self=static_cast<Editor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(msg==WM_NCDESTROY && self){LRESULT r=DefWindowProcW(hwnd,msg,wp,lp);SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);self->window=nullptr;return r;}
        return self?self->message(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
    }
public:
    explicit Editor(Hooks h):hooks(h) {}
    ~Editor() {
        if(window){stopDrag();DestroyWindow(window);}
        if(!className.empty())UnregisterClassW(className.c_str(),instance);
        background.reset();faceplate.reset();frames.reset();
        if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);
    }
    bool create() {
        Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&gdiplusToken,&input,nullptr)!=Gdiplus::Ok)return false;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&wndProc),&instance);
        faceplate=resource(instance);frames=std::make_unique<KnobFrames>();if(!faceplate)return false;
        className=L"Imperius.Editor."+std::to_wstring(reinterpret_cast<uintptr_t>(this));
        WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.lpfnWndProc=wndProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=className.c_str();
        if(!RegisterClassW(&wc))return false;
        window=CreateWindowExW(0,className.c_str(),L"Imperius",WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,width,height,nullptr,nullptr,instance,this);
        return window!=nullptr;
    }
    bool parent(HWND p) {
        if(!window || !IsWindow(p))return false;
        SetWindowLongPtrW(window,GWL_STYLE,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);SetLastError(0);SetParent(window,p);
        if(GetLastError())return false;
        SetWindowPos(window,nullptr,0,0,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);return true;
    }
    bool size(uint32_t w,uint32_t h) {
        if(!window || w<650 || h<390 || w>2600 || h>1560)return false;
        width=w;height=h;SetWindowPos(window,nullptr,0,0,w,h,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        background.reset();InvalidateRect(window,nullptr,FALSE);return true;
    }
    void dimensions(uint32_t* w,uint32_t* h)const{*w=width;*h=height;}
    bool show(bool visible) {
        if(!window)return false;
        if(visible)SetTimer(window,1,33,nullptr);else{stopDrag();KillTimer(window,1);}
        ShowWindow(window,visible?SW_SHOWNOACTIVATE:SW_HIDE);return true;
    }
};
}
