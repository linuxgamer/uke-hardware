// SPDX-License-Identifier: MIT
#pragma once
#include "io.hpp"
#include "screen.hpp"
#include "../battery/provider.hpp"
#include <charconv>
#include <chrono>
#include <cerrno>
#include <limits.h>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace uke::offline {
inline std::optional<std::int64_t> number(const std::string &path) {
    const auto raw=read(path,64);
    const auto text=trim(raw);
    std::int64_t value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<INT32_MIN || value>INT32_MAX) return {};
    return value;
}
inline std::string sample_epoch(const std::filesystem::path &provider) {
    unsigned version,generation,firmware,source,up,desynced,suspended,removing,raw;
    int error; std::uint64_t receipt,instance;
    std::istringstream in(read((provider/"uke_battmgr_state").string(),512));
    if(!(in>>version>>generation>>firmware>>source>>up>>desynced>>suspended>>removing>>error>>raw>>receipt>>instance) ||
       !(in>>std::ws).eof() || version!=1 || up!=1 || desynced || suspended || removing || !instance) return {};
    return std::to_string(generation)+":"+std::to_string(firmware)+":"+std::to_string(source)+":"+std::to_string(instance);
}
inline Snapshot sample(const std::string &root="/sys") {
    const std::filesystem::path bat=root+"/class/power_supply/qcom-battmgr-bat", usb=root+"/class/power_supply/qcom-battmgr-usb";
    const auto provider=battery::provider::find(bat,root);
    if(provider.empty() || battery::provider::find(usb,root)!=provider) return {};
    const auto first=sample_epoch(provider);
    if(first.empty()) return {};
    Snapshot s;
    const auto percent=number((bat/"capacity").string()),present=number((bat/"present").string());
    const auto online=number((usb/"online").string());
    s.status=trim(read((bat/"status").string(),64)); s.health=trim(read((bat/"health").string(),64));
    s.valid=percent && *percent>=0 && *percent<=100 && present && *present==1 && !s.status.empty() && !s.health.empty();
    if(s.valid) s.percent=*percent;
    s.input_known=online && (*online==0 || *online==1); s.online=s.input_known && *online==1;
    s.power_uw=number((bat/"power_now").string());
    s.power_estimated=bool(s.power_uw); // The matched Uke driver also computes power_now from V*I.
    // Uke has no firmware power_now; label V*I as an estimate, never a raw measurement.
    if(!s.power_uw) {
        const auto voltage=number((bat/"voltage_now").string()),current=number((bat/"current_now").string());
        if(voltage && *voltage>0 && current) { s.power_uw=*voltage * *current / 1000000; s.power_estimated=true; }
    }
    s.temperature_decic=number((bat/"temp").string());
    const auto seconds=number((bat/"time_to_full_avg").string());
    if(seconds && *seconds>0 && *seconds<=9999*60) s.full_seconds=seconds;
    if(first!=sample_epoch(provider) || battery::provider::find(bat,root)!=provider || battery::provider::find(usb,root)!=provider) return {};
    return s;
}

// One bounded child keeps a blocking driver read away from the power-key loop.
class Sampler {
    using Clock=std::chrono::steady_clock;
    struct Wire {
        bool valid,input_known,online,power_estimated,has_power,has_temp,has_time;
        int percent;
        std::int64_t power,temp,time;
        char status[32],health[32];
    };
    static_assert(sizeof(Wire)<=PIPE_BUF);
    pid_t child_=-1;
    int fd_=-1;
    bool reported_=false;
    Clock::time_point deadline_{},next_{};
    void close_pipe() { if(fd_>=0) close(fd_); fd_=-1; }
public:
    ~Sampler() { close_pipe(); if(child_>0) { kill(child_,SIGKILL); waitpid(child_,nullptr,WNOHANG); } }
    Sampler()=default;
    Sampler(const Sampler&)=delete;
    Sampler& operator=(const Sampler&)=delete;
    int descriptor() const { return fd_; }
    template<class Producer> std::optional<Snapshot> update(Producer produce) {
        const auto now=Clock::now();
        if(child_<0 && now>=next_) {
            int pipefd[2];
            next_=now+std::chrono::seconds(3);
            if(pipe2(pipefd,O_CLOEXEC|O_NONBLOCK)) return Snapshot{};
            child_=fork();
            if(child_==0) {
                close(pipefd[0]);
                try {
                    const auto s=produce();
                    Wire w{};
                    w.valid=s.valid; w.input_known=s.input_known; w.online=s.online; w.percent=s.percent;
                    w.power_estimated=s.power_estimated; w.has_power=bool(s.power_uw); w.power=s.power_uw.value_or(0);
                    w.has_temp=bool(s.temperature_decic); w.temp=s.temperature_decic.value_or(0);
                    w.has_time=bool(s.full_seconds); w.time=s.full_seconds.value_or(0);
                    if(s.status.size()>=sizeof(w.status) || s.health.size()>=sizeof(w.health)) _exit(1);
                    std::memcpy(w.status,s.status.c_str(),s.status.size()+1); std::memcpy(w.health,s.health.c_str(),s.health.size()+1);
                    _exit(write(pipefd[1],&w,sizeof(w))==sizeof(w)?0:1);
                } catch(...) { _exit(1); }
            }
            close(pipefd[1]);
            if(child_<0) { close(pipefd[0]); return Snapshot{}; }
            fd_=pipefd[0]; deadline_=now+std::chrono::seconds(2); reported_=false;
        }
        std::optional<Snapshot> result;
        if(fd_>=0) {
            Wire w{}; const auto bytes=::read(fd_,&w,sizeof(w));
            if(bytes==sizeof(w)) {
                Snapshot s;
                s.valid=w.valid; s.input_known=w.input_known; s.online=w.online; s.percent=w.percent;
                s.status=w.status; s.health=w.health; s.power_estimated=w.power_estimated;
                if(w.has_power) s.power_uw=w.power;
                if(w.has_temp) s.temperature_decic=w.temp;
                if(w.has_time) s.full_seconds=w.time;
                result=s;
            } else if(bytes==0 || (bytes<0 && errno!=EAGAIN && errno!=EINTR) || bytes>0 || now>=deadline_) {
                kill(child_,SIGKILL); result=Snapshot{};
            }
            if(result) { close_pipe(); reported_=true; }
        }
        if(child_>0) {
            int status=0; const auto reaped=waitpid(child_,&status,WNOHANG);
            if(reaped==child_ || (reaped<0 && errno==ECHILD)) {
                child_=-1; close_pipe();
                if(!reported_) result=Snapshot{};
            }
        }
        return result;
    }
};
}
