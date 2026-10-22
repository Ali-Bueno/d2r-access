#include "map_survey.h"
#include "test_support.h"

#include <string>

using namespace D2RAccess;
using namespace MapSurvey;

namespace {

constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask the sonar tests use
constexpr int      Size = 40;

// One open room with the player in the middle, same convention as the entity
// radar tests: screen up is world (-X,-Y).
Radar::World OpenWorld() {
	Radar::World world;
	world.player      = {20, 20};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}

void Block(Radar::World& world, int x, int y) {
	world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall;
}

// A wall across the whole room except one gap, so a target directly behind it
// is only reachable the long way round.
void WallWithGap(Radar::World& world, int y, int gapX) {
	for (int x = 0; x < Size; ++x) {
		if (x != gapX) Block(world, x, y);
	}
}

EntityRadar::Entity Make(uint32_t id, double x, double y, EntityRadar::Kind kind = EntityRadar::Kind::Item) {
	EntityRadar::Entity entity;
	entity.id       = id;
	entity.kind     = kind;
	entity.position = {x, y};
	entity.name     = "thing " + std::to_string(id);
	return entity;
}

Navigation::Grid GridFor(const Radar::World& world) { return Navigation::Build(world, Size); }

void StepsGoAroundWalls() {
	Radar::World world = OpenWorld();
	// Player at (20,20); the target sits four cells straight up the world Y
	// axis, with a solid wall between them and the only gap far to the left.
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 20, 16)});
	Check(landmarks.size() == 1, "one object surveyed");
	const Landmark& only = landmarks.front();
	Check(only.reachable, "a target behind a wall with a gap is still reachable");
	Check(only.straightLine < 5, "its straight line is short");
	Check(only.steps > 10, "but its walking distance goes the long way round, through the gap");
}

void UnreachableIsReportedAsSuch() {
	Radar::World world = OpenWorld();
	// Seal a single cell off completely; nothing can walk to it.
	Block(world, 9, 10);
	Block(world, 11, 10);
	Block(world, 10, 9);
	Block(world, 10, 11);
	Block(world, 9, 9);
	Block(world, 11, 11);
	Block(world, 9, 11);
	Block(world, 11, 9);
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 10, 10)});
	Check(landmarks.size() == 1, "the sealed object is still catalogued");
	Check(!landmarks.front().reachable, "an object with no route is reported unreachable, not as a straight line");
	Check(landmarks.front().steps < 0, "and carries no step count");
}

void NearestByStepsNotByStraightLine() {
	Radar::World world = OpenWorld();
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	// Behind the wall: close in a straight line, far on foot.
	// Below the player: further in a straight line, but an open walk.
	const auto landmarks = Survey(world, grid, {Make(1, 20, 16), Make(2, 20, 28)});
	Check(landmarks.size() == 2, "both objects surveyed");
	Check(landmarks.front().entity.id == 2, "the one that is actually closer on foot is named first");
	Check(landmarks.front().steps < landmarks.back().steps, "ordering follows steps");
}

void ReachableSortsAheadOfUnreachable() {
	Radar::World world = OpenWorld();
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx != 0 || dy != 0) Block(world, 10 + dx, 10 + dy);
		}
	}
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {Make(1, 10, 10), Make(2, 30, 30)});
	Check(landmarks.front().entity.id == 2, "a reachable object outranks a nearer sealed one");
	Check(!landmarks.back().reachable, "the sealed one is still listed, last");
}

void OutOfViewObjectsAreKept() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	Check(registry.Entities().size() == 1, "a seen object is catalogued");

	// The grid covers the whole room here, so an absent object IS provably gone.
	for (int i = 0; i < 3; ++i) registry.Observe(1, {}, grid);
	Check(registry.Entities().empty(), "an object the map can see is gone is dropped after repeated misses");
}

void AbsenceOutsideTheMapIsNotForgetting() {
	Radar::World world = OpenWorld();
	// A grid that knows only a small window around the player leaves the far
	// corner unknown, so absence there proves nothing.
	const Navigation::Grid narrow = Navigation::Build(world, 3);
	Registry registry;
	registry.Observe(1, {Make(1, 35, 35)}, narrow);
	for (int i = 0; i < 10; ++i) registry.Observe(1, {}, narrow);
	Check(registry.Entities().size() == 1,
		"an object outside what the map currently knows is kept: that is what the player still has to walk to");
}

void AbsenceBeyondTheScanIsNotForgetting() {
	Radar::World world = OpenWorld();
	// The grid knows the whole room, but the object stood some 25 subtiles
	// away: the game may still hold the floor there and not the thing on it.
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 38, 38)}, grid);
	for (int i = 0; i < 10; ++i) registry.Observe(1, {}, grid);
	Check(registry.Entities().size() == 1,
		"an object beyond the scan's reach is kept however long it is missing, to walk back to");
}

void RecreatedObjectTakesItsOldPlace() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	auto shrine = Make(28, 38, 38, EntityRadar::Kind::Object);
	shrine.name = "Refilling Shrine";
	registry.Observe(1, {shrine}, grid);
	auto again = shrine;
	again.id = 61;
	const std::vector<EntityRadar::Entity> present{again};
	registry.Observe(1, present, grid, {}, &present);
	Check(registry.Entities().size() == 1 && registry.Entities().front().id == 61,
		"an object loaded again under a new id is one entry, with the id it has now");

	Registry items;
	auto potion = Make(4, 38, 38);
	potion.name = "Minor Healing Potion";
	auto other = potion;
	other.id = 5;
	const std::vector<EntityRadar::Entity> first{potion, other};
	items.Observe(1, first, grid, {}, &first);
	Check(items.Entities().size() == 2, "two items of one name on one spot are two: an item may lie on another");
	auto potionAgain = potion, otherAgain = other;
	potionAgain.id = 6;
	otherAgain.id = 7;
	const std::vector<EntityRadar::Entity> again2{potionAgain, otherAgain};
	items.Observe(1, again2, grid, {}, &again2);
	Check(items.Entities().size() == 2 && items.Entities().front().id != 4 && items.Entities().back().id != 5,
		"loaded again under new ids, they are still two, not four");

	Registry monsters;
	auto fallen = Make(9, 38, 30, EntityRadar::Kind::Enemy);
	fallen.name = "Fallen";
	monsters.Observe(1, {fallen}, grid);
	auto moved = fallen;
	moved.id = 90;
	moved.position = {36, 33};
	const std::vector<EntityRadar::Entity> pack{moved};
	monsters.Observe(1, pack, grid, {}, &pack);
	Check(monsters.Entities().size() == 1 && monsters.Entities().front().id == 90,
		"a monster of the same name near where one was lost from the loaded rooms takes its entry");
}

// A tester's Den of Evil: a Fallen and a Zombie stayed listed where nothing stood.
void EnemiesFollowTheirUnitAndDie() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	auto fallen = Make(8, 30, 30, EntityRadar::Kind::Enemy);
	const std::vector<EntityRadar::Entity> first{fallen};
	registry.Observe(1, first, grid, {}, &first);
	auto fled = fallen;
	fled.position = {38, 38};
	const std::vector<EntityRadar::Entity> loaded{fled};
	registry.Observe(1, {}, grid, {}, &loaded);
	Check(registry.Entities().size() == 1 && registry.Entities().front().position.x == 38,
		"a discovered enemy out of sight but still loaded is listed where it stands now");
	const std::vector<EntityRadar::Entity> nothing;
	auto near = fallen;
	near.id = 9;
	const std::vector<EntityRadar::Entity> second{near};
	Registry dying;
	dying.Observe(1, second, grid, {}, &second);
	dying.Observe(1, {}, grid, {}, &nothing);
	Check(dying.Entities().empty(), "an enemy missing from its place within reach is gone at once");
	registry.ForgetEnemies({8});
	Check(registry.Entities().empty(), "an enemy the game holds dead is forgotten wherever it lay");
	Registry items;
	const std::vector<EntityRadar::Entity> potion{Make(8, 38, 38)};
	items.Observe(1, potion, grid, {}, &potion);
	items.ForgetEnemies({8});
	Check(items.Entities().size() == 1, "a dead monster's id never removes an item that shares it");
}

void ChangingLevelClears() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	registry.Observe(2, {}, grid);
	Check(registry.Entities().empty(), "a new level starts from nothing");
	Check(registry.Level() == 2, "and records which level it is");
}

void ReobservingRefreshesPosition() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	registry.Observe(1, {Make(1, 30, 30)}, grid);
	registry.Observe(1, {Make(1, 25, 25)}, grid);
	Check(registry.Entities().size() == 1, "the same object is not duplicated");
	Check(registry.Entities().front().position.x == 25, "its position follows the world");
}

void KindFiltering() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	const auto landmarks = Survey(world, grid, {
		Make(1, 22, 22, EntityRadar::Kind::Enemy),
		Make(2, 24, 24, EntityRadar::Kind::Exit),
		Make(3, 26, 26, EntityRadar::Kind::Enemy),
	});
	const auto enemies = OfKind(landmarks, EntityRadar::Kind::Enemy, 5);
	Check(enemies.size() == 2, "only the requested kind comes back");
	Check(enemies.front().entity.id == 1, "nearest first within the kind");
	Check(OfKind(landmarks, EntityRadar::Kind::Enemy, 1).size() == 1, "the count caps the list");
	Check(OfKind(landmarks, EntityRadar::Kind::Npc, 5).empty(), "an absent kind yields nothing");
}

void CategoryCycling() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	auto with = [](uint32_t id, double x, double y, EntityRadar::Category category) {
		EntityRadar::Entity entity = Make(id, x, y);
		entity.category = category;
		return entity;
	};
	const auto landmarks = Survey(world, grid, {
		with(1, 22, 22, EntityRadar::Category::Treasure),
		with(2, 30, 30, EntityRadar::Category::Exit),
		with(3, 24, 24, EntityRadar::Category::Treasure),
		with(4, 26, 26, EntityRadar::Category::Shrine),
	});

	const auto present = Categories(landmarks);
	Check(present.size() == 3, "only categories that have something in them are offered");
	Check(present.front() == EntityRadar::Category::Exit, "the way out comes first for a stranded player");

	const auto treasure = OfCategory(landmarks, EntityRadar::Category::Treasure);
	Check(treasure.size() == 2, "a category returns all of its members");
	Check(treasure.front().entity.id == 1, "nearest on foot first inside the category");
	Check(OfCategory(landmarks, EntityRadar::Category::Npc).empty(), "an absent category is empty");

	const auto next = StepCategory(landmarks, EntityRadar::Category::Exit, 1);
	Check(next == EntityRadar::Category::Treasure, "stepping forward skips categories with nothing in them");
	Check(StepCategory(landmarks, EntityRadar::Category::Exit, -1) == EntityRadar::Category::Shrine,
		"stepping back from the first wraps to the last");
	Check(StepCategory(landmarks, EntityRadar::Category::Shrine, 1) == EntityRadar::Category::Exit,
		"stepping forward from the last wraps to the first");
	Check(StepCategory(landmarks, EntityRadar::Category::Npc, 1) == EntityRadar::Category::Exit,
		"stepping from a category that is not present starts at the first");
	Check(StepCategory({}, EntityRadar::Category::Exit, 1) == EntityRadar::Category::Unknown,
		"an empty map offers no category at all");

	const auto people = Survey(world, grid, {
		with(5, 22, 22, EntityRadar::Category::Enemy),
		with(6, 24, 24, EntityRadar::Category::Player),
		with(7, 26, 26, EntityRadar::Category::Npc),
	});
	const auto order = Categories(people);
	Check(order.size() == 3 && order[0] == EntityRadar::Category::Npc && order[1] == EntityRadar::Category::Player
		&& order[2] == EntityRadar::Category::Enemy, "other players are their own category, between NPCs and enemies");

	const auto rooms = Survey(world, grid, {
		with(8, 22, 22, EntityRadar::Category::Room),
		with(9, 24, 24, EntityRadar::Category::UnexploredRoom),
		with(10, 26, 26, EntityRadar::Category::Exit),
	});
	const auto roomOrder = Categories(rooms);
	Check(roomOrder.size() == 3 && roomOrder[0] == EntityRadar::Category::Exit
		&& roomOrder[1] == EntityRadar::Category::UnexploredRoom && roomOrder[2] == EntityRadar::Category::Room,
		"where is left to explore follows the ways out, before the rooms already walked");

	const auto quest = Survey(world, grid, {
		with(11, 22, 22, EntityRadar::Category::Interactable),
		with(12, 24, 24, EntityRadar::Category::Npc),
		with(13, 26, 26, EntityRadar::Category::Quest),
	});
	const auto questOrder = Categories(quest);
	Check(questOrder.size() == 3 && questOrder[0] == EntityRadar::Category::Quest
		&& questOrder[1] == EntityRadar::Category::Npc && questOrder[2] == EntityRadar::Category::Interactable,
		"quest objects are their own category, ahead of the people and objects around them");
}

void ExplorationDiscoversOnlyNearbyVisibleTargets() {
	Radar::World world = OpenWorld();
	auto nearExit = Make(1, 22, 20, EntityRadar::Kind::Exit);
	nearExit.placeId = 100;
	auto distantExit = Make(2, 38, 20, EntityRadar::Kind::Exit);
	distantExit.placeId = 200;
	const auto found = Discoverable(world, {nearExit, distantExit, Make(3, 39, 39)});
	Check(found.size() == 1 && found.front().id == 1,
		"nearby visible exit is discovered, but generated distant exits and same-room objectives are hidden");

	Block(world, 21, 20);
	Check(Discoverable(world, {nearExit}).empty(), "a fixed exit behind a wall is not a new discovery");
	world.rooms[0].cells[20 * Size + 21] = 0;
	world.rooms[0].cells[20 * Size + 22] = 0x20; // native unknown subtile
	Check(Discoverable(world, {nearExit}).empty(), "a nearby generated exit on unknown collision is not discovered");
	world.rooms[0].cells[20 * Size + 22] = 0;
	world.rooms[0].cells[20 * Size + 21] = 0x20;
	Check(Discoverable(world, {nearExit}).empty(), "unknown terrain between player and exit cannot reveal it");
}

void KnownLandmarkSurvivesLossOfSight() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Registry registry;
	const auto fixed = Make(1, 24, 20, EntityRadar::Kind::Object);
	const std::vector<EntityRadar::Entity> firstPresent{fixed};
	registry.Observe(1, Discoverable(world, firstPresent), grid, {}, &firstPresent);
	Check(registry.Entities().size() == 1, "visible landmark enters the catalogue");
	Block(world, 22, 20);
	const auto hidden = Make(1, 28, 20, EntityRadar::Kind::Object);
	const std::vector<EntityRadar::Entity> present{hidden};
	for (int i = 0; i < 5; ++i)
		registry.Observe(1, Discoverable(world, present), grid, {}, &present);
	Check(registry.Entities().size() == 1 && registry.Entities().front().position.x == 24,
		"known landmark remains at its last discovered position when currently hidden");
}

void ExplorationSurveyUsesOrientation() {
	Radar::World world = OpenWorld();
	WallWithGap(world, 18, 4);
	const Navigation::Grid grid = GridFor(world);
	const auto assisted = Survey(world, grid, {Make(1, 20, 16)}, true);
	const auto exploration = Survey(world, grid, {Make(1, 20, 16)}, false);
	Check(assisted.front().steps > 10, "full assistance keeps its walking route distance");
	Check(!exploration.front().reachable && exploration.front().steps < 0 && exploration.front().straightLine < 5,
		"exploration reports only direct distance and direction");
	// Screen sectors clockwise from up: 1 is up and to the right, 7 up and to the left.
	Check(Radar::ScreenSector(exploration.front().offset) == 1, "a target up the world Y axis is up and to the right on screen");
	const auto diagonal = Survey(world, grid, {Make(2, 16, 20)}, false);
	Check(Radar::ScreenSector(diagonal.front().offset) == 7, "a target up the world X axis is up and to the left on screen");
}

void ReturningToLevelRetainsFixedLandmarks() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Memory memory;
	auto waypoint = Make(1, 24, 20, EntityRadar::Kind::Object);
	waypoint.category = EntityRadar::Category::Waypoint;
	const auto enemy = Make(2, 25, 20, EntityRadar::Kind::Enemy);
	const auto loot = Make(3, 26, 20, EntityRadar::Kind::Item);
	memory.Observe(1, {waypoint, enemy, loot}, grid, true);
	memory.Observe(2, {}, grid, true);
	Check(memory.Entities().empty(), "other level does not show the previous level's entries");
	memory.Select(1, true);
	Check(memory.Entities().size() == 1 && memory.Entities().front().id == waypoint.id,
		"returning in the same session retains a known waypoint but no enemy or loot");
	memory.Clear();
	memory.Select(1, true);
	Check(memory.Entities().empty(), "a new game or mode reset forgets session landmarks");
}

void FullAssistanceKeepsEveryLevel() {
	Radar::World world = OpenWorld();
	const Navigation::Grid grid = GridFor(world);
	Memory memory;
	auto enemy = Make(2, 25, 20, EntityRadar::Kind::Enemy);
	enemy.category = EntityRadar::Category::Enemy;
	auto loot = Make(3, 26, 20);
	loot.category = EntityRadar::Category::Item;
	memory.Observe(1, {Make(1, 24, 20, EntityRadar::Kind::Object), enemy, loot}, grid, false);
	memory.Select(2, false);
	Check(memory.Entities().empty(), "another level shows none of this one's");
	memory.Select(1, false);
	Check(memory.Entities().size() == 3, "full assistance keeps everything a level discovered, to walk back to");
}

}

void ChartedRoomsAdmitLandmarksOutOfSight() {
	Radar::World world = OpenWorld();
	const auto near = Make(1, 22, 20, EntityRadar::Kind::Object);
	const auto charted = Make(2, 38, 38, EntityRadar::Kind::Object);
	const auto uncharted = Make(3, 39, 2, EntityRadar::Kind::Object);
	// One 2x2-tile room (10 subtiles) in the far corner; the rest of the map is not drawn.
	const std::vector<NativeRooms::Room> rooms{{6, 6, 2, 2, 1}};
	const auto listed = Charted(world, {near, charted, uncharted}, rooms);
	Check(listed.size() == 2 && listed[0].id == 1 && listed[1].id == 2,
		"a landmark in sight or in a room the automap has drawn is listed; one in neither is not");
}

void ScannerSplitsLandmarksFromLiveEnemies() {
	auto enemy = Make(1, 20, 20, EntityRadar::Kind::Enemy);
	enemy.category = EntityRadar::Category::Enemy;
	auto chest = Make(2, 21, 20, EntityRadar::Kind::Object);
	chest.category = EntityRadar::Category::Treasure;
	auto questChest = Make(3, 22, 20, EntityRadar::Kind::Object);
	questChest.category = EntityRadar::Category::Treasure;
	questChest.questObject = true;
	auto loot = Make(4, 23, 20);
	loot.category = EntityRadar::Category::Item;
	auto waypoint = Make(5, 24, 20, EntityRadar::Kind::Object);
	waypoint.category = EntityRadar::Category::Waypoint;
	auto ally = Make(6, 25, 20, EntityRadar::Kind::Player);
	ally.category = EntityRadar::Category::Player;
	const auto scene = Split({enemy, chest, questChest, loot, waypoint, ally});
	Check(scene.enemies.size() == 1 && scene.enemies.front().id == 1, "enemies are kept apart to be read live");
	Check(scene.players.size() == 1 && scene.players.front().id == 6, "other players are kept apart to be read live, never remembered");
	Check(scene.landmarks.size() == 3 && scene.landmarks[0].id == 2 && scene.landmarks[1].id == 3 && scene.landmarks[2].id == 5,
		"a chest, a quest container and a waypoint are landmarks; items are left to their own reader");
}

// What a later game on the same map may be told about: only what the layout places.
void FixedPlacesAreTheLayoutsOwn() {
	using EntityRadar::Category;
	using EntityRadar::Kind;
	const auto place = [](Kind kind, Category category, uint64_t id) {
		auto entity = Make(1, 10, 10, kind);
		entity.category = category;
		entity.placeId = id;
		return entity;
	};
	Check(Fixed(place(Kind::Exit, Category::Exit, 7)), "stairs, passages and borders are fixed");
	Check(Fixed(place(Kind::Exit, Category::Waypoint, 7)), "a waypoint is fixed");
	Check(Fixed(place(Kind::Object, Category::Stash, 7)), "the stash is fixed");
	Check(!Fixed(place(Kind::Exit, Category::Waypoint, 0)), "without a place identity nothing can be matched in another game");
	Check(!Fixed(place(Kind::Exit, Category::Portal, 7)), "a portal may be gone next game");
	Check(!Fixed(place(Kind::Object, Category::Shrine, 7)) && !Fixed(place(Kind::Object, Category::Treasure, 7))
		&& !Fixed(place(Kind::Object, Category::Interactable, 7)), "shrines, chests and other objects change between games");
	Check(!Fixed(place(Kind::Npc, Category::Npc, 7)) && !Fixed(place(Kind::Room, Category::Room, 7)),
		"people move, and rooms have their own memory");
}

void TownNpcsListedOnceSeen() {
	auto fara = Make(1, 30, 30, EntityRadar::Kind::Npc);
	fara.category = EntityRadar::Category::Npc; fara.placeId = 200;
	auto atma = Make(2, 90, 90, EntityRadar::Kind::Npc);
	atma.category = EntityRadar::Category::Npc; atma.placeId = 177;
	TownNpcSightings sightings;
	Check(sightings.Seen(40, {fara, atma}).empty(), "no town NPC is listed before it is seen");
	sightings.Found(40, {fara});
	auto walked = fara;
	walked.id = 55; walked.position = {60, 60};
	const auto seen = sightings.Seen(40, {walked, atma});
	Check(seen.size() == 1 && seen.front().id == 55 && seen.front().position.x == 60,
		"a seen NPC stays listed, at its live unit's place, and an unseen one is not");
	Check(sightings.Seen(1, {fara}).empty(), "a sighting belongs to its own town");
	sightings.Clear();
	Check(sightings.Seen(40, {fara}).empty(), "a new game starts with nobody seen");
}

void SeenNpcWithoutPresetIsRemembered() {
	constexpr double Far = 900;
	auto jerhyn = Make(1156, 40, 40, EntityRadar::Kind::Npc);
	jerhyn.category = EntityRadar::Category::Npc; jerhyn.placeId = 202; jerhyn.name = "Jerhyn"; jerhyn.unit = 1;
	TownNpcSightings sightings;
	Check(sightings.Listed(40, {}, true).empty(), "an NPC never seen and not listed is not there");
	sightings.Found(40, {jerhyn});
	auto moved = jerhyn;
	moved.position = {Far, Far};
	sightings.Found(40, {moved});
	const auto gone = sightings.Listed(40, {}, true);
	Check(gone.size() == 1 && gone.front().name == "Jerhyn" && gone.front().position.x == Far
		&& gone.front().unit == EntityRadar::Entity::NoUnit && gone.front().placeId == 202,
		"a seen NPC out of the loaded rooms stays, at its last place, with no unit to command");
	Check(sightings.Listed(40, {}, false).size() == 1 && sightings.Listed(1, {}, true).empty(),
		"it is listed with the switch off too, and only in its own town");
	auto loaded = jerhyn;
	loaded.id = 77; loaded.position = {45, 45};
	const auto live = sightings.Listed(40, {loaded}, true);
	Check(live.size() == 1 && live.front().id == 77 && live.front().position.x == 45, "loaded again, the live unit replaces the remembered one");
	sightings.Clear();
	Check(sightings.Listed(40, {}, true).empty(), "a new game forgets it");
}

EntityRadar::Entity TownPortal(uint32_t id, const char* owner, const char* level, double x = 38, double y = 38) {
	auto portal = Make(id, x, y, EntityRadar::Kind::Object);
	portal.category = EntityRadar::Category::Portal; portal.owner = owner; portal.name = level;
	return portal;
}

void NewPortalOfTheSameOwnerReplacesTheOld() {
	const Navigation::Grid grid = GridFor(OpenWorld());
	Registry registry;
	registry.Observe(1, {TownPortal(10, "Raine", "Maggot Lair"), TownPortal(11, "Host", "Lost City", 20, 20)}, grid);
	Check(registry.Entities().size() == 2, "two owners, two portals");
	const std::vector<EntityRadar::Entity> now{TownPortal(12, "Host", "Tower", 70, 70)};
	registry.Observe(1, now, grid, {}, &now);
	int host = 0;
	for (const auto& entity : registry.Entities()) host += entity.owner == "Host";
	Check(registry.Entities().size() == 2 && host == 1, "a player's new portal replaces the old one wherever it stood");
	bool danis = false;
	for (const auto& entity : registry.Entities()) danis |= entity.id == 10;
	Check(danis, "another owner's portal is untouched");
	bool moved = false;
	for (const auto& entity : registry.Entities()) moved |= entity.owner == "Host" && entity.id == 12 && entity.position.x == 70;
	Check(moved, "the old entry takes the new portal's unit and place");
	Check(EntityRadar::Recreated(TownPortal(11, "Host", "Host's portal", 20, 20), TownPortal(12, "Host", "Host's portal", 70, 70)),
		"the beacon following the old one is led to the new one (a tester, 2026-10-22)");
	Check(!EntityRadar::Recreated(TownPortal(13, "", "Catacombs Level 4", 20, 20), TownPortal(14, "", "Catacombs Level 4", 70, 70)),
		"portals with no owner, the event and shrine ones, stay apart");
}

void RosterDropsAPortalItNoLongerNames() {
	const Navigation::Grid grid = GridFor(OpenWorld());
	Registry registry;
	registry.Observe(1, {TownPortal(10, "Host", "Lost City")}, grid);
	const std::vector<EntityRadar::Entity> none;
	Check(registry.DropStalePortals({{"Host", 10, 0}}, none).empty(), "the roster's local id names it: kept");
	Check(registry.DropStalePortals({{"Host", 0, 10}}, none).empty(), "the roster's remote id names it: kept");
	Check(registry.DropStalePortals({{"Host", 0, 0}}, none).empty(), "no ids at all says nothing: kept");
	Check(registry.DropStalePortals({{"Other", 5, 6}}, none).empty(), "another member's ids say nothing: kept");
	const std::vector<EntityRadar::Entity> loaded{TownPortal(10, "Host", "Lost City")};
	Check(registry.DropStalePortals({{"Host", 20, 21}}, loaded).empty(), "a portal the game has loaded is never dropped");
	const auto dropped = registry.DropStalePortals({{"Host", 20, 21}}, none);
	Check(dropped.size() == 1 && dropped.front().id == 10 && dropped.front().local == 20 && dropped.front().remote == 21
		&& registry.Entities().empty(), "ids naming another portal drop the remembered one, said once");
	Check(registry.DropStalePortals({{"Host", 20, 21}}, none).empty(), "nothing is dropped twice");
}

void TownNpcSwitchListsEveryoneOrTheSeen() {
	auto fara = Make(1, 30, 30, EntityRadar::Kind::Npc);
	fara.category = EntityRadar::Category::Npc; fara.placeId = 200;
	auto standIn = Make(178, 900, 900, EntityRadar::Kind::Npc); // an unloaded NPC at its spawn point
	standIn.category = EntityRadar::Category::Npc; standIn.placeId = 178;
	TownNpcSightings sightings;
	sightings.Found(40, {fara});
	Check(sightings.Listed(40, {fara, standIn}, true).size() == 2, "switch on: every town NPC is listed, unseen stand-ins included");
	const auto seen = sightings.Listed(40, {fara, standIn}, false);
	Check(seen.size() == 1 && seen.front().placeId == 200, "switch off: only the NPCs already seen");
	sightings.Found(40, {standIn});
	auto loaded = standIn;
	loaded.id = 4242; loaded.position = {905, 896};
	const auto later = sightings.Listed(40, {fara, loaded}, false);
	Check(later.size() == 2 && later.back().id == 4242, "a stand-in seen stays seen once its live unit replaces it");
}


// Unexplored areas one per direction in turn: three up and one each right and down
// read up, right, down, then the other two up (a tester, 2026-10-21).
void SpreadTakesOneWayAtATime() {
	const auto at = [](uint32_t id, double x, double y) {
		MapSurvey::Landmark landmark;
		landmark.entity.id = id;
		landmark.offset = {x, y};
		return landmark;
	};
	const std::vector<MapSurvey::Landmark> nearestFirst{
		at(1, -1, -1), at(2, -2, -2), at(3, -3, -3), at(4, 4, -4), at(5, 5, 5)};
	const auto spread = MapSurvey::SpreadByDirection(nearestFirst);
	std::vector<uint32_t> order;
	for (const auto& landmark : spread) order.push_back(landmark.entity.id);
	Check(order == std::vector<uint32_t>{1, 4, 5, 2, 3}, "the nearest of each direction first, then the next round");
	Check(MapSurvey::SpreadByDirection({}).empty(), "nothing to spread is nothing");
}

int main() {
	TownNpcsListedOnceSeen();
	TownNpcSwitchListsEveryoneOrTheSeen();
	SeenNpcWithoutPresetIsRemembered();
	NewPortalOfTheSameOwnerReplacesTheOld();
	RosterDropsAPortalItNoLongerNames();
	StepsGoAroundWalls();
	UnreachableIsReportedAsSuch();
	NearestByStepsNotByStraightLine();
	ReachableSortsAheadOfUnreachable();
	OutOfViewObjectsAreKept();
	AbsenceOutsideTheMapIsNotForgetting();
	AbsenceBeyondTheScanIsNotForgetting();
	RecreatedObjectTakesItsOldPlace();
	EnemiesFollowTheirUnitAndDie();
	ChangingLevelClears();
	ReobservingRefreshesPosition();
	KindFiltering();
	CategoryCycling();
	ExplorationDiscoversOnlyNearbyVisibleTargets();
	KnownLandmarkSurvivesLossOfSight();
	ExplorationSurveyUsesOrientation();
	ReturningToLevelRetainsFixedLandmarks();
	FullAssistanceKeepsEveryLevel();
	ScannerSplitsLandmarksFromLiveEnemies();
	ChartedRoomsAdmitLandmarksOutOfSight();
	FixedPlacesAreTheLayoutsOwn();
	SpreadTakesOneWayAtATime();
	Passed();
	return 0;
}
