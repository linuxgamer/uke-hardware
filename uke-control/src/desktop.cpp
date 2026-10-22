// SPDX-License-Identifier: MIT
#include "desktop.hpp"
#include "sysfs.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>
namespace uke::desktop {
namespace {
using Message = std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)>;
using Bus = std::unique_ptr<sd_bus, decltype(&sd_bus_unref)>;
using uke::sysfs::quote;
struct Field { const char *name, *signature, *unit; bool positive_only = false; };
uint64_t now() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
class Reader {
    uint64_t started_ = now(), deadline_ = started_ + 5000000;
    bool enumeration_truncated_ = false;
    std::vector<std::string> objects_;
    Message call(sd_bus *bus, const char *service, const char *path, const char *interface,
                 const char *method, const char *argument = nullptr) const {
        sd_bus_message *raw = nullptr, *reply = nullptr;
        if (!bus || now() >= deadline_ || sd_bus_message_new_method_call(bus, &raw, service, path, interface, method) < 0)
            return {nullptr, sd_bus_message_unref};
        Message request(raw, sd_bus_message_unref);
        sd_bus_message_set_auto_start(raw, 0);
        if (argument && sd_bus_message_append(raw, "s", argument) < 0) return {nullptr, sd_bus_message_unref};
        const auto current = now();
        if (current >= deadline_ || sd_bus_call(bus, raw, std::min<uint64_t>(250000, deadline_ - current), nullptr, &reply) < 0)
            return {nullptr, sd_bus_message_unref};
        return {reply, sd_bus_message_unref};
    }
    std::string owner(sd_bus *bus, const char *service) const {
        auto reply = call(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetNameOwner", service);
        const char *value = nullptr;
        return reply && sd_bus_message_read(reply.get(), "s", &value) > 0 && value ? value : "";
    }
    static std::string value(sd_bus_message *reply, const Field &field) {
        const char *signature = nullptr; char type = 0;
        if (sd_bus_message_peek_type(reply, &type, &signature) <= 0 || type != 'v') return "null";
        if (!signature || std::strcmp(signature, field.signature)) { sd_bus_message_skip(reply, "v"); return "null"; }
        if (sd_bus_message_enter_container(reply, 'v', signature) <= 0) return "null";
        std::string result = "null";
        if (*signature == 's') {
            const char *text = nullptr;
            if (sd_bus_message_read(reply, "s", &text) > 0 && text && std::strlen(text) <= 512) result = quote(text);
        } else if (*signature == 'b') {
            int b = 0; if (sd_bus_message_read(reply, "b", &b) > 0) result = b ? "true" : "false";
        } else {
            double n = NAN;
            if (*signature == 'd') sd_bus_message_read(reply, "d", &n);
            else if (*signature == 'u') { uint32_t v = 0; if (sd_bus_message_read(reply, "u", &v) > 0) n = v; }
            else if (*signature == 'x') { int64_t v = 0; if (sd_bus_message_read(reply, "x", &v) > 0) n = double(v); }
            else if (*signature == 'i') { int32_t v = 0; if (sd_bus_message_read(reply, "i", &v) > 0) n = v; }
            if (std::isfinite(n) && (n >= 0 || (!std::strcmp(field.name,"Temperature") && n >= -273.15)) &&
                (!field.positive_only || n > 0) &&
                (std::strcmp(field.name, "State") || n <= 6) &&
                (std::strcmp(field.name, "Percentage") || (n >= 0 && n <= 100))) {
                std::ostringstream number; number << std::setprecision(17) << n; result = number.str();
            }
        }
        if (sd_bus_message_exit_container(reply) < 0) return "null";
        return result;
    }
public:
    void properties(sd_bus *bus, const char *service, const char *path, const char *interface,
                    const std::vector<Field> &fields) {
        const auto unique = owner(bus, service);
        auto reply = unique.empty() ? Message(nullptr, sd_bus_message_unref) :
            call(bus, unique.c_str(), path, "org.freedesktop.DBus.Properties", "GetAll", interface);
        std::vector<std::string> values(fields.size(), "null");
        bool valid = reply && sd_bus_message_enter_container(reply.get(), 'a', "{sv}") > 0;
        unsigned count = 0;
        if (valid) {
            int next;
            while ((next = sd_bus_message_enter_container(reply.get(), 'e', "sv")) > 0) {
                const char *name = nullptr;
                if (++count > 128 || sd_bus_message_read(reply.get(), "s", &name) <= 0 || !name) { valid = false; break; }
                const auto field = std::find_if(fields.begin(), fields.end(), [&](const auto &f) { return !std::strcmp(f.name, name); });
                if (field == fields.end()) sd_bus_message_skip(reply.get(), "v");
                else values[size_t(field - fields.begin())] = value(reply.get(), *field);
                if (sd_bus_message_exit_container(reply.get()) < 0) { valid = false; break; }
            }
            if (next < 0 || sd_bus_message_exit_container(reply.get()) < 0) valid = false;
            if (unique != owner(bus, service)) valid = false;
        }
        for (size_t i = 0; i < fields.size(); ++i) {
            if (std::strcmp(fields[i].name,"ChargeStartThreshold") && std::strcmp(fields[i].name,"ChargeEndThreshold")) continue;
            const auto support = std::find_if(fields.begin(),fields.end(),[](const auto &f){return !std::strcmp(f.name,"ChargeThresholdSupported");});
            if (support == fields.end() || values[size_t(support-fields.begin())] != "true") values[i] = "null";
        }
        std::string json = "{\"service\":" + quote(service) + ",\"path\":" + quote(path) +
            ",\"interface\":" + quote(interface) + ",\"available\":" + (valid ? "true" : "false") +
            ",\"reason\":" + quote(valid ? "cached_standard_interface" : unique.empty() ? "service_not_running" : "query_or_owner_unavailable") +
            ",\"properties\":{";
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i) json += ',';
            json += quote(fields[i].name) + ":{\"value\":" + (valid ? values[i] : "null") +
                ",\"unit\":" + quote(fields[i].unit) + ",\"state\":" + quote(valid && values[i] != "null" ? "reported" : "unknown_or_unsupported") + '}';
        }
        objects_.push_back(json + "}}");
    }
    void batteries(sd_bus *bus) {
        const auto unique = owner(bus,"org.freedesktop.UPower");
        if (unique.empty()) return;
        auto reply = call(bus,unique.c_str(),"/org/freedesktop/UPower","org.freedesktop.UPower","EnumerateDevices");
        if (!reply || sd_bus_message_enter_container(reply.get(),'a',"o") <= 0) return;
        const char *path = nullptr;unsigned count = 0;
        while (sd_bus_message_read(reply.get(),"o",&path) > 0) {
            if (++count > 16) { enumeration_truncated_ = true; break; }
            if (!path || std::strlen(path)>512 || !std::string_view(path).starts_with("/org/freedesktop/UPower/devices/battery_")) continue;
            properties(bus,"org.freedesktop.UPower",path,"org.freedesktop.UPower.Device",
                {{"Type","u","UPower device enum"},{"PowerSupply","b",""},{"Percentage","d","percent"},
                 {"Temperature","d","C"},{"Voltage","d","V"},{"Capacity","d","reported retention percent",true},
                 {"ChargeCycles","i","reported hardware cycles",true},{"ChargeThresholdSupported","b","interface capability"},
                 {"ChargeThresholdEnabled","b","reported setting; physical effect unverified"},
                 {"ChargeStartThreshold","u","configured percent; not effective cutoff"},
                 {"ChargeEndThreshold","u","configured percent; not effective cutoff"}});
        }
    }
    void method(sd_bus *bus, const char *service, const char *path, const char *interface, const char *method, const char *signature, const char *unit) {
        const auto unique = owner(bus, service);
        auto reply = unique.empty() ? Message(nullptr, sd_bus_message_unref) : call(bus, unique.c_str(), path, interface, method);
        std::string result = "null";
        if (reply && !std::strcmp(signature, "s")) {
            const char *s = nullptr; if (sd_bus_message_read(reply.get(), "s", &s) > 0 && s && std::strlen(s) <= 512) result = quote(s);
        } else if (reply && !std::strcmp(signature, "t")) {
            uint64_t n = 0; if (sd_bus_message_read(reply.get(), "t", &n) > 0 && n) result = std::to_string(n);
        }
        if (unique != owner(bus, service)) result = "null";
        objects_.push_back("{\"service\":" + quote(service) + ",\"method\":" + quote(method) +
            ",\"value\":" + result + ",\"unit\":" + quote(unit) + ",\"available\":" + (result == "null" ? "false" : "true") + '}');
    }
    std::string json() const {
        std::string json = "{\"schema\":\"org.uke.control.desktop.v1\",\"read_only\":true,\"service_activation\":false,"
            "\"sensor_claims\":false,\"physical_acceptance\":false,\"snapshot_atomic\":false,\"cache_age_ms\":null,"
            "\"observed_monotonic_us\":" + std::to_string(started_) + ",\"collection_span_ms\":" + std::to_string((now()-started_)/1000) +
            ",\"device_enumeration_truncated\":" + (enumeration_truncated_ ? "true" : "false") + ",\"interfaces\":[";
        for (size_t i = 0; i < objects_.size(); ++i) { if (i) json += ','; json += objects_[i]; }
        return json + "]}\n";
    }
};
}
std::string snapshot(sd_bus *system, sd_bus *session) {
    Reader reader;
    reader.properties(system, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower",
        {{"DaemonVersion","s",""},{"OnBattery","b",""},{"LidIsClosed","b",""}});
    reader.properties(system, "org.freedesktop.UPower", "/org/freedesktop/UPower/devices/DisplayDevice", "org.freedesktop.UPower.Device",
        {{"State","u","UPower state enum"},{"Percentage","d","percent"},{"Energy","d","Wh"},{"EnergyFull","d","Wh"},
         {"EnergyRate","d","W magnitude; direction is State"},{"TimeToFull","x","s",true},{"TimeToEmpty","x","s",true}});
    reader.batteries(system);
    reader.method(system, "org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", "GetCriticalAction", "s", "action");
    reader.properties(system, "org.freedesktop.UPower.PowerProfiles", "/org/freedesktop/UPower/PowerProfiles", "org.freedesktop.UPower.PowerProfiles",
        {{"ActiveProfile","s","profile"},{"PerformanceDegraded","s","reason"},{"PerformanceInhibited","s","reason"},{"BatteryAware","b",""}});
    reader.properties(system, "net.hadess.SensorProxy", "/net/hadess/SensorProxy", "net.hadess.SensorProxy",
        {{"HasAccelerometer","b","interface capability"},{"HasAmbientLight","b","interface capability"},{"HasProximity","b","interface capability"}});
    reader.method(system, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "CanSuspend", "s", "permission/capability; not a suspend test");
    reader.method(system, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "CanHibernate", "s", "permission/capability; not a hibernation test");
    reader.method(session, "org.kde.Solid.PowerManagement", "/org/kde/Solid/PowerManagement", "org.kde.Solid.PowerManagement", "batteryRemainingTime", "t", "ms");
    return reader.json();
}
std::string status() {
    sd_bus *system = nullptr, *session = nullptr;
    sd_bus_open_system(&system); sd_bus_open_user(&session);
    Bus system_bus(system, sd_bus_unref), session_bus(session, sd_bus_unref);
    return snapshot(system, session);
}
}
