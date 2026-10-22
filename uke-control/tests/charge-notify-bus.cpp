// SPDX-License-Identifier: MIT
// Run only under dbus-run-session: validates real wire messages on a private bus.
#include "../src/charge-notify.cpp"
#include <atomic>
#include <cassert>
#include <cstring>
namespace uke::charge { Result control_query(const std::string &) { throw std::runtime_error("Unexpected helper execution"); } }
static unsigned calls;
static int handle(sd_bus_message *m,void *,sd_bus_error *) {
    assert(std::strcmp(sd_bus_message_get_signature(m,true),"susssasa{sv}i")==0);
    const char *app,*icon,*title,*body,*key;uint32_t replaced;uint8_t urgency;int32_t timeout;
    assert(sd_bus_message_read(m,"susss",&app,&replaced,&icon,&title,&body)>0);
    assert(std::strcmp(app,"Uke Control")==0 && replaced==(calls?42U:0U));
    assert(std::strcmp(icon,"dialog-warning")==0 && *title && *body);
    assert(sd_bus_message_enter_container(m,'a',"s")>0 && sd_bus_message_at_end(m,false)>0);
    assert(sd_bus_message_exit_container(m)>0);
    assert(sd_bus_message_enter_container(m,'a',"{sv}")>0);
    assert(sd_bus_message_enter_container(m,'e',"sv")>0 && sd_bus_message_read(m,"s",&key)>0);
    assert(std::strcmp(key,"urgency")==0 && sd_bus_message_enter_container(m,'v',"y")>0);
    assert(sd_bus_message_read(m,"y",&urgency)>0);
    assert(sd_bus_message_exit_container(m)>0 && sd_bus_message_exit_container(m)>0 && sd_bus_message_exit_container(m)>0);
    assert(sd_bus_message_read(m,"i",&timeout)>0);
    assert(urgency==(calls==2?1:2) && timeout==(calls==2?8000:0));
    if(calls==1) assert(std::strstr(body,"could not be verified"));
    ++calls;return sd_bus_reply_method_return(m,"u",calls==4?0U:42U);
}
int main() {
    sd_bus *server=nullptr,*client=nullptr;sd_bus_slot *slot=nullptr;
    assert(sd_bus_open_user(&server)>=0 && sd_bus_open_user(&client)>=0);
    static const sd_bus_vtable table[]={SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("Notify","susssasa{sv}i","u",handle,SD_BUS_VTABLE_UNPRIVILEGED),SD_BUS_VTABLE_END};
    assert(sd_bus_add_object_vtable(server,&slot,"/org/freedesktop/Notifications","org.freedesktop.Notifications",table,nullptr)>=0);
    assert(sd_bus_request_name(server,"org.freedesktop.Notifications",0)>=0);
    std::atomic<bool> done=false;
    std::thread worker([&]{while(!done){const int r=sd_bus_process(server,nullptr);assert(r>=0);if(!r)assert(sd_bus_wait(server,10000)>=0);}});
    uint32_t id=0;
    assert(uke::charge::notify(client,id,uke::charge::MoistureNotice::wet_inhibited) && id==42);
    assert(uke::charge::notify(client,id,uke::charge::MoistureNotice::wet_unverified) && id==42);
    assert(uke::charge::notify(client,id,uke::charge::MoistureNotice::cleared) && id==42);
    assert(!uke::charge::notify(client,id,uke::charge::MoistureNotice::wet_inhibited));
    done=true;worker.join();assert(calls==4);
    sd_bus_slot_unref(slot);sd_bus_unref(client);sd_bus_unref(server);
}
