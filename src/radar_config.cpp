#include "radar_config.h"
#include "async_log.h"

#include <string>

namespace D2RAccess::RadarConfig {
namespace {
std::string_view Trim(std::string_view text) {
	constexpr std::string_view Whitespace = " \t\r\n";
	const auto first = text.find_first_not_of(Whitespace);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(Whitespace) - first + 1);
}
}
Result Parse(std::string_view config) {
	if (config.starts_with("\xEF\xBB\xBF")) config.remove_prefix(3); // UTF-8 BOM from text editors
	Result result;
	bool radar = false, found = false;
	while (!config.empty()) {
		const auto end = config.find('\n');
		auto line = config.substr(0, end);
		config = end == std::string_view::npos ? std::string_view{} : config.substr(end + 1);
		line = Trim(line.substr(0, line.find('#')));
		if (line.starts_with('[')) { radar = line == "[radar]"; continue; }
		if (!radar || line.empty()) continue;
		const auto equal = line.find('=');
		if (Trim(line.substr(0, equal)) != "enabled") continue;
		if (equal == std::string_view::npos || found) return {.valid = false};
		const auto value = Trim(line.substr(equal + 1));
		if (value != "true" && value != "false") return {.valid = false};
		result.enabled = value == "true";
		found = true;
	}
	return result;
}
bool Enabled(const D2RL::PluginContext* context) {
	if (!context) return true;
	uint32_t size = 0;
	(void)context->ReadConfig(nullptr, 0, &size);
	constexpr uint32_t MaxConfigBytes = 1024 * 1024; // SDK embedded config limit
	if (!size || size > MaxConfigBytes) return true;
	std::string config(size, '\0');
	if (!context->ReadConfig(config.data(), size, &size)) return true;
	const auto terminator = config.find('\0');
	if (terminator != std::string::npos) config.resize(terminator);
	const auto result = Parse(config);
	if (!result.valid) AsyncLog::Warn(context, "[radar] Invalid enabled setting; using default true.");
	return result.enabled;
}
}
