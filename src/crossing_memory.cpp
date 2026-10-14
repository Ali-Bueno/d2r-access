#include "crossing_memory.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace D2RAccess::CrossingMemory {
namespace {
// Policy: crossings nearer each other than this are one opening; a border has
// one or two (the Monastery Gate's east and middle entrances), so a few are kept.
constexpr double SameOpening = 6;
constexpr size_t KeptPerBorder = 8;

struct Crossing { int level = 0; uint32_t destination = 0; Radar::Position position; };
std::mutex g_mutex;
uint64_t g_session = 0;
std::vector<Crossing> g_crossings;

double Distance(Radar::Position a, Radar::Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
}

void Note(uint64_t session, int level, const std::vector<Radar::LevelExit>& crossings) {
	std::lock_guard lock(g_mutex);
	if (session != g_session) {
		g_crossings.clear(); // another game: another map
		g_session = session;
	}
	for (const auto& crossing : crossings) {
		const auto border = [&](const Crossing& known) { return known.level == level && known.destination == crossing.level; };
		if (std::any_of(g_crossings.begin(), g_crossings.end(),
			[&](const Crossing& known) { return border(known) && Distance(known.position, crossing.position) < SameOpening; })) continue;
		if (static_cast<size_t>(std::count_if(g_crossings.begin(), g_crossings.end(), border)) >= KeptPerBorder) continue;
		g_crossings.push_back({level, crossing.level, crossing.position});
	}
}

std::optional<Radar::Position> Find(uint64_t session, int level, uint32_t destination, Radar::Position near) {
	std::lock_guard lock(g_mutex);
	if (session != g_session) return std::nullopt;
	std::optional<Radar::Position> nearest;
	for (const auto& known : g_crossings)
		if (known.level == level && known.destination == destination
			&& (!nearest || Distance(known.position, near) < Distance(*nearest, near))) nearest = known.position;
	return nearest;
}

void Clear() {
	std::lock_guard lock(g_mutex);
	g_crossings.clear();
	g_session = 0;
}
}
