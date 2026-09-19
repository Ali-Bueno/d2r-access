#include "radar_config.h"
#include "async_log.h"

#include <charconv>
#include <string>

namespace D2RAccess::RadarConfig {
namespace {
std::string_view Trim(std::string_view text) {
	constexpr std::string_view Whitespace = " \t\r\n";
	const auto first = text.find_first_not_of(Whitespace);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(Whitespace) - first + 1);
}
enum class Line { Ignored, Accepted, Invalid };
// Walks the TOML subset once: comments stripped, one [section] tracked, every
// key line inside `section` handed to `visit(key, value)` where value is empty
// when the line has no '='. Returns false as soon as a line is Invalid.
template <class Visit> bool ForEachKey(std::string_view config, std::string_view section, Visit visit) {
	if (config.starts_with("\xEF\xBB\xBF")) config.remove_prefix(3); // UTF-8 BOM from text editors
	bool inside = false;
	while (!config.empty()) {
		const auto end = config.find('\n');
		auto line = config.substr(0, end);
		config = end == std::string_view::npos ? std::string_view{} : config.substr(end + 1);
		line = Trim(line.substr(0, line.find('#')));
		if (line.starts_with('[')) { inside = line == section; continue; }
		if (!inside || line.empty()) continue;
		const auto equal = line.find('=');
		const auto key = Trim(line.substr(0, equal));
		const auto value = equal == std::string_view::npos ? std::optional<std::string_view>{} : Trim(line.substr(equal + 1));
		if (visit(key, value) == Line::Invalid) return false;
	}
	return true;
}
bool ParseBool(std::string_view value, bool& out) {
	if (value != "true" && value != "false") return false;
	out = value == "true";
	return true;
}
template <class Number> bool ParseNumber(std::string_view value, Number& out) {
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), out);
	return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
std::string Load(const D2RL::PluginContext* context) {
	if (!context) return {};
	uint32_t size = 0;
	(void)context->ReadConfig(nullptr, 0, &size);
	constexpr uint32_t MaxConfigBytes = 1024 * 1024; // SDK embedded config limit
	if (!size || size > MaxConfigBytes) return {};
	std::string config(size, '\0');
	if (!context->ReadConfig(config.data(), size, &size)) return {};
	const auto terminator = config.find('\0');
	if (terminator != std::string::npos) config.resize(terminator);
	return config;
}
}
Result Parse(std::string_view config) {
	Result result;
	bool foundEnabled = false, foundVolume = false;
	const bool valid = ForEachKey(config, "[radar]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "enabled" && key != "volume") return Line::Ignored;
		if (!value) return Line::Invalid;
		if (key == "enabled") {
			if (foundEnabled || !ParseBool(*value, result.enabled)) return Line::Invalid;
			foundEnabled = true;
		} else {
			unsigned volume = 0;
			if (foundVolume || !ParseNumber(*value, volume) || volume > MaxVolume) return Line::Invalid;
			result.volume = volume;
			foundVolume = true;
		}
		return Line::Accepted;
	});
	return valid ? result : Result{.valid = false};
}
Result Read(const D2RL::PluginContext* context) {
	const auto result = Parse(Load(context));
	if (!result.valid) AsyncLog::Warn(context, "[radar] Invalid [radar] settings; using compiled defaults.");
	return result.valid ? result : Result{};
}

std::optional<bool> ParseEnabled(std::string_view config, std::string_view section) {
	std::optional<bool> result;
	const bool valid = ForEachKey(config, section, [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "enabled") return Line::Ignored;
		bool flag = false;
		if (result || !value || !ParseBool(*value, flag)) return Line::Invalid;
		result = flag;
		return Line::Accepted;
	});
	return valid ? result : std::nullopt;
}

std::optional<bool> ReadEnabled(const D2RL::PluginContext* context, std::string_view section) {
	return ParseEnabled(Load(context), section);
}

Npc ParseNpc(std::string_view config) {
	Npc result;
	const bool valid = ForEachKey(config, "[npc]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "enabled" && key != "range" && key != "line_of_sight") return Line::Ignored;
		if (!value) return Line::Invalid;
		bool flag = false;
		double range = 0;
		if (key == "range") {
			if (result.range || !ParseNumber(*value, range) || range <= 0) return Line::Invalid;
			result.range = range;
		} else {
			auto& slot = key == "enabled" ? result.enabled : result.lineOfSight;
			if (slot || !ParseBool(*value, flag)) return Line::Invalid;
			slot = flag;
		}
		return Line::Accepted;
	});
	return valid ? result : Npc{.valid = false};
}
Npc ReadNpc(const D2RL::PluginContext* context) {
	const auto result = ParseNpc(Load(context));
	if (!result.valid) AsyncLog::Warn(context, "[npc] Invalid [npc] settings; using compiled defaults.");
	return result.valid ? result : Npc{};
}
}
