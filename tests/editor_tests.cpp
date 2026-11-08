#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <clap/clap.h>
#include "../src/synth.hpp"
#include "../src/vector_art.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>

void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
const void* CLAP_ABI extension(const clap_host_t*,const char*){return nullptr;}
void CLAP_ABI request(const clap_host_t*){}
struct Output {
    struct Event{uint16_t type;clap_id id;double value;};std::vector<Event> events;bool reject=false;
    clap_output_events_t api{this,[](const clap_output_events_t* out,const clap_event_header_t* e){
        auto& self=*static_cast<Output*>(out->ctx);if(self.reject)return false;
        if(e->type==CLAP_EVENT_PARAM_VALUE){auto v=reinterpret_cast<const clap_event_param_value_t*>(e);self.events.push_back({e->type,v->param_id,v->value});}
        else {auto g=reinterpret_cast<const clap_event_param_gesture_t*>(e);self.events.push_back({e->type,g->param_id,0});}return true;}};
};
void screenshot(HWND editor,const wchar_t* path,int width,int height){
    Gdiplus::Bitmap image(width,height,PixelFormat32bppARGB);
    {Gdiplus::Graphics g(&image);HDC dc=g.GetHDC();SendMessageW(editor,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);g.ReleaseHDC(dc);}
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
    require(image.Save(path,&png,nullptr)==Gdiplus::Ok,"Save editor screenshot");
}
int main(int argc,char** argv){
    try {
        HMODULE dll=LoadLibraryW(L"build/Shiboleth.clap");require(dll!=nullptr,"Load plugin");
        auto entry=reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(dll,"clap_entry"));require(entry && entry->init("build/Shiboleth.clap"),"Entry init");
        auto factory=static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        clap_host_t host{CLAP_VERSION_INIT,nullptr,"Editor test host","Local","","1",extension,request,request,request};
        auto p=factory->create_plugin(factory,&host,factory->get_plugin_descriptor(factory,0)->id);require(p && p->init(p),"Create plugin");
        auto gui=static_cast<const clap_plugin_gui_t*>(p->get_extension(p,CLAP_EXT_GUI));
        auto params=static_cast<const clap_plugin_params_t*>(p->get_extension(p,CLAP_EXT_PARAMS));
        require(gui && gui->is_api_supported(p,CLAP_WINDOW_API_WIN32,false),"Win32 embedded editor advertised");
        require(!gui->is_api_supported(p,CLAP_WINDOW_API_WIN32,true),"Do not advertise floating windows");
        require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Create vector editor");
        // Render vector knobs at two device scales: lighting must stay fixed
        // across rotation, while the pointer moves. No sprite assets involved.
        for(int size:{112,256})for(int style=0;style<3;++style){
            Gdiplus::Bitmap low(size,size,PixelFormat32bppARGB),mid(size,size,PixelFormat32bppARGB),high(size,size,PixelFormat32bppARGB);
            auto draw=[&](Gdiplus::Bitmap& image,double value){Gdiplus::Graphics g(&image);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);ui::art::knob(g,size*.5f,size*.5f,float(size),float(size),style,value);};
            draw(low,0);draw(mid,.5);draw(high,1);Gdiplus::Color a,b,c;
            low.GetPixel(size/3,size/3,&a);mid.GetPixel(size/3,size/3,&b);high.GetPixel(size/3,size/3,&c);
            require(a.GetValue()==b.GetValue() && b.GetValue()==c.GetValue(),"Vector knob lighting is fixed across rotation and scale");
            mid.GetPixel(size/2,size/5,&a);low.GetPixel(size/2,size/5,&b);require(a.GetValue()!=b.GetValue(),"Vector pointer rotates");
        }
        require(!gui->set_scale(p,1.5),"First-open host DPI hint is declined for fixed-size panel");
        uint32_t width=0,height=0;require(gui->get_size(p,&width,&height),"Get editor size");
        require(width==1300 && height==765,"Initial editor dimensions");
        HWND parent=CreateWindowExW(0,L"STATIC",L"Shiboleth — editor preview",WS_OVERLAPPEDWINDOW,100,100,int(width+32),int(height+60),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(parent!=nullptr,"Create parent");clap_window_t window{};window.api=CLAP_WINDOW_API_WIN32;window.win32=parent;
        require(gui->set_parent(p,&window),"Embed editor");require(gui->show(p),"Show editor");
        HWND child=GetWindow(parent,GW_CHILD);require(child!=nullptr,"Editor child exists");
        auto point=[&](int x,int y){return MAKELPARAM(int(x*width/1635.),int(y*height/962.));};
        auto click=[&](int x,int y){SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(x,y));SendMessageW(child,WM_LBUTTONUP,0,point(x,y));};
        Output out;
        click(364,751);params->flush(p,nullptr,&out.api);
        double value=0;params->get_value(p,1000+syn::Resonance,&value);require(value==2,"Resonance cycles to next state");
        require(out.events.size()==3 && out.events[0].type==CLAP_EVENT_PARAM_GESTURE_BEGIN && out.events[1].type==CLAP_EVENT_PARAM_VALUE && out.events[2].type==CLAP_EVENT_PARAM_GESTURE_END,"Click emits begin/value/end automation gesture");
        for(int expected:{1,-2,-1,0}){click(300,350);params->flush(p,nullptr,&out.api);params->get_value(p,1000+syn::Octave,&value);require(value==expected,"Octave button cycles all four positions");}
        out.events.clear();click(300,246);params->flush(p,nullptr,&out.api);params->get_value(p,1000+syn::Wave1,&value);require(value==2,"Wave selector advances");
        // Real Windows rapid-click sequence: DOWN, UP, DBLCLK, UP.
        // Every physical press must advance selectors, irrespective of timing.
        struct Selector {int id,x,y;};
        for(auto selector:{Selector{syn::Resonance,364,751},Selector{syn::Octave,300,350},
                Selector{syn::Wave1,300,246},Selector{syn::Wave2,1115,246},Selector{syn::AftertouchTarget,1270,775},
                Selector{syn::Wave1,300,153},Selector{syn::Wave2,1115,153},Selector{syn::AftertouchTarget,1441,774}}){
            const auto& def=syn::definitions[selector.id];int count=int(def.max-def.min+1);
            params->get_value(p,1000+selector.id,&value);int expected=int(value);
            for(int press=0;press<2*count;++press){out.events.clear();
                SendMessageW(child,press%2?WM_LBUTTONDBLCLK:WM_LBUTTONDOWN,MK_LBUTTON,point(selector.x,selector.y));
                SendMessageW(child,WM_LBUTTONUP,0,point(selector.x,selector.y));params->flush(p,nullptr,&out.api);
                expected=int(def.min)+(expected-int(def.min)+1)%count;
                params->get_value(p,1000+selector.id,&value);require(value==expected,"Rapid selector clicks cycle every position");
                require(out.events.size()==3 && out.events.front().type==CLAP_EVENT_PARAM_GESTURE_BEGIN && out.events.back().type==CLAP_EVENT_PARAM_GESTURE_END,"Every rapid click sends one complete gesture");
            }
        }
        SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(435,322));SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(435,180));SendMessageW(child,WM_LBUTTONUP,0,point(435,180));params->flush(p,nullptr,&out.api);
        params->get_value(p,1000+syn::Reduce1,&value);require(value>.6 && value<.8,"Slider drag changes value");
        SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,point(435,180));params->flush(p,nullptr,&out.api);params->get_value(p,1000+syn::Reduce1,&value);require(value==0,"Double click resets");
        SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(200,650));SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(200,600));SendMessageW(child,WM_LBUTTONUP,0,point(200,600));params->flush(p,nullptr,&out.api);
        params->get_value(p,1000+syn::Cutoff,&value);require(value>.75,"Knob drag changes cutoff");
        const int symbols[]={435,515,588,1250,1326,1400};
        for(int i=0;i<6;++i){
            const int x=symbols[i];out.events.clear();
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(x,373));
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(x,318));
            SendMessageW(child,WM_LBUTTONUP,0,point(x,318));params->flush(p,nullptr,&out.api);
            params->get_value(p,1000+syn::CrushEnv1+i,&value);require(value>.48 && value<.52,"Symbol drag applies positive depth independently");
            require(out.events.front().type==CLAP_EVENT_PARAM_GESTURE_BEGIN && out.events.back().type==CLAP_EVENT_PARAM_GESTURE_END,"Depth drag has automation gesture");
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(x,373));
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(x,483));
            SendMessageW(child,WM_LBUTTONUP,0,point(x,483));params->flush(p,nullptr,&out.api);
            params->get_value(p,1000+syn::CrushEnv1+i,&value);require(value<-.48 && value>-.52,"Symbol drag crosses to negative depth");
            SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,point(x,373));params->flush(p,nullptr,&out.api);
            params->get_value(p,1000+syn::CrushEnv1+i,&value);require(value==0,"Double-click symbol resets depth");
        }
        SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(515,373));SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(515,300));SendMessageW(child,WM_LBUTTONUP,0,point(515,300));params->flush(p,nullptr,&out.api);
        click(1270,775);params->flush(p,nullptr,&out.api);params->get_value(p,1000+syn::AftertouchTarget,&value);require(value==1,"Aftertouch target selector");
        click(1563,862);click(1460,681);params->flush(p,nullptr,&out.api);params->get_value(p,1000+syn::NoiseCutoff,&value);require(value>300,"Options exposes shared noise cutoff");
        screenshot(child,L"build/editor-options.png",int(width),int(height));click(1563,862);
        // A host temporarily refusing events must not lose the gesture/value.
        out.events.clear();out.reject=true;click(364,751);params->flush(p,nullptr,&out.api);require(out.events.empty(),"Host rejection simulated");
        out.reject=false;params->flush(p,nullptr,&out.api);require(out.events.size()==3,"Retry rejected automation events");
        // Host automation updates parameter storage used by the editor.
        clap_event_param_value_t change{};change.header={sizeof change,0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};change.param_id=1000+syn::Cutoff;change.note_id=change.port_index=change.channel=change.key=-1;change.value=.58;
        clap_input_events_t input{&change,[](const clap_input_events_t*)->uint32_t{return 1;},[](const clap_input_events_t* s,uint32_t)->const clap_event_header_t*{return &static_cast<clap_event_param_value_t*>(s->ctx)->header;}};
        params->flush(p,&input,&out.api);params->get_value(p,change.param_id,&value);require(value==.58,"Host automation reaches editor value store");
        require(p->activate(p,48000,1,256) && p->start_processing(p),"Activate with editor open");
        float l[256]{},r[256]{};float* channels[]={l,r};clap_audio_buffer_t audio{};audio.data32=channels;audio.channel_count=2;
        clap_process_t block{};block.frames_count=256;block.audio_outputs=&audio;block.audio_outputs_count=1;block.out_events=&out.api;
        for(int n=0;n<100;++n)require(p->process(p,&block)!=CLAP_PROCESS_ERROR,"Process scope data");
        SendMessageW(child,WM_MOUSELEAVE,0,0);SetFocus(parent);screenshot(child,L"build/editor.png",int(width),int(height));
        require(!gui->can_resize(p),"Fixed panel does not advertise resizing");
        uint32_t otherW=820,otherH=482;require(!gui->adjust_size(p,&otherW,&otherH),"No interactive resizing");
        require(!gui->set_size(p,820,482),"Reject different fixed panel dimensions");require(gui->set_size(p,width,height),"Accept fixed dimensions");
        for(double scale:{1.,1.25,1.5,2.,0.}){
            require(!gui->set_scale(p,scale),"Scale hints do not change fixed-size editor");
            uint32_t w,h;gui->get_size(p,&w,&h);require(w==1300 && h==765,"Host DPI request retains approved dimensions");
            RECT client;GetClientRect(child,&client);require(client.right==1300 && client.bottom==765,"Native child matches advertised dimensions");
        }
        require(gui->hide(p) && gui->show(p),"Hide/show lifecycle");
        if(argc>1 && std::strcmp(argv[1],"--show")==0){ShowWindow(parent,SW_SHOW);MSG message;for(int i=0;i<600 && IsWindow(parent);++i){while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}p->process(p,&block);Sleep(33);}}
        // No test-owned GDI+ token: each close really shuts graphics down.
        // Keep the plugin active and process between editor teardown/recreation.
        for(int repeat=0;repeat<12;++repeat){
            gui->destroy(p);require(GetWindow(parent,GW_CHILD)==nullptr,"Editor child destroyed");
            require(p->process(p,&block)!=CLAP_PROCESS_ERROR,"Audio continues with editor closed");
            require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Recreate editor");require(gui->set_parent(p,&window),"Re-embed editor");require(gui->show(p),"Show reopened editor");
            uint32_t reopenedW,reopenedH;gui->get_size(p,&reopenedW,&reopenedH);require(reopenedW==width && reopenedH==height,"Reopen matches first-open size without repeated scale hint");
            child=GetWindow(parent,GW_CHILD);click(1563,862);screenshot(child,L"build/editor-reopened.png",int(width),int(height));click(1563,862);
        }
        // Hosts may destroy their parent before notifying the plugin.
        DestroyWindow(parent);gui->destroy(p);
        require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Recreate after parent destruction");gui->destroy(p);
        p->stop_processing(p);p->deactivate(p);
        DestroyWindow(parent);p->destroy(p);
        p=factory->create_plugin(factory,&host,factory->get_plugin_descriptor(factory,0)->id);require(p && p->init(p),"Fresh synth instance");
        gui=static_cast<const clap_plugin_gui_t*>(p->get_extension(p,CLAP_EXT_GUI));require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Fresh instance editor");
        require(!gui->set_scale(p,2),"Fresh instance declines first-open scale");gui->get_size(p,&width,&height);require(width==1300 && height==765,"Reloading synth retains approved size");
        gui->destroy(p);p->destroy(p);entry->deinit();FreeLibrary(dll);
        std::cout<<"PASS: embedded editor, automation gestures/retry, controls, octave, fixed size/DPI, scopes, teardown/recreation; saved editor screenshots\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
