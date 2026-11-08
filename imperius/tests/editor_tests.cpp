#define NOMINMAX
#include "../src/editor.hpp"
#include <clap/clap.h>
#include <iostream>
#include <vector>
#include <stdexcept>

using namespace imperius;
void require(bool pass,const char* why){if(!pass)throw std::runtime_error(why);}
const void* CLAP_ABI extension(const clap_host_t*,const char*){return nullptr;}
void CLAP_ABI request(const clap_host_t*){}
struct Output {
    struct Event{uint16_t type;clap_id id;double value;};std::vector<Event> events;bool reject=false;
    clap_output_events_t api{this,[](const clap_output_events_t* out,const clap_event_header_t* e){
        auto& s=*static_cast<Output*>(out->ctx);if(s.reject)return false;
        if(e->type==CLAP_EVENT_PARAM_VALUE){auto v=reinterpret_cast<const clap_event_param_value_t*>(e);s.events.push_back({e->type,v->param_id,v->value});}
        else{auto g=reinterpret_cast<const clap_event_param_gesture_t*>(e);s.events.push_back({e->type,g->param_id,0});}return true;}};
};
void screenshot(HWND child,const wchar_t* path,int width,int height) {
    Gdiplus::Bitmap image(width,height,PixelFormat32bppARGB);
    {Gdiplus::Graphics graphics(&image);HDC dc=graphics.GetHDC();SendMessageW(child,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);graphics.ReleaseHDC(dc);}
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
    require(image.Save(path,&png,nullptr)==Gdiplus::Ok,"Save native screenshot");
}
int main() {
    try {
        HMODULE dll=LoadLibraryW(L"build/Imperius.clap");require(dll,"Load plugin");
        auto entry=reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(dll,"clap_entry"));require(entry && entry->init("build/Imperius.clap"),"Entry init");
        auto factory=static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        clap_host_t host{CLAP_VERSION_INIT,nullptr,"Imperius editor tests","Local","","1",extension,request,request,request};
        const auto* p=factory->create_plugin(factory,&host,"local.abdicant.imperius");require(p && p->init(p),"Instantiate");
        auto gui=static_cast<const clap_plugin_gui_t*>(p->get_extension(p,CLAP_EXT_GUI));
        auto params=static_cast<const clap_plugin_params_t*>(p->get_extension(p,CLAP_EXT_PARAMS));
        require(gui && gui->is_api_supported(p,CLAP_WINDOW_API_WIN32,false),"Embedded Win32 supported");
        require(!gui->is_api_supported(p,CLAP_WINDOW_API_WIN32,true),"Floating API rejected");
        require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Create editor with embedded artwork");
        require(!gui->create(p,CLAP_WINDOW_API_WIN32,false),"Reject duplicate editor creation");
        uint32_t width=0,height=0;require(gui->get_size(p,&width,&height) && width==1300 && height==780,"Native dimensions");
        HWND parent=CreateWindowExW(0,L"STATIC",L"Imperius preview",WS_OVERLAPPEDWINDOW,0,0,int(width+32),int(height+60),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        require(parent,"Create host parent");clap_window_t w{};w.api=CLAP_WINDOW_API_WIN32;w.win32=parent;
        require(gui->set_parent(p,&w) && gui->show(p),"Embed/show editor");HWND child=GetWindow(parent,GW_CHILD);require(child,"Editor child");
        Output out;
        auto point=[&](float x,float y){return MAKELPARAM(int(x*width/ui::designWidth),int(y*height/ui::designHeight));};
        auto click=[&](float x,float y){SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(x,y));SendMessageW(child,WM_LBUTTONUP,0,point(x,y));};
        auto value=[&](int id){double v=0;require(params->get_value(p,2000+id,&v),"Read parameter");return v;};
        auto flush=[&](){params->flush(p,nullptr,&out.api);};
        // Every parameter has one and only one reachable panel control.
        std::array<int,Count> occurrences{};for(const auto& c:ui::controls)++occurrences[c.id];
        for(int n:occurrences)require(n==1,"One control for each parameter");
        for(const auto& c:ui::controls) {
            if(definitions[c.id].stepped)continue;
            double previous=value(c.id);out.events.clear();
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(c.x,c.y));
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(c.x,c.y-22));
            SendMessageW(child,WM_LBUTTONUP,0,point(c.x,c.y-22));flush();
            require(value(c.id)>previous,"Every native knob is mapped and draggable");
            require(out.events.size()==3 && out.events[0].type==CLAP_EVENT_PARAM_GESTURE_BEGIN && out.events[1].type==CLAP_EVENT_PARAM_VALUE && out.events[2].type==CLAP_EVENT_PARAM_GESTURE_END,"Host receives a complete knob automation gesture");
            require(out.events[1].id==2000+clap_id(c.id),"Gesture uses correct parameter ID");
            SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,point(c.x,c.y));flush();
            require(value(c.id)==definitions[c.id].initial,"Every knob double-click resets default");
        }
        // The center of each toggle cycles; its ends select left/right directly.
        click(1145,93);flush();require(value(Octave)==-1,"Octave left");
        click(1170,93);flush();require(value(Octave)==0,"Octave center cycle");
        click(1195,93);flush();require(value(Octave)==1,"Octave right");
        click(845,312);flush();require(value(BasicWave)==0,"Basic square");
        click(870,312);flush();require(value(BasicWave)==1,"Basic sine");
        click(895,312);flush();require(value(BasicWave)==2,"Basic saw");
        click(1508,320);flush();require(value(Highpass)==1,"Filter LP/HP toggle");
        for(int n=0;n<2;++n){click(n?443:117,893);flush();require(value(n?Lfo2Sync:Lfo1Sync)==1,"One sync button per LFO");}
        screenshot(child,L"build/editor-sync.png",width,height);
        for(int n=0;n<2;++n) {
            float x=n?453.f:130.f;int id=n?Lfo2Rate:Lfo1Rate;
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(x,794));
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(x,750));
            SendMessageW(child,WM_LBUTTONUP,0,point(x,750));flush();
            require(std::abs(value(id)*9-std::round(value(id)*9))<1e-10,"Synced rate drag snaps to tempo divisions");
            double previous=value(id);SendMessageW(child,WM_KEYDOWN,VK_LEFT,0);flush();
            require(std::abs(value(id)-previous+1./9)<1e-10,"Synced rate keyboard changes by one division");
        }
        // Capture loss and hiding must finish an active automation gesture.
        out.events.clear();SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(638,300));
        SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(638,270));ReleaseCapture();flush();
        require(out.events.back().type==CLAP_EVENT_PARAM_GESTURE_END,"Capture loss ends gesture");
        out.events.clear();SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(638,300));gui->hide(p);flush();
        require(out.events.back().type==CLAP_EVENT_PARAM_GESTURE_END,"Hide ends gesture");require(gui->show(p),"Show again");
        out.events.clear();out.reject=true;click(117,893);flush();require(out.events.empty(),"Host event rejection simulated");
        out.reject=false;flush();require(out.events.size()==3,"Rejected UI events are retried intact");
        // Fine mouse movement has one tenth the normal normalized travel.
        SendMessageW(child,WM_LBUTTONDBLCLK,0,point(424,300));flush();double before=value(Harmonics);
        SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON|MK_SHIFT,point(424,300));
        SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON|MK_SHIFT,point(424,278));SendMessageW(child,WM_LBUTTONUP,0,point(424,278));flush();
        require(value(Harmonics)-before>.009 && value(Harmonics)-before<.011,"Shift fine movement");
        // Tab traverses all controls. Home resets even the stepped switches.
        SetFocus(parent);SetFocus(child);SendMessageW(child,WM_LBUTTONDOWN,0,point(50,110));
        for(size_t i=0;i<ui::controls.size();++i){SendMessageW(child,WM_KEYDOWN,VK_TAB,0);SendMessageW(child,WM_KEYDOWN,VK_HOME,0);flush();}
        for(int i=0;i<Count;++i)require(value(i)==definitions[i].initial,"Keyboard traverses and resets all controls");
        // Host automation is read directly by the editor timer/render path.
        clap_event_param_value_t change{};change.header={sizeof change,0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};
        change.param_id=2000+Fold;change.note_id=change.port_index=change.channel=change.key=-1;change.value=.8;
        clap_input_events_t in{&change,[](const clap_input_events_t*)->uint32_t{return 1;},[](const clap_input_events_t* e,uint32_t)->const clap_event_header_t*{return &static_cast<clap_event_param_value_t*>(e->ctx)->header;}};
        params->flush(p,&in,&out.api);require(value(Fold)==.8,"Host automation updates displayed store");SendMessageW(child,WM_TIMER,1,0);
        SendMessageW(child,WM_MOUSELEAVE,0,0);SetFocus(parent);
        screenshot(child,L"build/editor-automated.png",width,height);
        change.value=.15;params->flush(p,&in,&out.api);
        screenshot(child,L"build/editor.png",width,height);
        // Fixed-light rendering: the metal center stays identical as the pointer turns.
        {ui::KnobFrames frames;
            for(bool large:{false,true}) {
                auto low=frames.get(large,0),high=frames.get(large,1),middle=frames.get(large,.5);
                Gdiplus::Color a,b,c;int size=int(low->GetWidth());
                low->GetPixel(size/2,size/3,&a);high->GetPixel(size/2,size/3,&b);middle->GetPixel(size/2,size/3,&c);
                require(a.GetValue()==b.GetValue() && a.GetValue()==c.GetValue(),"Metal lighting stays fixed when knob turns");
                low->GetPixel(size/2,size/12,&a);middle->GetPixel(size/2,size/12,&b);
                require(a.GetValue()!=b.GetValue(),"Pointer changes position independently");
            }
        }
        require(!gui->can_resize(p) && !gui->set_size(p,800,500),"Fixed size enforced");
        require(gui->set_scale(p,1.25),"DPI scale supported");gui->get_size(p,&width,&height);
        require(width==1625 && height==975,"DPI dimensions");screenshot(child,L"build/editor-125.png",width,height);
        require(gui->set_scale(p,1),"Restore scale");gui->get_size(p,&width,&height);
        require(p->activate(p,48000,1,256) && p->start_processing(p),"Process with editor open");
        float left[256]{},right[256]{};float* channels[]{left,right};clap_audio_buffer_t audio{};audio.data32=channels;audio.channel_count=2;
        clap_process_t block{};block.frames_count=256;block.audio_outputs_count=1;block.audio_outputs=&audio;block.out_events=&out.api;
        // No extra test-owned GDI+ token; every close shuts rendering down fully.
        for(int repeat=0;repeat<12;++repeat) {
            gui->destroy(p);require(!GetWindow(parent,GW_CHILD),"Destroy child");
            require(p->process(p,&block)!=CLAP_PROCESS_ERROR,"Audio continues without editor");
            require(gui->create(p,CLAP_WINDOW_API_WIN32,false) && gui->set_parent(p,&w) && gui->show(p),"Recreate native editor");
            child=GetWindow(parent,GW_CHILD);screenshot(child,L"build/editor-reopened.png",width,height);
        }
        DestroyWindow(parent);gui->destroy(p);
        require(gui->create(p,CLAP_WINDOW_API_WIN32,false),"Recreate after parent-first destruction");gui->destroy(p);
        p->stop_processing(p);p->deactivate(p);p->destroy(p);entry->deinit();FreeLibrary(dll);
        std::cout<<"Native editor checks passed: 36 controls, gestures/retry, capture, switches, fine adjustment, keyboard, automation, DPI, repeated teardown.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
