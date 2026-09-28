#include "death_place.h"
#include "test_support.h"

using namespace D2RAccess;
using namespace D2RAccess::DeathPlace;
using EntityRadar::Category;
using EntityRadar::Entity;
using EntityRadar::Kind;

namespace {

Entity Exit(uint32_t id, int destination, double x, double y) {
	return {id, Kind::Exit, {x, y}, "to " + std::to_string(destination), id, false, Category::Exit, static_cast<uint32_t>(destination)};
}
Entity Waypoint(uint32_t id, double x, double y) {
	return {id, Kind::Object, {x, y}, "waypoint", id, false, Category::Waypoint};
}

void MemoryKeepsOnePlacePerMap() {
	Memory memory;
	memory.Died({7, 49, {100, 200}});
	memory.Died({9, 2, {5, 6}});
	memory.Died({7, 48, {300, 400}});
	Check(memory.Records().size() == 2 && memory.Records().front().seed == 7, "a new death replaces its map's place and comes first");
	Check(memory.Find(7) && memory.Find(7)->level == 48 && memory.Find(7)->position.x == 300, "the latest death of a map is the one kept");
	Check(!memory.Find(8), "a map never died on has no place");
	Memory restored;
	restored.Parse(memory.Serialize());
	Check(restored.Records() == memory.Records(), "the places survive a save and a load");
	restored.Parse("garbage\n0 4 1 1\n5 0 1 1\n6 3 -1 2\n11 3 1.5 2.25\n11 4 8 8\n");
	Check(restored.Records().size() == 1 && restored.Records()[0].seed == 11 && restored.Records()[0].level == 3
		&& restored.Records()[0].position.y == 2.25, "a damaged file keeps only its valid lines, one per map");
	Check(memory.Clear(7) && !memory.Find(7) && !memory.Clear(7), "a place walked back to is forgotten once");
	for (uint32_t seed = 100; seed < 100 + Memory::MaxRecords + 5; ++seed) memory.Died({seed, 1, {1, 1}});
	Check(memory.Records().size() == Memory::MaxRecords && !memory.Find(9), "the oldest maps are dropped past the limit");
}

void StepFollowsKnownExits() {
	// Lut Gholein (40) -> Sewers 1 (47) -> Sewers 2 (48) -> Sewers 3 (49).
	KnownPlaces known{
		{40, {Exit(1, 47, 100, 100), Exit(2, 47, 20, 20), Exit(3, 41, 10, 10)}},
		{41, {Exit(4, 40, 0, 0)}},
		{47, {Exit(5, 40, 0, 0), Exit(6, 48, 50, 50)}},
		{48, {Exit(7, 47, 0, 0), Exit(8, 49, 60, 60)}},
		{49, {Exit(9, 48, 0, 0)}},
	};
	auto step = NextStep(40, 49, known, {0, 0});
	Check(step && step->landmark.id == 2 && step->next == 47 && !step->waypoint,
		"the step is the nearest exit into the next level of the shortest known route");
	step = NextStep(48, 49, known, {0, 0});
	Check(step && step->landmark.id == 8 && step->next == 49, "one level away, the step is the exit into the death level");
	step = NextStep(41, 49, known, {0, 0});
	Check(step && step->landmark.id == 4 && step->next == 40, "a level off the route first leads back onto it");
	Check(!NextStep(49, 49, known, {0, 0}), "on the death level there is no step");
	Check(!NextStep(50, 49, known, {0, 0}), "a level with nothing known offers no step");
}

void WaypointOnlyWithoutAWalkingRoute() {
	KnownPlaces known{
		{40, {Waypoint(1, 5, 5), Exit(2, 41, 10, 10)}},
		{41, {Exit(3, 40, 0, 0)}},
		{47, {Waypoint(4, 0, 0), Exit(5, 48, 1, 1)}},
		{48, {Waypoint(6, 0, 0), Exit(7, 49, 1, 1)}},
		{49, {}},
	};
	auto step = NextStep(40, 49, known, {0, 0});
	Check(step && step->waypoint && step->landmark.id == 1 && step->next == 48,
		"with no walking route the waypoint leads to the known waypoint nearest the death level");
	known[49].push_back(Waypoint(8, 0, 0));
	step = NextStep(40, 49, known, {0, 0});
	Check(step && step->waypoint && step->next == 49, "a waypoint on the death level itself is picked first");
	known[40].push_back(Exit(9, 47, 50, 50));
	step = NextStep(40, 49, known, {0, 0});
	Check(step && !step->waypoint && step->landmark.id == 9, "a known walking route wins over the waypoint");
	known[40] = {Exit(2, 41, 10, 10)};
	known[47].push_back(Exit(10, 40, 0, 0));
	known[47].erase(known[47].begin() + 1);
	Check(!NextStep(40, 49, known, {0, 0}), "without a route or a waypoint here there is no step");
}

void DeathPlaceIsACorpseLandmark() {
	const auto entity = Landmark({7, 49, {12, 34}}, "where you died");
	Check(entity.kind == Kind::Corpse && entity.category == Category::Corpse && entity.placeId == Identity
		&& entity.position.x == 12 && entity.name == "where you died", "the death place is listed with the corpses, by its own identity");
}

}

int main() {
	MemoryKeepsOnePlacePerMap();
	StepFollowsKnownExits();
	WaypointOnlyWithoutAWalkingRoute();
	DeathPlaceIsACorpseLandmark();
	Passed();
	return 0;
}
