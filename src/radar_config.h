#pragma once

#include <D2RLPlugin/api.h>
#include <string_view>

namespace D2RAccess::RadarConfig {
// Only the documented [radar] enabled boolean is consumed. Other keys belong
// to other features. Invalid/duplicate radar values retain the compiled default.
struct Result { bool enabled = true, valid = true; };
Result Parse(std::string_view config);
bool Enabled(const D2RL::PluginContext* context);
}
