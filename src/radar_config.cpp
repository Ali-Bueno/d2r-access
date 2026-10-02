#include "radar_config.h"
#include "async_log.h"

#include <charconv>
#include <cmath>
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
// Every section reads the same way: parse it, warn once if the file is
// malformed, fall back to the compiled defaults.
template <class Section, class Parser>
Section ReadSection(const D2RL::PluginContext* context, Parser parse, const char* warning) {
	const auto result = parse(Load(context));
	if (!result.valid) AsyncLog::Warn(context, warning);
	return result.valid ? result : Section{};
}
}
Result Parse(std::string_view config) {
	Result result;
	bool foundEnabled = false, foundVolume = false, foundDiagonals = false;
	const bool valid = ForEachKey(config, "[radar]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "enabled" && key != "volume" && key != "diagonals") return Line::Ignored;
		if (!value) return Line::Invalid;
		if (key == "enabled") {
			if (foundEnabled || !ParseBool(*value, result.enabled)) return Line::Invalid;
			foundEnabled = true;
		} else if (key == "diagonals") {
			if (foundDiagonals || !ParseBool(*value, result.diagonals)) return Line::Invalid;
			foundDiagonals = true;
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
	return ReadSection<Result>(context, Parse, "[radar] Invalid [radar] settings; using compiled defaults.");
}

Navigation ParseNavigation(std::string_view config) {
	Navigation result;
	bool found = false;
	const bool valid = ForEachKey(config, "[navigation]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "full_assistance") return Line::Ignored;
		if (found || !value || !ParseBool(*value, result.fullAssistance)) return Line::Invalid;
		found = true;
		return Line::Accepted;
	});
	return valid ? result : Navigation{.valid = false};
}
Navigation ReadNavigation(const D2RL::PluginContext* context) {
	return ReadSection<Navigation>(context, ParseNavigation,
		"[navigation] Invalid [navigation] settings; using exploration-first routing.");
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
	return ReadSection<Npc>(context, ParseNpc, "[npc] Invalid [npc] settings; using compiled defaults.");
}

Target ParseTarget(std::string_view config) {
	Target result;
	const bool valid = ForEachKey(config, "[target]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "line_of_sight" && key != "reach") return Line::Ignored;
		if (!value) return Line::Invalid;
		if (key == "reach") {
			double reach = 0;
			if (result.reach || !ParseNumber(*value, reach) || reach <= 0) return Line::Invalid;
			result.reach = reach;
		} else {
			bool flag = false;
			if (result.lineOfSight || !ParseBool(*value, flag)) return Line::Invalid;
			result.lineOfSight = flag;
		}
		return Line::Accepted;
	});
	return valid ? result : Target{.valid = false};
}
Target ReadTarget(const D2RL::PluginContext* context) {
	return ReadSection<Target>(context, ParseTarget, "[target] Invalid [target] settings; using compiled defaults.");
}

Switch ParseSwitch(std::string_view config, std::string_view section) {
	Switch result;
	const bool valid = ForEachKey(config, section, [&](std::string_view key, std::optional<std::string_view> value) {
		if (key != "enabled") return Line::Ignored;
		bool flag = false;
		if (!value || result.enabled || !ParseBool(*value, flag)) return Line::Invalid;
		result.enabled = flag;
		return Line::Accepted;
	});
	return valid ? result : Switch{.valid = false};
}
Switch ParseChests(std::string_view config) { return ParseSwitch(config, "[chests]"); }
Switch ReadChests(const D2RL::PluginContext* context) {
	return ReadSection<Switch>(context, ParseChests, "[chests] Invalid [chests] settings; using compiled defaults.");
}
Switch ParseBossLife(std::string_view config) { return ParseSwitch(config, "[boss_life]"); }
Switch ReadBossLife(const D2RL::PluginContext* context) {
	return ReadSection<Switch>(context, ParseBossLife, "[boss-life] Invalid [boss_life] settings; using compiled defaults.");
}
Switch ParseDoors(std::string_view config) { return ParseSwitch(config, "[doors]"); }
Switch ReadDoors(const D2RL::PluginContext* context) {
	return ReadSection<Switch>(context, ParseDoors, "[doors] Invalid [doors] settings; using compiled defaults.");
}

namespace {
std::optional<AttackReach::Mode> ParseAttackMode(std::string_view value) {
	if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
	if (value == "auto") return AttackReach::Mode::Auto;
	if (value == "melee") return AttackReach::Mode::Melee;
	if (value == "ranged") return AttackReach::Mode::Ranged;
	return std::nullopt;
}
}

EntityCueSettings ParseEntityCues(std::string_view config) {
	EntityCueSettings result;
	const bool valid = ForEachKey(config, "[entity_cues]", [&](std::string_view key, std::optional<std::string_view> value) {
		if (key == "range" || key == "melee_reach" || key == "ranged_reach") {
			auto& slot = key == "range" ? result.range : key == "melee_reach" ? result.meleeReach : result.rangedReach;
			double range = 0;
			if (!value || slot || !ParseNumber(*value, range) || !std::isfinite(range)
				|| range <= 0 || range > MaxEntityCueRange) return Line::Invalid;
			slot = range;
		} else if (key == "attack") {
			if (!value || result.attack) return Line::Invalid;
			result.attack = ParseAttackMode(*value);
			if (!result.attack) return Line::Invalid;
		} else if (key == "volume") {
			unsigned volume = 0;
			if (!value || result.volume || !ParseNumber(*value, volume) || volume > MaxVolume) return Line::Invalid;
			result.volume = volume;
		} else {
			std::optional<bool>* slot = nullptr;
			if (key == "enabled") slot = &result.enabled;
			else if (key == "npcs") slot = &result.npcs;
			else if (key == "enemies") slot = &result.enemies;
			else if (key == "ranged") slot = &result.ranged;
			else if (key == "ordinary") slot = &result.ordinary;
			else if (key == "reach") slot = &result.reach;
			else return Line::Ignored;
			bool flag = false;
			if (!value || slot->has_value() || !ParseBool(*value, flag)) return Line::Invalid;
			*slot = flag;
		}
		return Line::Accepted;
	});
	return valid ? result : EntityCueSettings{.valid = false};
}
EntityCueSettings ReadEntityCues(const D2RL::PluginContext* context) {
	return ReadSection<EntityCueSettings>(context, ParseEntityCues, "[entity-cues] Invalid [entity_cues] settings; using compiled defaults.");
}
}
