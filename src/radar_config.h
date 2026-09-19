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

// The documented [target] keys of the HUD sight gate (TargetSight::Settings):
// line_of_sight decides whether a label the game puts on something behind a
// wall is spoken, reach is how far a bearer of that name may stand and still
// be the one meant, in collision subtiles.
struct Target {
	std::optional<bool> lineOfSight;
	std::optional<double> reach;
	bool valid = true;
};
Target ParseTarget(std::string_view config);
Target ReadTarget(const D2RL::PluginContext* context);

// The documented [explore] keys of the nearest-unexplored-space route:
// enabled, radius (how far the route may reach) and mark_radius (how much of
// the map walking past a place counts as exploring it). Both distances are
// collision subtiles; absent keys keep the compiled defaults.
struct Explore {
	std::optional<bool> enabled;
	std::optional<double> radius, markRadius, probeRadius;
	bool valid = true;
};
Explore ParseExplore(std::string_view config);
Explore ReadExplore(const D2RL::PluginContext* context);

// The documented [chests] key of the container proximity loop. Ctrl+K toggles
// it for the current run; this is only the state it starts in.
struct Chests {
	std::optional<bool> enabled;
	bool valid = true;
};
Chests ParseChests(std::string_view config);
Chests ReadChests(const D2RL::PluginContext* context);
}
