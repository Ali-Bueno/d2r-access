#pragma once

#include <D2RLPlugin/api.h>
#include <optional>
#include <string_view>

namespace D2RAccess::RadarConfig {
// The documented [radar] keys: enabled (boolean) and volume (0-100 percent of
// the compiled cue level). Other keys belong to other features. Invalid or
// duplicate radar values retain the compiled defaults.
struct Result { bool enabled = true; unsigned volume = 100; bool valid = true; };
inline constexpr unsigned MaxVolume = 100;
Result Parse(std::string_view config);
Result Read(const D2RL::PluginContext* context);

// The documented [npc] keys of the friendly NPC announcer. Absent keys keep
// the feature's compiled defaults (NpcProximity::Settings), so nothing here
// repeats them; range is in collision subtiles like the entity scan.
struct Npc {
	std::optional<bool> enabled, lineOfSight;
	std::optional<double> range;
	bool valid = true;
};
Npc ParseNpc(std::string_view config);
Npc ReadNpc(const D2RL::PluginContext* context);
}
