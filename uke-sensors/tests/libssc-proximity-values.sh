#!/usr/bin/env bash
# Exercise the actual callback with decoded-message and delivery boundaries mocked.
set -Eeuo pipefail
source=${1:?Pass prepared libssc-sensor-proximity.c}
work=$(mktemp -d); trap 'rm -rf -- "$work"' EXIT
{
cat <<'CPP'
#include <glib.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include "libssc-sensor-proximity-private.h"
struct SSCClient {};
struct SSCSensorProximityPrivate { gboolean near=false, reported_once=false; };
struct SSCSensorProximity { SSCSensorProximityPrivate state; };
struct SignalContext { SSCSensorProximity *sensor; gboolean near; };
struct SscProximityResponse { gint32 near; };
struct SscProximityResponseDavinci { GArray *data; };
static SscProximityResponse ordinary;
static ProximityDataDavinci vendor_data;
static GArray vendor_bytes{reinterpret_cast<gchar *>(&vendor_data),sizeof(vendor_data)};
static SscProximityResponseDavinci vendor{&vendor_bytes};
static bool unpack=true;
static unsigned freed=0, delivered=0, warnings=0;
static gboolean last_near;
#define SSC_SENSOR(x) (x)
#define SSC_SENSOR_PROXIMITY(x) static_cast<SSCSensorProximity *>(x)
#define SSC_SENSOR_UID_HIGH "high"
#define SSC_SENSOR_UID_LOW "low"
#define SSC_MSG_REPORT_MEASUREMENT_PROXIMITY 1025
#define SSC_MSG_REPORT_MEASUREMENT 769
#define proximity_unpack_context(...) ((void)0)
#undef g_warning
#define g_warning(...) (++warnings)
static void g_object_get(SSCSensorProximity *,const char *,guint64 *high,
                         const char *,guint64 *low,void *) { *high=42; *low=43; }
static SSCSensorProximityPrivate *ssc_sensor_proximity_get_instance_private(SSCSensorProximity *s) { return &s->state; }
static SscProximityResponse *ssc_proximity_response__unpack(void *,guint,const uint8_t *) { return unpack?&ordinary:nullptr; }
static SscProximityResponseDavinci *ssc_proximity_response_davinci__unpack(void *,guint,const uint8_t *) { return unpack?&vendor:nullptr; }
static void ssc_proximity_response__free_unpacked(SscProximityResponse *,void *) { ++freed; }
static void ssc_proximity_response_davinci__free_unpacked(SscProximityResponseDavinci *,void *) { ++freed; }
static gboolean emit_signal(gpointer data) { ++delivered; last_near=static_cast<SignalContext *>(data)->near; return G_SOURCE_REMOVE; }
static void signal_context_free(gpointer data) { auto *ctx=static_cast<SignalContext *>(data); g_slice_free(SignalContext,ctx); }
static guint deliver(gint,GSourceFunc callback,gpointer data,GDestroyNotify destroy) { callback(data); destroy(data); return 1; }
#define g_idle_add_full deliver
CPP
awk '/^#define SSC_SENSOR_PROXIMITY_(NEAR|FAR)[[:space:]]/ {print}' "$source"
awk '/^report_received \(/ {emit=1; print "static void"} emit {print} emit && /^}/ {found=1;exit} END {if(!found)exit 1}' "$source"
cat <<'CPP'
static void sample(guint event,bool valid,bool expected=false) {
    SSCSensorProximity sensor; GArray payload{nullptr,0};
    freed=delivered=warnings=0;
    report_received(nullptr,event,42,43,&payload,&sensor);
    assert(freed==1 && delivered==unsigned(valid));
    assert(sensor.state.reported_once==valid);
    if(valid) assert(last_near==expected && !warnings);
}
int main() {
    for(int value:{0,1}) {
        ordinary.near=value; sample(1025,true,value==1);
        vendor_data.near=static_cast<float>(value); sample(769,true,value==1);
    }
    for(int value:{-1,2,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
        ordinary.near=value; sample(1025,false); assert(warnings);
    }
    for(float value:{-.5f,.5f,1.5f,2.0f,std::numeric_limits<float>::infinity(),
                     -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::max()}) {
        vendor_data.near=value; sample(769,false); assert(warnings);
    }
    vendor_bytes.len=0; sample(769,false); vendor_bytes.len=sizeof(vendor_data);
    SSCSensorProximity sensor; GArray payload{nullptr,0};
    ordinary.near=1; freed=delivered=0;
    report_received(nullptr,1025,42,43,&payload,&sensor);
    report_received(nullptr,1025,42,43,&payload,&sensor);
    assert(freed==2 && delivered==1);
    ordinary.near=2; report_received(nullptr,1025,42,43,&payload,&sensor);
    vendor_data.near=.5f; report_received(nullptr,769,42,43,&payload,&sensor);
    assert(freed==4 && delivered==1 && sensor.state.near && sensor.state.reported_once);
    ordinary.near=0; report_received(nullptr,1025,42,43,&payload,&sensor);
    assert(freed==5 && delivered==2 && !last_near);
    freed=delivered=0;
    report_received(nullptr,1025,0,43,&payload,&sensor);
    report_received(nullptr,99,42,43,&payload,&sensor);
    assert(!freed && !delivered);
    unpack=false;
    report_received(nullptr,1025,42,43,&payload,&sensor);
    report_received(nullptr,769,42,43,&payload,&sensor);
    assert(!freed && !delivered);
}
CPP
} >"$work/check.cpp"
flags=(-std=c++20 -UNDEBUG -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer); fi
"${CXX:-c++}" "${flags[@]}" -I "$(dirname -- "$source")" "$work/check.cpp" \
    $(pkg-config --cflags --libs glib-2.0) -o "$work/check"
"$work/check"
echo 'Actual proximity callback preserves near/far delivery, rejects invalid integer/float states and releases decoded messages; no device input'
