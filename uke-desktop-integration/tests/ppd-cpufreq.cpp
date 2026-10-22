// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include "../src/ppd-driver-uke-cpufreq.cpp"
static std::string root;
static unsigned fail_policy = 99;
static bool bad_readback;
static bool fail_gpu;
static bool thermal_clamp;
static std::string gpu_request;
static unsigned writes;
extern "C" char *ppd_utils_get_sysfs_path(const char *path) { return g_strdup((root + path).c_str()); }
extern "C" gboolean ppd_utils_write(const char *path, const char *value, GError **error) {
    ++writes;
    if (strstr(path, "/devfreq/") && fail_gpu) {
        fail_gpu = false;
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Injected GPU write failure");
        return FALSE;
    }
    if (strstr(path, ("policy" + std::to_string(fail_policy) + "/").c_str())) {
        fail_policy = 99;
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Injected write failure");
        return FALSE;
    }
    std::ofstream file(path);
    if (strstr(path, "/devfreq/")) gpu_request = value;
    file << (bad_readback ? "wrong" : thermal_clamp && strstr(path, "/devfreq/") ? "353000000" : value);
    bad_readback = false;
    return bool(file);
}
static void write(const std::string &path, const std::string &value) {
    auto full = std::filesystem::path(root + path);
    std::filesystem::create_directories(full.parent_path());
    std::ofstream(full) << value;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    root = argv[1];
    const std::string base = "/sys/devices/system/cpu/cpufreq/policy";
    write("/sys/firmware/devicetree/base/compatible", std::string("xiaomi,uke\0qcom,sm7675\0", 23));
    for (unsigned policy : policies) {
        write(base + std::to_string(policy) + "/scaling_driver", "qcom-cpufreq-hw\n");
        write(base + std::to_string(policy) + "/scaling_available_governors", "powersave schedutil performance\n");
        write(base + std::to_string(policy) + "/scaling_governor", "schedutil\n");
    }
    g_autoptr(PpdDriverUkeCpufreq) driver = PPD_DRIVER_UKE_CPUFREQ(g_object_new(PPD_TYPE_DRIVER_UKE_CPUFREQ, nullptr));
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    unsigned reprobes = 0;
    g_signal_connect(driver, "probe-request", G_CALLBACK(+[](PpdDriver *, gpointer data) {
        ++*static_cast<unsigned *>(data);
    }), &reprobes);
    g_signal_emit_by_name(driver->client, "uevent", "add", nullptr);
    assert(reprobes == 1);
    write(std::string(gpu) + "device/of_node/compatible", std::string("qcom,adreno-43030b00\0qcom,adreno\0", 32));
    write(std::string(gpu) + "available_frequencies", "255000000 353000000 500000000 633000000 684000000 736000000 835000000 900000000 950000000\n");
    write(std::string(gpu) + "governor", "simple_ondemand\n");
    write(std::string(gpu) + "min_freq", "255000000\n");
    write(std::string(gpu) + "max_freq", "950000000\n");
    assert(ppd_driver_get_profiles(PPD_DRIVER(driver)) == (PPD_PROFILE_POWER_SAVER | PPD_PROFILE_BALANCED | PPD_PROFILE_PERFORMANCE));
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_SUCCESS);
    g_signal_emit_by_name(driver->client, "uevent", "change", nullptr);
    assert(reprobes == 1);
    for (auto profile : {PPD_PROFILE_POWER_SAVER, PPD_PROFILE_PERFORMANCE, PPD_PROFILE_BALANCED}) {
        g_autoptr(GError) error = nullptr;
        assert(activate(PPD_DRIVER(driver), profile, PPD_PROFILE_ACTIVATION_REASON_USER, &error));
        const char *expected = profile == PPD_PROFILE_POWER_SAVER ? "powersave" : profile == PPD_PROFILE_PERFORMANCE ? "performance" : "schedutil";
        for (unsigned policy : policies) assert(read_is(policy, "scaling_governor", expected));
        assert(gpu_read_is("max_freq", profile == PPD_PROFILE_POWER_SAVER ? "500000000" : "950000000"));
    }
    for (bool readback : {false, true}) {
        g_autoptr(GError) error = nullptr;
        fail_policy = readback ? 99 : 3; bad_readback = readback;
        assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_PERFORMANCE, PPD_PROFILE_ACTIVATION_REASON_USER, &error));
        assert(error);
        for (unsigned policy : policies) assert(read_is(policy, "scaling_governor", "schedutil"));
        assert(gpu_read_is("max_freq", "950000000"));
    }
    fail_gpu = true;
    g_autoptr(GError) gpu_error = nullptr;
    assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_POWER_SAVER, PPD_PROFILE_ACTIVATION_REASON_USER, &gpu_error));
    for (unsigned policy : policies) assert(read_is(policy, "scaling_governor", "schedutil"));
    assert(gpu_read_is("max_freq", "950000000"));
    thermal_clamp = true;
    assert(activate(PPD_DRIVER(driver), PPD_PROFILE_POWER_SAVER, PPD_PROFILE_ACTIVATION_REASON_USER, nullptr));
    assert(gpu_read_is("max_freq", "353000000"));
    fail_policy = 3;
    g_clear_error(&gpu_error);
    assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_PERFORMANCE, PPD_PROFILE_ACTIVATION_REASON_USER, &gpu_error));
    assert(gpu_request == "500000000");
    thermal_clamp = false;
    write(std::string(gpu) + "max_freq", gpu_request);
    assert(gpu_read_is("max_freq", "500000000"));
    write(std::string(gpu) + "min_freq", "633000000\n");
    g_clear_error(&gpu_error);
    assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_POWER_SAVER, PPD_PROFILE_ACTIVATION_REASON_USER, &gpu_error));
    write(std::string(gpu) + "min_freq", "255000000\n");
    g_autoptr(GError) error = nullptr;
    assert(!activate(PPD_DRIVER(driver), static_cast<PpdProfile>(0), PPD_PROFILE_ACTIVATION_REASON_USER, &error));
    write(base + "3/scaling_available_governors", "powersave schedutil performance-extra\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_FAIL);
    write(base + "3/scaling_available_governors", "powersave schedutil performance\n");
    write(base + "7/scaling_driver", "other\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_FAIL);
    write(base + "7/scaling_driver", "qcom-cpufreq-hw\n");
    write("/sys/firmware/devicetree/base/compatible", "xiaomi,other");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_FAIL);
    write("/sys/firmware/devicetree/base/compatible", std::string("xiaomi,uke\0qcom,sm7675\0", 23));
    const std::string native = "/sys/module/uke_platform_profile/";
    std::filesystem::create_directories(root + native);
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    write(native + "version", "1\n");
    const unsigned before_handoff = writes;
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    write(native + "parameters/probe_status", "0\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    g_signal_emit_by_name(driver->client, "uevent", "change", nullptr);
    assert(reprobes == 2);
    g_clear_error(&error);
    assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_PERFORMANCE, PPD_PROFILE_ACTIVATION_REASON_USER, &error));
    assert(error && writes == before_handoff);
    write(native + "parameters/probe_status", "1\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_FAIL);
    g_signal_emit_by_name(driver->client, "uevent", "change", nullptr);
    assert(reprobes == 3);
    g_clear_error(&error);
    assert(!activate(PPD_DRIVER(driver), PPD_PROFILE_POWER_SAVER, PPD_PROFILE_ACTIVATION_REASON_USER, &error));
    assert(error && writes == before_handoff);
    write(native + "parameters/probe_status", "-1\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_SUCCESS);
    g_clear_error(&error);
    assert(activate(PPD_DRIVER(driver), PPD_PROFILE_BALANCED, PPD_PROFILE_ACTIVATION_REASON_USER, &error));
    for (const char *state : {"2\n", "-2\n", "invalid\n", ""}) {
        write(native + "parameters/probe_status", state);
        assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    }
    write(native + "version", "2\n");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    std::filesystem::remove_all(root + native);
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_SUCCESS);
    std::filesystem::create_directories(root + native);
    std::filesystem::create_symlink("version", root + native + "version");
    assert(probe(PPD_DRIVER(driver)) == PPD_PROBE_RESULT_DEFER);
    g_print("Uke PPD: CPU/GPU profiles, delayed GPU probe, thermal clamp, write/readback rollback and unsupported inputs passed\n");
}
