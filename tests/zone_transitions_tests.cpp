#include "zone_transitions.h"
#include "test_support.h"

#include <limits>
#include <algorithm>
#include "fixtures/spider_flayer_border.h"
#include <vector>

using namespace D2RAccess;

namespace {
constexpr uint16_t PlayerPath = 0x1C09; // observed native walking mask
constexpr uint16_t Wall = 1;

Radar::RoomGrid Room(int x, int y, int width, int height) {
	Radar::RoomGrid room{x, y, width, height, std::vector<uint16_t>(static_cast<size_t>(width) * height, 0)};
	for (int i = 0; i < width; ++i) {
		room.cells[static_cast<size_t>(i)] = Wall;
		room.cells[static_cast<size_t>(height - 1) * width + i] = Wall;
	}
	for (int i = 0; i < height; ++i) {
		room.cells[static_cast<size_t>(i) * width] = Wall;
		room.cells[static_cast<size_t>(i) * width + width - 1] = Wall;
	}
	return room;
}
void Carve(Radar::RoomGrid& room, int x, int y) {
	room.cells[static_cast<size_t>(y - room.y) * room.width + x - room.x] = 0;
}
Radar::World World(Radar::RoomGrid own, Radar::RoomGrid adjacent, Radar::Position player, int bodyPattern = 0) {
	Radar::World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = bodyPattern;
	world.walkingMask = PlayerPath;
	world.rooms = {std::move(own), std::move(adjacent)};
	return world;
}
NativeLevelExits::Layout Layout(int heightTiles) {
	NativeLevelExits::Layout layout;
	layout.level = 1;
	layout.rooms = {{{0, 0, 2, heightTiles}, true}};
	layout.neighbours = {{{2, 0, 2, heightTiles}, 2}};
	return layout;
}
}

int main() {
	auto own = Room(0, 0, 10, 10), adjacent = Room(10, 0, 10, 10);
	for (int y = 4; y <= 6; ++y) { Carve(own, 9, y); Carve(adjacent, 10, y); }
	auto world = World(own, adjacent, {6, 5}, 2);
	auto layout = Layout(2);
	auto found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].level == 2 && found[0].position.x == 9 && found[0].position.y == 5,
		"an actual three-cell opening into Blood Moor creates one zone transition at the opening");
	world.player = {8, 4};
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.y == 5,
		"a square body uses the safe middle of a three-cell gate when nearer its blocked edge");
	world.player = {6, 5};

	layout.neighbours.clear();
	Check(ZoneTransitions::Find(world, layout).empty(), "an opening into an ordinary room is not a zone transition");
	layout = Layout(2);
	Carve(adjacent, 10, 5); // retain only one open cell on the far side
	adjacent.cells[static_cast<size_t>(4) * adjacent.width] = Wall;
	adjacent.cells[static_cast<size_t>(6) * adjacent.width] = Wall;
	world = World(own, adjacent, {6, 5}, 2);
	Check(ZoneTransitions::Find(world, layout).empty(), "a gap too narrow for the player's body is not a crossing");
	world = World(own, Room(10, 0, 10, 10), {6, 5}, 0);
	Check(ZoneTransitions::Find(world, layout).empty(), "a gap facing collision in the next level is silent");

	auto largeOwn = Room(0, 0, 10, 100), largeNext = Room(10, 0, 10, 100);
	for (int y = 20; y <= 80; ++y) { Carve(largeOwn, 9, y); Carve(largeNext, 10, y); }
	world = World(largeOwn, largeNext, {8, 20});
	layout = Layout(20);
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.x == 9 && found[0].position.y == 20,
		"a long open border sounds at the nearest passable point rather than its distant midpoint");
	world.player = {50, 20};
	Check(ZoneTransitions::Find(world, layout).empty(), "a crossing outside the audible radius does not create a source");
	found = ZoneTransitions::Find(world, layout, 100);
	Check(found.size() == 1 && found[0].position.x == 9 && found[0].position.y == 20,
		"a wider radius finds the same crossing from afar, for the beacon");

	EntityRadar::Entity border{2, EntityRadar::Kind::Exit, {10, 50}, "Blood Moor", 99, false, EntityRadar::Category::Exit, 2};
	border.border = EntityRadar::Entity::Border::Bearing;
	Check(ZoneTransitions::MoveToCrossing(border, found) && border.position.x == 9 && border.position.y == 20 && border.placeId == 99,
		"an area border is placed at its walkable crossing and keeps its identity");
	EntityRadar::Entity stairs{3, EntityRadar::Kind::Exit, {10, 50}, "Cave", 98, false, EntityRadar::Category::Exit, 9};
	Check(!ZoneTransitions::MoveToCrossing(stairs, found) && stairs.position.y == 50, "an exit to a level with no crossing stays put");
	EntityRadar::Entity chest{4, EntityRadar::Kind::Object, {10, 50}, "Chest"};
	Check(!ZoneTransitions::MoveToCrossing(chest, found) && chest.position.y == 50, "anything but an exit stays put");

	// Tamoe Highland, 2026-09-28: the Monastery Gate was walked into before its edge's middle was ever seen.
	const ZoneTransitions::Filter nearPlayer = [](const std::vector<EntityRadar::Entity>& entities) {
		std::vector<EntityRadar::Entity> kept;
		for (const auto& entity : entities) if (entity.position.y <= 25) kept.push_back(entity);
		return kept;
	};
	EntityRadar::Entity unseenBorder{2, EntityRadar::Kind::Exit, {10, 50}, "Blood Moor", 99, false, EntityRadar::Category::Exit, 2};
	unseenBorder.border = EntityRadar::Entity::Border::Bearing;
	const EntityRadar::Entity nearChest{5, EntityRadar::Kind::Object, {5, 10}, "Chest"};
	const auto kept = ZoneTransitions::FoundAtCrossings({unseenBorder, stairs, nearChest}, found, nearPlayer);
	Check(kept.size() == 2 && kept[0].placeId == 99 && kept[0].position.y == 20 && kept[0].border == EntityRadar::Entity::Border::Crossing && kept[1].id == 5,
		"a border is remembered at the measured opening, retaining its naming identity");
	Check(ZoneTransitions::FoundAtCrossings({unseenBorder}, {}, nearPlayer).empty(),
		"a border with no crossing in reach is not discovered at a guessed point");

	const ZoneTransitions::Filter charted = [](const auto& entities) { return entities; };
	const auto measured = ZoneTransitions::FoundAtCrossings({unseenBorder}, found, charted);
	Check(measured.size() == 1 && measured[0].position.y == 20,
		"charting the midpoint does not short-circuit the measured crossing");
	Check(ZoneTransitions::FoundAtCrossings({unseenBorder}, {}, charted).empty(),
		"an automap-charted guessed midpoint is still not a measured exit");
	auto restored = unseenBorder;
	Check(ZoneTransitions::RestoreCrossing(restored, measured[0]) && restored.position.y == 20,
		"a fresh area bearing restores the remembered crossing after its floor unloads");
	Check(!ZoneTransitions::RestoreCrossing(restored, unseenBorder) && restored.position.y == 20,
		"an approximate point cannot overwrite a measured crossing");
	auto newer = restored;
	newer.position.y = 21;
	Check(!ZoneTransitions::RestoreCrossing(newer, restored) && newer.position.y == 21,
		"a currently measured crossing wins over the previous measurement");
	std::vector<EntityRadar::Entity> reread{unseenBorder};
	ZoneTransitions::MergeKnown(reread, measured);
	Check(reread.size() == 1 && reread[0].position.y == 20,
		"map reload retains one landmark at the real crossing");
	auto legacy = unseenBorder;
	legacy.border = EntityRadar::Entity::Border::None;
	reread = {unseenBorder};
	ZoneTransitions::MergeKnown(reread, {legacy});
	Check(reread.size() == 1 && reread[0].border == EntityRadar::Entity::Border::Bearing,
		"an old unverified midpoint does not become a measured crossing");
	for (const uint32_t destination : {107u, 113u, 128u}) {
		// Act 4 City/River stairs; Act 5 cave and summit/Worldstone stairs.
		auto warp = stairs;
		warp.destinationLevel = destination;
		for (const uint8_t unit : {uint8_t(5), EntityRadar::Entity::NoUnit}) {
			warp.unit = unit;
			Check(!ZoneTransitions::MoveToCrossing(warp, {{{9, 20}, destination}}) && warp.position.y == 50,
				"a warp or its unloaded preset is never relocated to an area border with the same destination");
		}
	}

	own = Room(0, 0, 10, 10); adjacent = Room(10, 0, 10, 10);
	for (int y : {2, 7}) { Carve(own, 9, y); Carve(adjacent, 10, y); }
	world = World(own, adjacent, {7, 7});
	layout = Layout(2);
	layout.neighbours.push_back(layout.neighbours.front());
	found = ZoneTransitions::Find(world, layout);
	Check(found.size() == 1 && found[0].position.y == 7,
		"repeated neighbour records and multiple gaps keep one nearest cue per destination");
	layout.level = 9;
	Check(ZoneTransitions::Find(world, layout).empty(), "a stale layout from another level is ignored");
	layout.level = 1;
	world.player.x = std::numeric_limits<double>::quiet_NaN();
	Check(ZoneTransitions::Find(world, layout).empty(), "invalid player coordinates do not become collision-grid indices");
	{
		EntityRadar::Entity bearing;
		bearing.kind = EntityRadar::Kind::Exit;
		bearing.category = EntityRadar::Category::Exit;
		bearing.border = EntityRadar::Entity::Border::Bearing;
		bearing.destinationLevel = 76;
		bearing.position = {5160, 5000}; // failed from Meshif's dock in the user's log
		NativeLevelExits::Layout dock;
		dock.level = 75;
		dock.rooms = {{{1000, 1000, 64, 48}, true}};
		dock.neighbours = {{{1024, 808, 64, 192}, 76}};
		const auto edge = ZoneTransitions::ApproachRegions(bearing, dock);
		Check(edge.size() == 1 && edge[0].minX == 5120 && edge[0].maxX == 5320 && edge[0].minY == 5000 && edge[0].maxY == 5002,
			"unmeasured Spider Forest exit approaches the shared edge, including the real off-midpoint opening");
		Check(bearing.position.x == 5160 && bearing.border == EntityRadar::Entity::Border::Bearing,
			"approaching the edge does not claim a collision-measured crossing");
		bearing.border = EntityRadar::Entity::Border::Crossing;
		Check(ZoneTransitions::ApproachRegions(bearing, dock).empty(), "a measured crossing keeps its exact target");
		bearing.border = EntityRadar::Entity::Border::None;
		Check(ZoneTransitions::ApproachRegions(bearing, dock).empty(), "stairs and warp presets keep their exact target");
		bearing.border = EntityRadar::Entity::Border::Bearing;
		bearing.destinationLevel = 99;
		Check(ZoneTransitions::ApproachRegions(bearing, dock).empty(), "an unrelated area cannot provide an approach edge");
		bearing.destinationLevel = 76;
		for (const auto& other : std::vector<NativeLevelExits::TileRect>{{1064, 1000, 8, 48}, {992, 1000, 8, 48}, {1000, 1048, 64, 8}}) {
			dock.neighbours.front().rect = other;
			Check(!ZoneTransitions::ApproachRegions(bearing, dock).empty(), "shared-edge approach works on every side of an outdoor area");
		}
	}
	{
		// Current user's logical-room/near-list capture. Collision is tested
		// separately: these rectangles alone do not establish a doorway.
		auto forest = SpiderFlayerBorder::Layout();
		EntityRadar::Entity bearing;
		bearing.kind = EntityRadar::Kind::Exit; bearing.category = EntityRadar::Category::Exit;
		bearing.border = EntityRadar::Entity::Border::Bearing; bearing.destinationLevel = 78;
		const auto sections = ZoneTransitions::ApproachRegions(bearing, forest);
		Check(sections.size() == 8 && sections.front().minX == 5000 && sections.back().maxX == 5320,
			"captured Flayer near lists include the whole eight-room shared edge, not the first strip");
		for (const auto& section : sections)
			Check(section.minY == 4040 && section.maxY == 4042 && section.maxX - section.minX == 40,
				"captured Flayer approach stays just inside each actual forest border room");
		std::reverse(forest.neighbours.begin(), forest.neighbours.end());
		forest.neighbours.push_back(forest.neighbours.front());
		Check(ZoneTransitions::ApproachRegions(bearing, forest) == sections,
			"reordering and duplicate native near records cannot change the candidate border or route cache");
		bearing.destinationLevel = 77;
		const auto marsh = ZoneTransitions::ApproachRegions(bearing, forest);
		Check(marsh.size() == 16 && marsh.front().minX == 5318 && marsh.back().maxY == 4680,
			"the captured long Great Marsh border also includes every neighboring room");
	}
	{
		// Several acts/directions, with holes and two disjoint boundary sections.
		// These are synthetic shapes, not captures of those acts' terrain.
		for (const uint32_t level : {1u, 76u, 77u, 78u, 83u, 104u, 105u, 106u, 109u, 110u, 111u, 112u}) {
			for (int side = 0; side < 4; ++side) {
				NativeLevelExits::Layout layout;
				layout.level = level;
				layout.rooms = {{{100, 100, 8, 8}, false}, {{100, 116, 8, 8}, false}};
				layout.neighbours = {{{108, 100, 8, 8}, level + 1}, {{108, 116, 8, 8}, level + 1}};
				if (side == 1) for (auto& room : layout.neighbours) room.rect.x = 92;
				if (side >= 2) {
					for (auto& room : layout.rooms) { std::swap(room.rect.x, room.rect.y); }
					for (auto& room : layout.neighbours) { std::swap(room.rect.x, room.rect.y); }
					if (side == 3) for (auto& room : layout.neighbours) room.rect.y = 92;
				}
				EntityRadar::Entity bearing;
				bearing.kind = EntityRadar::Kind::Exit; bearing.category = EntityRadar::Category::Exit;
				bearing.border = EntityRadar::Entity::Border::Bearing; bearing.destinationLevel = level + 1;
				const auto sections = ZoneTransitions::ApproachRegions(bearing, layout);
				Check(sections.size() == 2, "each outdoor direction preserves separate border sections without filling a hole");
				for (const auto& section : sections)
					Check((side < 2 ? section.maxY - section.minY : section.maxX - section.minX) == 40,
						"an irregular boundary targets real room spans instead of their enclosing rectangle");
			}
		}
	}
	Passed();
}
