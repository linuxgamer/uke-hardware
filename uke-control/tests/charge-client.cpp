// SPDX-License-Identifier: MIT
#include "charge-client.hpp"
#include <cassert>
#include <deque>
#include <iostream>
#include <stdexcept>

namespace {
std::deque<uke::Result> responses;
std::vector<std::vector<std::string>> calls;
void reset(std::initializer_list<uke::Result> results) {responses=results;calls.clear();}
}
namespace uke {
Result execute(const std::vector<std::string>& arguments, unsigned seconds, size_t limit) {
    assert(seconds>0&&seconds<=40&&limit<=65536);
    assert(!responses.empty());calls.push_back(arguments);
    auto result=responses.front();responses.pop_front();return result;
}
}
int main() {
    using uke::charge::control_mode;
    reset({{0,"{\"operation\":\"metadata_only\",\"physical_charge_stopped\":null}\n"}});
    const auto status=uke::charge::control_query("status");
    assert(status.status==0&&status.output.find("metadata_only")!=std::string::npos);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","status"}}));
    reset({{69,"metadata unavailable"}});
    assert(uke::charge::control_query("status").status==69&&calls.size()==1);
    reset({{0,"{\"kind\":\"uke_charge_settings\",\"applied\":false}\n"}});
    assert(uke::charge::control_query("settings").status==0);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","settings"}}));
    reset({{0,"cached protection"}});
    assert(uke::charge::control_query("protection-status").status==0);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","protection-status"}}));
    reset({{0,"moisture enabled"}});
    assert(uke::charge::enable_moisture().status==0);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","moisture-enable"}}));
    reset({});
    bool mutation_rejected=false;
    try{uke::charge::control_query("moisture-enable");}catch(const std::exception&){mutation_rejected=true;}
    assert(mutation_rejected&&calls.empty());
    reset({});
    bool query_rejected=false;
    try{uke::charge::control_query("verify");}catch(const std::exception&){query_rejected=true;}
    assert(query_rejected&&calls.empty());
    reset({{0,"{\"request_sent\":true,\"applied\":null}\n"}});
    const auto applied=uke::charge::apply_preconfigured_cap();
    assert(applied.status==0&&applied.output.find("\"applied\":null")!=std::string::npos);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","apply-cap80"}}));
    reset({{69,"uncertain"}});
    assert(uke::charge::apply_preconfigured_cap().status==69&&calls.size()==1);
    reset({});query_rejected=false;
    try{uke::charge::control_query("apply-cap80");}catch(const std::exception&){query_rejected=true;}
    assert(query_rejected&&calls.empty());
    reset({{69,"restricted cap unavailable"}});
    assert(uke::charge::enable_preconfigured_cap().status==69);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","enable-cap80"}}));
    reset({});query_rejected=false;
    try{uke::charge::control_query("enable-cap80");}catch(const std::exception&){query_rejected=true;}
    assert(query_rejected&&calls.empty());
    reset({{0,"{\"applied\":true}\n"}});
    assert(control_mode("charge80").status==0);
    assert((calls==std::vector<std::vector<std::string>>{{"/usr/libexec/uke-charge-control","cap80"}}));
    reset({{69,"unqualified"}});assert(control_mode("hold80-90").status==69);
    assert(calls[0][1]=="band80-90");
    for(const char* reply:{"{}", "bad json", "{\"kind\":\"foreign\",\"automatic_admission_ready\":true}",
        "{\"kind\":\"uke_charge_control_admission\",\"automatic_admission_ready\":false}",
        "{\"kind\":\"uke_charge_control_admission\",\"automatic_admission_ready\":\"true\"}"}) {
        reset({{0,reply}});assert(control_mode("automatic").status==69);assert(calls.size()==1);
    }
    const std::string automatic="{\"kind\":\"uke_charge_control_admission\",\"automatic_admission_ready\":true}";
    reset({{69,automatic}});assert(control_mode("automatic").status==69&&calls.size()==1);
    reset({{0,automatic+std::string("\0garbage",8)}});assert(control_mode("automatic").status==69&&calls.size()==1);
    reset({{0,automatic},{1,"start failure"}});assert(control_mode("automatic").status==69&&calls.size()==2);
    reset({{0,automatic},{0,""},{0,"ActiveState=failed\nSubState=failed\n"}});
    assert(control_mode("automatic").status==69);
    reset({{0,automatic},{0,""},{0,"SubState=running\nActiveState=active\n"}});
    auto started=control_mode("automatic");assert(started.status==0&&started.output.find("\"applied\":null")!=std::string::npos);
    assert((calls[1]==std::vector<std::string>{"/usr/bin/systemctl","start","uke-charge-automatic.service"}));
    const std::string qualified="{\"kind\":\"uke_charge_control_admission\",\"profile_qualified\":true}";
    reset({{0,qualified},{1,"stop failed"}});assert(control_mode("slow").status==69&&calls.size()==2);
    reset({{0,qualified},{0,""},{0,"applied"}});assert(control_mode("slow").status==0&&calls.size()==3);
    assert(calls[1][1]=="stop"&&calls[2][1]=="slow");
    reset({});assert(control_mode("firmware").status==69&&calls.empty());
    bool rejected=false;try{control_mode("raw-voltage");}catch(const std::exception&){rejected=true;}
    assert(rejected&&calls.empty());
    std::cout<<"Charge-client admission, failed-service handoff and applied-state boundaries passed; no device operation\n";
}
