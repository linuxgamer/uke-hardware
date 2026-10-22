// SPDX-License-Identifier: MIT
// Bounded Wayland presentation-time development diagnostic.
#include <wayland-client.h>
#include "xdg-shell-client.h"
#include "presentation-time-client.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <unordered_set>

namespace cadence {
static volatile sig_atomic_t stopped;
static uint64_t now() {
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) throw std::runtime_error("clock_gettime");
    return uint64_t(ts.tv_sec)*1000000000ULL + ts.tv_nsec;
}
struct Schedule { unsigned stage; uint64_t interval; bool idle, done; };
static uint64_t next_deadline(uint64_t previous,uint64_t current,uint64_t interval) {
    const auto next=previous+interval;
    return next>current?next:current+interval;
}
static Schedule schedule(uint64_t elapsed, unsigned frame, unsigned maximum) {
    constexpr std::array<unsigned,12> rates{30,36,40,45,48,50,60,72,90,100,120,144};
    if (elapsed < 24000000000ULL) {
        unsigned stage = elapsed/2000000000ULL;
        return {stage,1000000000ULL/std::min(rates[stage],maximum),false,false};
    }
    if (elapsed < 32000000000ULL) {
        const uint64_t shortest=1000000000ULL/maximum, longest=1000000000ULL/30;
        return {12,shortest+(uint64_t(frame)*7919001 % (longest-shortest+1)),false,false};
    }
    if (elapsed < 40000000000ULL) return {13,50000000,true,false};
    return {14,1000000000ULL/maximum,false,elapsed>=43000000000ULL};
}
struct App;
struct Buffer { wl_buffer *object{}; uint32_t *pixels{}; size_t bytes{}; bool busy{}; };
struct Feedback { App *app; struct wp_presentation_feedback *object; unsigned frame,stage; uint64_t submitted,requested; };
struct App {
    wl_display *display{}; wl_registry *registry{}; wl_compositor *compositor{}; wl_shm *shm{};
    xdg_wm_base *shell{}; wp_presentation *presentation{}; wl_surface *surface{};
    xdg_surface *xdg{}; xdg_toplevel *toplevel{};
    std::array<Buffer,3> buffers{}; std::unordered_set<Feedback*> pending;
    int width=1208,height=807; bool configured{},failed{}; uint32_t clock{},presented{},discarded{};
    ~App() {
        for (auto *f:pending) { wp_presentation_feedback_destroy(f->object); delete f; }
        for (auto &b:buffers) { if(b.object)wl_buffer_destroy(b.object); if(b.pixels)munmap(b.pixels,b.bytes); }
        if(toplevel)xdg_toplevel_destroy(toplevel);
        if(xdg)xdg_surface_destroy(xdg);
        if(surface)wl_surface_destroy(surface);
        if(presentation)wp_presentation_destroy(presentation);
        if(shell)xdg_wm_base_destroy(shell);
        if(shm)wl_shm_destroy(shm);
        if(compositor)wl_compositor_destroy(compositor);
        if(registry)wl_registry_destroy(registry);
        if(display)wl_display_disconnect(display);
    }
};
static void ping(void*,xdg_wm_base *shell,uint32_t serial){xdg_wm_base_pong(shell,serial);}
static const xdg_wm_base_listener shell_listener{ping};
static void clock_id(void *data,wp_presentation*,uint32_t clock){static_cast<App*>(data)->clock=clock;}
static const wp_presentation_listener clock_listener{clock_id};
static void global(void *data,wl_registry *registry,uint32_t id,const char *name,uint32_t version) {
    auto &a=*static_cast<App*>(data);
    if(!strcmp(name,wl_compositor_interface.name))a.compositor=static_cast<wl_compositor*>(wl_registry_bind(registry,id,&wl_compositor_interface,std::min(version,4U)));
    else if(!strcmp(name,wl_shm_interface.name))a.shm=static_cast<wl_shm*>(wl_registry_bind(registry,id,&wl_shm_interface,1));
    else if(!strcmp(name,xdg_wm_base_interface.name)) {
        a.shell=static_cast<xdg_wm_base*>(wl_registry_bind(registry,id,&xdg_wm_base_interface,1));
        xdg_wm_base_add_listener(a.shell,&shell_listener,&a);
    } else if(!strcmp(name,wp_presentation_interface.name)) {
        a.presentation=static_cast<wp_presentation*>(wl_registry_bind(registry,id,&wp_presentation_interface,1));
        wp_presentation_add_listener(a.presentation,&clock_listener,&a);
    }
}
static void removed(void*,wl_registry*,uint32_t){}
static const wl_registry_listener registry_listener{global,removed};
static void configure(void *data,xdg_surface *surface,uint32_t serial) {
    xdg_surface_ack_configure(surface,serial); static_cast<App*>(data)->configured=true;
}
static const xdg_surface_listener surface_listener{configure};
static void size(void *data,xdg_toplevel*,int32_t w,int32_t h,wl_array*) {
    auto &a=*static_cast<App*>(data);
    if(w<=0||h<=0)return;
    if(w>4096||h>4096 || (a.buffers[0].object&&(w!=a.width||h!=a.height)))a.failed=true;
    else {a.width=w;a.height=h;}
}
static void close_window(void*,xdg_toplevel*){stopped=1;}
static void bounds(void*,xdg_toplevel*,int32_t,int32_t){}
static void capabilities(void*,xdg_toplevel*,wl_array*){}
static const xdg_toplevel_listener toplevel_listener{size,close_window,bounds,capabilities};
static void release(void *data,wl_buffer*){static_cast<Buffer*>(data)->busy=false;}
static const wl_buffer_listener buffer_listener{release};
static void sync_output(void*,struct wp_presentation_feedback*,wl_output*){}
static void forget(Feedback *f){f->app->pending.erase(f);wp_presentation_feedback_destroy(f->object);delete f;}
static void presented(void *data,struct wp_presentation_feedback*,uint32_t hi,uint32_t lo,uint32_t ns,uint32_t refresh,uint32_t seq_hi,uint32_t seq_lo,uint32_t flags) {
    auto *f=static_cast<Feedback*>(data);
    ++f->app->presented;
    const uint64_t stamp=((uint64_t(hi)<<32)|lo)*1000000000ULL+ns;
    const uint64_t sequence=(uint64_t(seq_hi)<<32)|seq_lo;
    std::printf("present,%u,%u,%llu,%llu,%llu,%llu,%u,%u\n",f->frame,f->stage,
        (unsigned long long)f->requested,(unsigned long long)f->submitted,
        (unsigned long long)stamp,(unsigned long long)sequence,refresh,flags);forget(f);
}
static void discarded(void *data,struct wp_presentation_feedback*) {
    auto *f=static_cast<Feedback*>(data);++f->app->discarded;
    std::printf("discard,%u,%u\n",f->frame,f->stage);forget(f);
}
static void require_presentation(unsigned count) {
    if(!count)throw std::runtime_error("No frame was presented; unlock the session and expose the test window");
}
static const wp_presentation_feedback_listener feedback_listener{sync_output,presented,discarded};
static void allocate(App &a) {
    for(auto &b:a.buffers) {
        b.bytes=size_t(a.width)*a.height*4;
        const int fd=memfd_create("uke-vrr-lab",MFD_CLOEXEC);
        if(fd<0)throw std::runtime_error("memfd_create");
        if(ftruncate(fd,b.bytes)){close(fd);throw std::runtime_error("ftruncate");}
        void *map=mmap(nullptr,b.bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if(map==MAP_FAILED){close(fd);throw std::runtime_error("mmap");}
        b.pixels=static_cast<uint32_t*>(map);std::fill_n(b.pixels,b.bytes/4,0xff606060);
        auto *pool=wl_shm_create_pool(a.shm,fd,b.bytes);
        b.object=wl_shm_pool_create_buffer(pool,0,a.width,a.height,a.width*4,WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);close(fd);wl_buffer_add_listener(b.object,&buffer_listener,&b);
    }
}
int run(int argc,char **argv) {
    unsigned maximum=144;
    if(argc==2&&!strcmp(argv[1],"--self-test")) {
        assert(next_deadline(100,103,10)==110);
        assert(next_deadline(100,110,10)==120);
        assert(next_deadline(100,125,10)==135);
        for(unsigned max:{30U,60U,90U,144U})for(unsigned ms=0;ms<43000;ms+=17) {
            auto s=schedule(uint64_t(ms)*1000000,ms,max);
            if(!s.idle)assert(s.interval>=1000000000ULL/max&&s.interval<=1000000000ULL/30);
        }
        assert(schedule(33000000000ULL,0,144).idle);assert(schedule(43000000000ULL,0,144).done);
        bool refused=false;try{require_presentation(0);}catch(const std::exception&){refused=true;}
        assert(refused);require_presentation(1);
        std::puts("Bounded rates, irregular intervals, eight-second idle and termination passed");return 0;
    }
    if(argc==2) {char *end{};auto value=strtoul(argv[1],&end,10);if(*end||value<30||value>144)return 64;maximum=value;}
    else if(argc!=1)return 64;
    std::signal(SIGINT,[](int){stopped=1;});std::signal(SIGTERM,[](int){stopped=1;});
    try {
        App a;a.display=wl_display_connect(nullptr);if(!a.display)throw std::runtime_error("Wayland display");
        a.registry=wl_display_get_registry(a.display);wl_registry_add_listener(a.registry,&registry_listener,&a);
        if(wl_display_roundtrip(a.display)<0||!a.compositor||!a.shm||!a.shell||!a.presentation)throw std::runtime_error("Required original Wayland globals");
        a.surface=wl_compositor_create_surface(a.compositor);a.xdg=xdg_wm_base_get_xdg_surface(a.shell,a.surface);
        xdg_surface_add_listener(a.xdg,&surface_listener,&a);a.toplevel=xdg_surface_get_toplevel(a.xdg);
        xdg_toplevel_add_listener(a.toplevel,&toplevel_listener,&a);
        xdg_toplevel_set_title(a.toplevel,"Uke VRR laboratory — bounded 43-second cadence");
        xdg_toplevel_set_app_id(a.toplevel,"org.senemos.Uke.VrrLab");xdg_toplevel_set_fullscreen(a.toplevel,nullptr);wl_surface_commit(a.surface);
        const auto configure_deadline=now()+5000000000ULL;
        while(!a.configured&&!a.failed&&!stopped&&now()<configure_deadline) {
            pollfd fd{wl_display_get_fd(a.display),POLLIN,0};wl_display_flush(a.display);
            if(poll(&fd,1,100)>0&&wl_display_dispatch(a.display)<0)throw std::runtime_error("Configure dispatch");
        }
        if(!a.configured||a.failed||stopped)throw std::runtime_error("Fullscreen configure timeout or unsupported size");
        allocate(a);const auto start=now();uint64_t deadline=start;unsigned frame=0,last_stage=99;
        std::printf("# clock_id=%u maximum_requested_fps=%u width=%d height=%d\n",a.clock,maximum,a.width,a.height);
        std::puts("# present,frame,stage,requested_interval_ns,submit_monotonic_ns,presentation_ns,sequence,refresh_ns,flags");
        while(!stopped&&!a.failed) {
            if(wl_display_dispatch_pending(a.display)<0)throw std::runtime_error("Pending dispatch");
            if(now()-start>5000000000ULL)require_presentation(a.presented);
            auto s=schedule(now()-start,frame,maximum);if(s.done)break;
            if(s.stage!=last_stage){std::printf("stage,%u,%llu\n",s.stage,(unsigned long long)now());last_stage=s.stage;}
            if(now()>=deadline) {
                Buffer *buffer=nullptr;for(auto &b:a.buffers)if(!b.busy){buffer=&b;break;}
                if(!s.idle&&buffer) {
                    for(int y=64;y<std::min(a.height,128);y++)for(int x=0;x<a.width;x++) {
                        const unsigned bit=unsigned(x%(a.width/2))*16/(a.width/2);
                        buffer->pixels[y*a.width+x]=(frame&(1U<<bit))?0xffffffff:0xff101010;
                    }
                    const auto submitted=now();auto *f=new Feedback{&a,wp_presentation_feedback(a.presentation,a.surface),frame,s.stage,submitted,s.interval};
                    a.pending.insert(f);wp_presentation_feedback_add_listener(f->object,&feedback_listener,f);
                    buffer->busy=true;wl_surface_attach(a.surface,buffer->object,0,0);wl_surface_damage(a.surface,0,0,a.width,a.height);
                    wl_surface_commit(a.surface);++frame;
                } else if(!s.idle)std::printf("busy,%u,%u\n",frame,s.stage);
                // Absolute targets exclude rendering cost without a catch-up burst.
                deadline=next_deadline(deadline,now(),s.interval);
            }
            while(wl_display_prepare_read(a.display)!=0)if(wl_display_dispatch_pending(a.display)<0)throw std::runtime_error("Prepare dispatch");
            const int flushed=wl_display_flush(a.display);const bool wants_write=flushed<0&&errno==EAGAIN;
            if(flushed<0&&!wants_write){wl_display_cancel_read(a.display);throw std::runtime_error("Wayland flush");}
            const auto current=now();const uint64_t wait=deadline>current?std::min<uint64_t>(deadline-current,50000000):0;
            const timespec timeout{0,long(wait)};
            pollfd fd{wl_display_get_fd(a.display),short(POLLIN|(wants_write?POLLOUT:0)),0};
            const int ready=ppoll(&fd,1,&timeout,nullptr);
            if(ready>0&&(fd.revents&POLLIN)){if(wl_display_read_events(a.display)<0)throw std::runtime_error("Read events");}
            else wl_display_cancel_read(a.display);
            if((ready<0&&errno!=EINTR)||(fd.revents&(POLLERR|POLLHUP|POLLNVAL)))throw std::runtime_error("Display poll");
        }
        if(a.failed)throw std::runtime_error("Output size changed");
        require_presentation(a.presented);
        std::printf("done,%u,%llu,presented=%u,discarded=%u\n",frame,(unsigned long long)now(),a.presented,a.discarded);
    }catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}
    return 0;
}
}
