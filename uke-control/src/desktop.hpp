// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <systemd/sd-bus.h>
namespace uke::desktop {
// Existing services only: no activation, sensor claims or hardware controls.
std::string snapshot(sd_bus *system, sd_bus *session);
std::string status();
}
