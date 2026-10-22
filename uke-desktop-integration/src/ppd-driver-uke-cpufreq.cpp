// SPDX-License-Identifier: GPL-3.0-or-later
// User preferences use kernel QoS; firmware OPPs and thermal requests still win.
#include "ppd-driver-uke-cpufreq.h"
extern "C" {
#include "ppd-utils.h"
#include <glib/gstdio.h>
#include <gudev/gudev.h>
}
#include <cerrno>
#include <cstring>
#include <initializer_list>

struct _PpdDriverUkeCpufreq {
    PpdDriverCpu parent_instance;
    GUdevClient *client;
    bool awaiting_provider;
    const char *gpu_preference;
};
G_DEFINE_TYPE(PpdDriverUkeCpufreq, ppd_driver_uke_cpufreq, PPD_TYPE_DRIVER_CPU)
static constexpr unsigned policies[] = {0, 3, 7};
static constexpr const char *gpu = "/sys/class/devfreq/3d00000.qcom,kgsl-3d0/";

enum class NativeHandoff { legacy, pending, ready };
static NativeHandoff native_handoff() {
    g_autofree char *version_path = ppd_utils_get_sysfs_path("/sys/module/uke_platform_profile/version");
    g_autofree char *version = nullptr;
    g_autoptr(GError) error = nullptr;
    if (!g_file_get_contents(version_path, &version, nullptr, &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) return NativeHandoff::pending;
        g_autofree char *module = g_path_get_dirname(version_path);
        GStatBuf metadata{};
        return g_stat(module, &metadata) < 0 && errno == ENOENT ? NativeHandoff::legacy : NativeHandoff::pending;
    }
    if (strcmp(g_strchomp(version), "1")) return NativeHandoff::pending;
    g_autofree char *status_path = ppd_utils_get_sysfs_path("/sys/module/uke_platform_profile/parameters/probe_status");
    g_autofree char *status = nullptr;
    if (!g_file_get_contents(status_path, &status, nullptr, nullptr)) return NativeHandoff::pending;
    g_strchomp(status);
    if (!strcmp(status, "-1")) return NativeHandoff::legacy;
    return !strcmp(status, "1") ? NativeHandoff::ready : NativeHandoff::pending;
}

static char *gpu_path(const char *leaf) {
    g_autofree char *path = g_strconcat(gpu, leaf, nullptr);
    return ppd_utils_get_sysfs_path(path);
}

static bool gpu_read_is(const char *leaf, const char *expected) {
    g_autofree char *path = gpu_path(leaf);
    g_autofree char *value = nullptr;
    return g_file_get_contents(path, &value, nullptr, nullptr) && !strcmp(g_strchomp(value), expected);
}

static bool gpu_limit_at_most(const char *expected) {
    g_autofree char *path = gpu_path("max_freq");
    g_autofree char *value = nullptr;
    guint64 limit = 0;
    return g_file_get_contents(path, &value, nullptr, nullptr) &&
        g_ascii_string_to_unsigned(g_strchomp(value), 10, 255000000,
                                   g_ascii_strtoull(expected, nullptr, 10), &limit, nullptr);
}

static char *policy_path(unsigned policy, const char *leaf) {
    g_autofree char *path = g_strdup_printf("/sys/devices/system/cpu/cpufreq/policy%u/%s", policy, leaf);
    return ppd_utils_get_sysfs_path(path);
}

static bool read_is(unsigned policy, const char *leaf, const char *expected) {
    g_autofree char *path = policy_path(policy, leaf);
    g_autofree char *value = nullptr;
    return g_file_get_contents(path, &value, nullptr, nullptr) && !strcmp(g_strchomp(value), expected);
}

static PpdProbeResult probe(PpdDriver *driver) {
    auto self = PPD_DRIVER_UKE_CPUFREQ(driver);
    self->awaiting_provider = false;
    g_autofree char *path = ppd_utils_get_sysfs_path("/sys/firmware/devicetree/base/compatible");
    g_autofree char *compatible = nullptr;
    gsize length = 0;
    if (!g_file_get_contents(path, &compatible, &length, nullptr) || length < sizeof("xiaomi,uke") ||
        memcmp(compatible, "xiaomi,uke", sizeof("xiaomi,uke"))) return PPD_PROBE_RESULT_FAIL;
    const auto handoff = native_handoff();
    if (handoff == NativeHandoff::ready) return PPD_PROBE_RESULT_FAIL;
    if (handoff == NativeHandoff::pending) {
        self->awaiting_provider = true;
        return PPD_PROBE_RESULT_DEFER;
    }
    for (unsigned policy : policies) {
        if (!read_is(policy, "scaling_driver", "qcom-cpufreq-hw")) return PPD_PROBE_RESULT_FAIL;
        g_autofree char *available_path = policy_path(policy, "scaling_available_governors");
        g_autofree char *available = nullptr;
        if (!g_file_get_contents(available_path, &available, nullptr, nullptr)) return PPD_PROBE_RESULT_FAIL;
        g_auto(GStrv) governors = g_strsplit_set(available, " \t\n", -1);
        for (const char *name : {"powersave", "schedutil", "performance"})
            if (!g_strv_contains(governors, name)) return PPD_PROBE_RESULT_FAIL;
    }
    g_autofree char *gpu_id_path = gpu_path("device/of_node/compatible");
    g_autofree char *gpu_id = nullptr;
    if (!g_file_get_contents(gpu_id_path, &gpu_id, &length, nullptr)) {
        self->awaiting_provider = true;
        return PPD_PROBE_RESULT_DEFER;
    }
    if (length < sizeof("qcom,adreno-43030b00") ||
        memcmp(gpu_id, "qcom,adreno-43030b00", sizeof("qcom,adreno-43030b00")) ||
        !gpu_read_is("governor", "simple_ondemand") ||
        !gpu_read_is("available_frequencies", "255000000 353000000 500000000 633000000 684000000 736000000 835000000 900000000 950000000"))
        return PPD_PROBE_RESULT_FAIL;
    return PPD_PROBE_RESULT_SUCCESS;
}

static gboolean activate(PpdDriver *driver, PpdProfile profile, PpdProfileActivationReason, GError **error) {
    auto self = PPD_DRIVER_UKE_CPUFREQ(driver);
    if (native_handoff() != NativeHandoff::legacy) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY, "The native platform-profile provider owns this transition");
        return FALSE;
    }
    const char *governor;
    switch (profile) {
    case PPD_PROFILE_POWER_SAVER: governor = "powersave"; break;
    case PPD_PROFILE_BALANCED: governor = "schedutil"; break;
    case PPD_PROFILE_PERFORMANCE: governor = "performance"; break;
    default:
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "Unsupported CPU profile");
        return FALSE;
    }
    char *previous[3] = {};
    g_autofree char *gpu_max_path = gpu_path("max_freq");
    g_autofree char *previous_gpu = nullptr;
    g_autofree char *gpu_min_path = gpu_path("min_freq");
    g_autofree char *gpu_min = nullptr;
    const char *gpu_max = profile == PPD_PROFILE_POWER_SAVER ? "500000000" : "950000000";
    // shortcut: the 500 MHz preference needs energy calibration before a production release.
    if (!g_file_get_contents(gpu_max_path, &previous_gpu, nullptr, error) ||
        !g_file_get_contents(gpu_min_path, &gpu_min, nullptr, error)) return FALSE;
    g_strchomp(previous_gpu);
    // Sysfs reports the thermal-clamped limit, not our previous QoS preference.
    if (self->gpu_preference) {
        g_free(previous_gpu);
        previous_gpu = g_strdup(self->gpu_preference);
    }
    g_strchomp(gpu_min);
    guint64 minimum = 0;
    if (!g_ascii_string_to_unsigned(gpu_min, 10, 0, 950000000, &minimum, error)) return FALSE;
    if (minimum > g_ascii_strtoull(gpu_max, nullptr, 10)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY, "GPU minimum conflicts with the requested profile");
        return FALSE;
    }
    bool success = true;
    unsigned attempted = 0;
    for (unsigned i = 0; i < 3; i++) {
        g_autofree char *path = policy_path(policies[i], "scaling_governor");
        if (!g_file_get_contents(path, &previous[i], nullptr, error)) { success = false; break; }
        g_strchomp(previous[i]);
    }
    bool gpu_attempted = false;
    if (success) {
        gpu_attempted = true;
        success = ppd_utils_write(gpu_max_path, gpu_max, error);
        if (success && !gpu_limit_at_most(gpu_max)) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "GPU limit readback failed");
            success = false;
        }
    }
    if (success) for (unsigned i = 0; i < 3; i++) {
        g_autofree char *path = policy_path(policies[i], "scaling_governor");
        attempted = i + 1;
        if (!ppd_utils_write(path, governor, error)) { success = false; break; }
        if (!read_is(policies[i], "scaling_governor", governor)) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "CPU governor readback failed");
            success = false; break;
        }
    }
    if (!success) for (unsigned i = 0; i < attempted; i++) {
        g_autofree char *path = policy_path(policies[i], "scaling_governor");
        g_autoptr(GError) rollback_error = nullptr;
        if (!ppd_utils_write(path, previous[i], &rollback_error) ||
            !read_is(policies[i], "scaling_governor", previous[i])) {
            g_prefix_error(error, "CPU policy%u rollback failed; ", policies[i]);
        }
    }
    if (!success && gpu_attempted) {
        g_autoptr(GError) rollback_error = nullptr;
        if (!ppd_utils_write(gpu_max_path, previous_gpu, &rollback_error) ||
            !gpu_limit_at_most(previous_gpu))
            g_prefix_error(error, "GPU preference rollback failed; ");
    }
    for (auto value : previous) g_free(value);
    if (success) self->gpu_preference = gpu_max;
    return success;
}

static GObject *constructor(GType type, guint count, GObjectConstructParam *params) {
    auto object = G_OBJECT_CLASS(ppd_driver_uke_cpufreq_parent_class)->constructor(type, count, params);
    g_object_set(object, "driver-name", "uke-cpufreq", "profiles",
                 PPD_PROFILE_POWER_SAVER | PPD_PROFILE_BALANCED | PPD_PROFILE_PERFORMANCE, nullptr);
    auto self = PPD_DRIVER_UKE_CPUFREQ(object);
    const char *subsystems[] = {"devfreq", "drm", "platform", nullptr};
    self->client = g_udev_client_new(subsystems);
    g_signal_connect(self->client, "uevent", G_CALLBACK(+[](GUdevClient *, const char *, GUdevDevice *, gpointer data) {
        auto pending = PPD_DRIVER_UKE_CPUFREQ(data);
        if (pending->awaiting_provider || native_handoff() != NativeHandoff::legacy)
            g_signal_emit_by_name(pending, "probe-request", 0);
    }), self);
    return object;
}

static void finalize(GObject *object) {
    auto self = PPD_DRIVER_UKE_CPUFREQ(object);
    g_clear_object(&self->client);
    G_OBJECT_CLASS(ppd_driver_uke_cpufreq_parent_class)->finalize(object);
}

static void ppd_driver_uke_cpufreq_class_init(PpdDriverUkeCpufreqClass *klass) {
    G_OBJECT_CLASS(klass)->constructor = constructor;
    G_OBJECT_CLASS(klass)->finalize = finalize;
    auto driver_class = PPD_DRIVER_CLASS(klass);
    driver_class->probe = probe;
    driver_class->activate_profile = activate;
}

static void ppd_driver_uke_cpufreq_init(PpdDriverUkeCpufreq *) {}
