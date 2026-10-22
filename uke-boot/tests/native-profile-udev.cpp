// SPDX-License-Identifier: MIT
// Run original udev rule evaluation in an isolated UMockdev tree.
#include <umockdev.h>
#include <cassert>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    assert(argc == 2);
    g_autoptr(UMockdevTestbed) testbed = umockdev_testbed_new();
    g_autofree char *root = umockdev_testbed_get_root_dir(testbed);
    assert(g_setenv("UMOCKDEV_DIR", root, true));
    g_autofree char *device = umockdev_testbed_add_devicev(testbed,
        "platform", "uke-platform-profile", nullptr, nullptr, nullptr);
    std::filesystem::create_directories(std::string(root) + "/sys/bus/platform/drivers/uke-platform-profile");
    umockdev_testbed_set_attribute_link(testbed, device, "driver", "../../bus/platform/drivers/uke-platform-profile");
    g_autofree char *foreign = umockdev_testbed_add_devicev(testbed,
        "platform", "unrelated-platform", nullptr, nullptr, nullptr);
    umockdev_testbed_set_attribute_link(testbed, foreign, "driver", "../../bus/platform/drivers/uke-platform-profile");
    const pid_t child = fork();
    assert(child >= 0);
    if (!child) { execl("/bin/bash", "bash", argv[1], static_cast<char *>(nullptr)); _exit(127); }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
