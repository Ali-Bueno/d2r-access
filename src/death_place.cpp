#include "death_place.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <deque>
#include <sstream>

namespace D2RAccess::DeathPlace {

namespace {

using EntityRadar::Category;
using EntityRadar::Entity;
using EntityRadar::Kind;

bool IsExit(const Entity& entity) {
	return entity.kind == Kind::Exit && entity.category == Category::Exit && entity.destinationLevel != 0;
}
bool IsWaypoint(const Entity& entity) { return entity.category == Category::Waypoint; }

double Distance(Radar::Position a, Radar::Position b) { return std::hypot(b.x - a.x, b.y - a.y); }

// Level changes from each known level to `target` over known exits.
std::map<int, int> HopsTo(int target, const KnownPlaces& known) {
	std::map<int, int> hops{{target, 0}};
	std::deque<int> queue{target};
	while (!queue.empty()) {
		const int level = queue.front();
		queue.pop_front();
		for (const auto& [from, places] : known) {
			if (hops.contains(from)) continue;
			if (std::any_of(places.begin(), places.end(), [level](const Entity& place) {
					return IsExit(place) && static_cast<int>(place.destinationLevel) == level;
				})) {
				hops[from] = hops[level] + 1;
				queue.push_back(from);
			}
		}
	}
	return hops;
}

// The nearest of the current level's places `match` selects, ranked first by `rank` (lower is better).
template<class Match, class Rank> const Entity* Best(const std::vector<Entity>& places, Radar::Position player, Match match, Rank rank) {
	const Entity* best = nullptr;
	int bestRank = INT_MAX;
	double bestDistance = 0;
	for (const Entity& place : places) {
		if (!match(place)) continue;
		const int placeRank = rank(place);
		const double distance = Distance(player, place.position);
		if (placeRank < bestRank || (placeRank == bestRank && distance < bestDistance)) {
			best = &place;
			bestRank = placeRank;
			bestDistance = distance;
		}
	}
	return best;
}

}

void Memory::Died(const Record& record) {
	std::erase_if(m_records, [&](const Record& known) { return known.seed == record.seed; });
	m_records.insert(m_records.begin(), record);
	if (m_records.size() > MaxRecords) m_records.resize(MaxRecords);
}

const Record* Memory::Find(uint32_t seed) const {
	const auto found = std::find_if(m_records.begin(), m_records.end(), [seed](const Record& record) { return record.seed == seed; });
	return found == m_records.end() ? nullptr : &*found;
}

bool Memory::Clear(uint32_t seed) {
	return std::erase_if(m_records, [seed](const Record& record) { return record.seed == seed; }) != 0;
}

std::string Memory::Serialize() const {
	std::string text;
	for (const Record& record : m_records) {
		char line[96]{};
		std::snprintf(line, sizeof line, "%u %d %.2f %.2f\n", record.seed, record.level, record.position.x, record.position.y);
		text += line;
	}
	return text;
}

void Memory::Parse(std::string_view text) {
	m_records.clear();
	std::istringstream lines{std::string(text)};
	std::string line;
	while (std::getline(lines, line) && m_records.size() < MaxRecords) {
		std::istringstream fields(line);
		Record record;
		if (!(fields >> record.seed >> record.level >> record.position.x >> record.position.y)) continue;
		if (!record.seed || record.level <= 0 || !std::isfinite(record.position.x) || !std::isfinite(record.position.y)
			|| record.position.x < 0 || record.position.y < 0 || Find(record.seed)) continue;
		m_records.push_back(record);
	}
}

std::optional<Step> NextStep(int current, int target, const KnownPlaces& known, Radar::Position player) {
	const auto here = known.find(current);
	if (current == target || here == known.end()) return std::nullopt;
	const auto hops = HopsTo(target, known);
	const auto hopsOf = [&hops](int level) {
		const auto found = hops.find(level);
		return found == hops.end() ? INT_MAX : found->second;
	};
	const Entity* exit = Best(here->second, player,
		[&](const Entity& place) { return IsExit(place) && hopsOf(static_cast<int>(place.destinationLevel)) != INT_MAX; },
		[&](const Entity& place) { return hopsOf(static_cast<int>(place.destinationLevel)); });
	if (exit) return Step{*exit, static_cast<int>(exit->destinationLevel), false};
	const Entity* waypoint = Best(here->second, player, IsWaypoint, [](const Entity&) { return 0; });
	if (!waypoint) return std::nullopt;
	int next = 0, nextHops = INT_MAX;
	for (const auto& [level, places] : known) {
		if (level == current || hopsOf(level) >= nextHops || std::none_of(places.begin(), places.end(), IsWaypoint)) continue;
		next = level;
		nextHops = hopsOf(level);
	}
	if (!next) return std::nullopt;
	return Step{*waypoint, next, true};
}

EntityRadar::Entity Landmark(const Record& record, std::string name) {
	return {static_cast<uint32_t>(Identity), Kind::Corpse, record.position, std::move(name), Identity, false, Category::Corpse};
}

}
