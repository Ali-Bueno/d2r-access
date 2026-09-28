#include "death_place_service.h"

#include "async_log.h"
#include "beacon.h"
#include "death_place.h"
#include "native_entities.h"
#include "speech_strings.h"
#include "terrain_service.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace D2RAccess::DeathPlaceService {

namespace {

const D2RL::PluginContext* g_context = nullptr;
std::filesystem::path g_file;
DeathPlace::Memory g_memory;
std::atomic<bool> g_died{false};
DeathPlace::Record g_alive; // the last place the player was read standing
// A death is only walked back to after leaving its level: the body lies on it until respawn.
bool g_armed = true;

void Save() {
	if (g_file.empty()) return;
	std::error_code error;
	std::filesystem::create_directories(g_file.parent_path(), error);
	const auto temporary = g_file.parent_path() / (g_file.filename().string() + ".tmp");
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		out << g_memory.Serialize();
		if (!out) {
			AsyncLog::Warn(g_context, "[death] could not write the death places");
			return;
		}
	}
	std::filesystem::rename(temporary, g_file, error);
	if (error) AsyncLog::Warn(g_context, "[death] could not replace the death places file");
}

}

void Register(const D2RL::PluginContext* context) {
	g_context = context;
	if (!context || !context->pluginDirectory) return;
	// Beside the exploration archive: `d2r-access\explored\deaths.txt`.
	g_file = std::filesystem::path(context->pluginDirectory) / "d2r-access" / "explored" / "deaths.txt";
	std::ifstream in(g_file, std::ios::binary);
	if (!in) return;
	std::ostringstream text;
	text << in.rdbuf();
	g_memory.Parse(text.str());
	AsyncLog::InfoF(context, "[death] %zu death places loaded", g_memory.Records().size());
}

void NoteDeath() { g_died = true; }

void Track(uint32_t seed, int level, Radar::Position player) {
	if (g_died.exchange(false) && g_alive.seed) {
		g_memory.Died(g_alive);
		g_armed = false;
		Save();
		AsyncLog::InfoF(g_context, "[death] recorded level=%d at=%.0f,%.0f", g_alive.level, g_alive.position.x, g_alive.position.y);
	}
	if (!seed) return;
	g_alive = {seed, level, player};
	const auto* death = g_memory.Find(seed);
	if (!death) return;
	if (death->level != level) {
		g_armed = true;
		return;
	}
	if (!g_armed || std::hypot(player.x - death->position.x, player.y - death->position.y) > Beacon::Settings{}.arrivalDistance) return;
	g_memory.Clear(seed);
	Save();
	AsyncLog::InfoF(g_context, "[death] reached level=%d at=%.0f,%.0f: forgotten", level, player.x, player.y);
}

std::vector<EntityRadar::Entity> Landmarks(uint32_t seed, int level, Radar::Position player,
	const std::vector<EntityRadar::Entity>& here) {
	const auto* death = seed ? g_memory.Find(seed) : nullptr;
	if (!death) return {};
	const Strings::Table& strings = Strings::Current();
	if (death->level == level) return {DeathPlace::Landmark(*death, strings.deathPlace)};
	auto known = TerrainService::MapPlaces(seed);
	known[level] = here;
	const auto step = DeathPlace::NextStep(level, death->level, known, player);
	if (!step) return {};
	auto entity = step->landmark;
	const auto died = NativeEntities::LevelName(static_cast<uint32_t>(death->level));
	char name[512]{};
	if (step->waypoint)
		std::snprintf(name, sizeof name, strings.deathRouteWaypoint, died.c_str(), entity.name.c_str(),
			NativeEntities::LevelName(static_cast<uint32_t>(step->next)).c_str());
	else std::snprintf(name, sizeof name, strings.deathRoute, died.c_str(), entity.name.c_str());
	entity.name = name;
	entity.nameKey.clear(); // composed, not a game string
	entity.category = EntityRadar::Category::Corpse;
	return {entity};
}

}
