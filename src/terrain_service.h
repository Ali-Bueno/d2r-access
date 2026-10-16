#pragma once

#include "entity_radar.h"
#include "radar.h"
#include "native_rooms.h"
#include <D2RLPlugin/api.h>
#include <functional>
#include <map>
#include <vector>

// Speaks the terrain overview on demand and keeps the exploration memory: the
// rooms the game has streamed, the rooms the player stood in, on disk across
// sessions. Lifecycle and speech plumbing around TerrainOverview::Describe.
// `docs/terrain-overview.md`, `docs/exploration.md`
namespace D2RAccess::TerrainService {
bool Register(const D2RL::PluginContext* context);
void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech);
void Unregister();
// Stamps the radar's world with what the memory knows: the rooms stood in,
// the exits into levels never seen, the frontier of open ground by the rays
// of the radar's frame.
void Qualify(Radar::World& world, Radar::Frame frame);
// True when the player has stood in the room whose rect origin is given, in
// tiles, on this level. The room announcer tags its ways out with it.
bool Visited(int level, int tileX, int tileY);
bool Known(int level); // at least one room of that level seen
// A validated snapshot of logical rooms stood in on the loaded current level.
std::vector<NativeRooms::Room> VisitedRooms(int level, const std::vector<NativeRooms::Room>& rooms);
// The fixed places found on this map in any game (MapSurvey::Fixed: exits,
// waypoints, the stash), so a later game on the same map lists them from
// anywhere, before the game streams their rooms again. Empty until the
// level's record is loaded, and when `rooms` (NativeRooms::Read) are not that
// record's layout: a regenerated map starts with none. `docs/exploration.md`
std::vector<EntityRadar::Entity> KnownPlaces(int level, const std::vector<NativeRooms::Room>& rooms);
// Keeps the fixed places among `found` (what the scanner counts as
// discovered) with the record of the map `rooms` belong to.
std::map<int, std::vector<EntityRadar::Entity>> MapPlaces(uint32_t seed);
void RememberPlaces(int level, const std::vector<NativeRooms::Room>& rooms, const std::vector<EntityRadar::Entity>& found);
// Drops the quest objects kept here whose spot `covered` says is in view while no quest
// object of `present` stands on it: used, or freed by another player.
void ForgetGoneQuestPlaces(int level, const std::vector<NativeRooms::Room>& rooms, const std::vector<EntityRadar::Entity>& present,
	const std::function<bool(Radar::Position)>& covered);
// Exploration's known ground: rooms stood in, rooms the automap has drawn in,
// and the room holding `player`. Game thread only (NativeAutomap::RevealedRooms).
std::vector<NativeRooms::Room> ExploredRooms(int level, const std::vector<NativeRooms::Room>& rooms, Radar::Position player);
// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool RoomSounds();
void SetRoomSounds(bool enabled);
bool PlaceAlerts();
void SetPlaceAlerts(bool enabled);
}
