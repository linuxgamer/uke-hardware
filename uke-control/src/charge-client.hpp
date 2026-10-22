// SPDX-License-Identifier: MIT
#pragma once
#include "backend.hpp"

namespace uke::charge {
Result control_query(const std::string& operation);
Result control_mode(const std::string& mode);
Result enable_preconfigured_cap();
Result enable_moisture();
Result apply_preconfigured_cap();
}
