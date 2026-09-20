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

// Sections whose only documented key is `enabled`: [keyboard], which arms the
// keyboard-as-controller pad, [probe], which arms the focused-widget byte
// dump, and [native], which arms the three inline hooks into the game's own
// code. The first two have to be settable from the file because the developer
// console cannot be opened outside a loaded game, and both matter most in the
// character-select and character-create screens, where it cannot. [native] has
// to be settable there because a hook is installed at load and cannot be
// withdrawn from a console afterwards.
// Absent section or absent key returns nothing, so the caller keeps its own
// default; an unparseable value is reported and also returns nothing.
std::optional<bool> ParseEnabled(std::string_view config, std::string_view section);
std::optional<bool> ReadEnabled(const D2RL::PluginContext* context, std::string_view section);
}
