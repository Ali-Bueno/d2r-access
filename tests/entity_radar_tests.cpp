#include "entity_radar.h"
#include "speech_strings.h"
#include "test_support.h"

#include <string>

using namespace D2RAccess;
using namespace EntityRadar;
namespace {
constexpr uint16_t Wall = 0x1; // BLOCK_WALK, the walking mask used by the sonar tests
constexpr int Size = 40;
// One open room with the player in the middle. Screen up is world (-X,-Y).
Radar::World OpenWorld() {
	Radar::World world;
	world.player = {20, 20};
	world.walkingMask = Wall;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size, 0)});
	return world;
}
void Block(Radar::World& world, int x, int y) { world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Wall; }
Entity Make(uint32_t id, double x, double y, std::string name = "Poción") {
	Entity entity; entity.id = id; entity.position = {x, y}; entity.name = std::move(name); return entity;
}

void Visibility() {
	auto world = OpenWorld();
	Check(LineOfSight(world, world.player, {10, 10}), "an open room hides nothing");
	for (int x = 0; x < Size; ++x) Block(world, x, 15);
	Check(!LineOfSight(world, world.player, {10, 10}), "a wall across the ray hides the target");
	Check(LineOfSight(world, world.player, {30, 30}), "the wall does not affect the other side");
	world.rooms[0].cells[15 * Size + 15] = 0;
	Check(LineOfSight(world, world.player, {10, 10}), "a doorway on the ray restores visibility");
	auto own = OpenWorld();
	Block(own, 10, 10);
	Check(LineOfSight(own, own.player, {10, 10}), "the target's own collision cell never hides it");
	Block(own, 20, 20);
	Check(LineOfSight(own, own.player, {10, 10}), "nor does the player's own cell");
	auto actors = OpenWorld();
	constexpr uint16_t Monster = 0x100;
	actors.walkingMask = Wall | Monster;
	actors.rooms[0].cells[15 * Size + 15] = Monster;
	Check(LineOfSight(actors, actors.player, {10, 10}), "another actor on the ray is not a wall");
}

void ObjectFootprints() {
	constexpr uint16_t Object = Radar::Collision::Object;
	constexpr uint16_t Door = Radar::Collision::Door;
	constexpr uint16_t NoPath = 0x1000;
	for (const auto [x, y] : {std::pair{16, 16}, std::pair{16, 24}, std::pair{24, 16}, std::pair{24, 24}}) {
		auto world = OpenWorld();
		world.walkingMask = Wall | Object | Door;
		auto stone = Make(69, x, y, "Piedras de Cairn, gota hacia abajo");
		stone.kind = Kind::Object;
		stone.footprint = {3, 3};
		const int nearX = x + (x < 20 ? 1 : -1), nearY = y + (y < 20 ? 1 : -1);
		world.rooms[0].cells[static_cast<size_t>(nearY * Size + nearX)] = Object;
		world.rooms[0].cells[static_cast<size_t>(y * Size + x)] = Object;
		Check(!LineOfSight(world, world.player, stone.position), "point LOS still sees the obstacle in the target's footprint");
		Check(LineOfSight(world, world.player, stone), "an object's own collision does not hide its centre");
		stone.footprint = {};
		Check(!LineOfSight(world, world.player, stone), "an entity with no footprint retains point LOS");
	}
	auto world = OpenWorld();
	world.walkingMask = Wall | Object | Door | NoPath;
	auto stone = Make(69, 16, 16, "Piedras de Cairn");
	stone.kind = Kind::Object;
	stone.footprint = {3, 3};
	world.rooms[0].cells[18 * Size + 18] = Object;
	Check(!LineOfSight(world, world.player, stone), "another object's collision outside the footprint still blocks sight");
	world.rooms[0].cells[18 * Size + 18] = Wall;
	Check(!LineOfSight(world, world.player, stone), "an external wall still blocks sight");
	world.rooms[0].cells[18 * Size + 18] = 0;
	world.rooms[0].cells[17 * Size + 17] = Object | Wall;
	Check(!LineOfSight(world, world.player, stone), "a wall overlapping the footprint still blocks sight");
	world.rooms[0].cells[17 * Size + 17] = Object | Door;
	Check(!LineOfSight(world, world.player, stone), "a door overlapping the footprint still blocks sight");
	world.rooms[0].cells[17 * Size + 17] = Object | NoPath;
	Check(!LineOfSight(world, world.player, stone), "a no-path cell overlapping the footprint still blocks sight");
	world.rooms[0].cells[17 * Size + 17] = Object;
	world.rooms[0].cells[16 * Size + 16] = Object | Wall;
	Check(!LineOfSight(world, world.player, stone), "a wall in the target centre is never bypassed");
	auto door = stone;
	door.footprint = {};
	world.rooms[0].cells[17 * Size + 17] = 0;
	world.rooms[0].cells[16 * Size + 16] = Door;
	Check(LineOfSight(world, world.player, door), "a door without a footprint keeps legacy target-centre visibility");
	auto even = Make(70, 16, 16, "Even footprint");
	even.kind = Kind::Object;
	even.footprint = {4, 4};
	auto fromLeft = OpenWorld();
	fromLeft.walkingMask = Wall | Object;
	fromLeft.player = {10, 10};
	fromLeft.rooms[0].cells[14 * Size + 14] = Object;
	Check(LineOfSight(fromLeft, fromLeft.player, even), "an even footprint includes centre minus two on both axes");
	fromLeft.rooms[0].cells[13 * Size + 13] = Object;
	Check(!LineOfSight(fromLeft, fromLeft.player, even), "the cell before an even footprint remains blocking");
	auto fromRight = OpenWorld();
	fromRight.walkingMask = Wall | Object;
	fromRight.rooms[0].cells[17 * Size + 17] = Object;
	Check(LineOfSight(fromRight, fromRight.player, even), "an even footprint includes centre plus one on both axes");
	fromRight.rooms[0].cells[18 * Size + 18] = Object;
	Check(!LineOfSight(fromRight, fromRight.player, even), "centre plus two lies outside an even footprint");
}

void LevelExitIdentity() {
	Entity stairs{42, Kind::Exit, {28, 28}, "A las Catacumbas", 1234};
	Entity enemy{42, Kind::Enemy, {28, 28}, "Zombi"};
	auto respawned = stairs; respawned.id = 900;
	Check(SameTarget(stairs, respawned), "a level exit survives unit recreation");
	Check(!SameTarget(stairs, enemy), "a native enemy id cannot alias a level exit");
	auto second = stairs; second.placeId = 4567;
	Check(!SameTarget(stairs, second), "separate exits with the same warp class keep distinct identities");
}

void CorpseIdentity() {
	Entity corpse{50, Kind::Corpse, {17, 17}, "Cadáver"};
	Entity casket{50, Kind::Object, {14, 14}, "Ataúd"};
	Check(!SameTarget(corpse, casket), "beacon matching cannot follow the same-ID world object in place of the corpse");
}

void TowerQuest() {
	Check(EntityRadar::QuestWay(6, 20) && EntityRadar::QuestWay(20, 21) && !EntityRadar::QuestWay(21, 20),
		"the Black Marsh's door and every step down the tower are quest ways, the way back up is not");
	Check(!EntityRadar::QuestRunning(0), "before the Moldy Tome the tower is no quest");
	Check(EntityRadar::QuestRunning(0x0004) && EntityRadar::QuestRunning(0x0048), "from the tome to the Countess it is");
	Check(!EntityRadar::QuestRunning(0x2001) && !EntityRadar::QuestRunning(0x8010), "completed, or done, it is no more");
	Check(!EntityRadar::QuestRunning(0x1000), "a flag of its own outside the steps is not a start");
}

void QuestSources() {
	Entity tree{7, Kind::Object, {20, 20}, "Árbol de Inifuss"};
	Check(!QuestSource(tree), "an object is not a quest source by its name");
	tree.questObject = true;
	Check(QuestSource(tree), "an unused quest object is a quest source");
	Entity tome{8, Kind::Object, {21, 21}, "Tomo mohoso"};
	tome.unreadTome = true;
	Check(QuestSource(tome), "an unread quest book is a quest source");
	Entity scroll{9, Kind::Item, {22, 22}, "Pergamino de Inifuss"};
	scroll.questObject = true;
	Check(!QuestSource(scroll), "only world objects count; ground quest items have their own reader");
}

// D at the Ancients' altar: the statues stand nearer, and the altar is what the quest needs.
void UsePreference() {
	Entity altar{1, Kind::Object, {20, 20}, "Altar of the Heavens"};
	altar.questObject = true;
	Entity statue{2, Kind::Object, {18, 18}, "Talic the Defender"};
	Entity npc{3, Kind::Npc, {17, 17}, "Qual-Kehk"};
	Check(UseBefore(altar, 9, statue, 3) && !UseBefore(statue, 3, altar, 9), "an unused quest object comes before a nearer statue");
	Check(UseBefore(altar, 9, npc, 1), "and before a nearer NPC");
	Check(UseBefore(statue, 3, npc, 5) && !UseBefore(npc, 5, statue, 3), "other things go by steps");
	Entity seal{4, Kind::Object, {22, 22}, "Seal 2"};
	seal.questObject = true;
	Check(UseBefore(seal, 4, altar, 9) && !UseBefore(altar, 9, seal, 4), "between quest objects the nearer comes first");
	altar.questObject = false;
	Check(UseBefore(statue, 3, altar, 9), "a used one is ordinary again");
}

void Wording() {
	const auto& es = Strings::For(Strings::Language::esES);
	Settings settings;
	Check(DistancePhrase(8.2, settings, es) == "8 unidades", "distance in units");
	Check(DistancePhrase(1.2, settings, es) == "aquí mismo", "a thing beside the body is under the player");
	Check(DistancePhrase(1.4, settings, es) == "aquí mismo", "diagonally beside too");
	Check(DistancePhrase(0.4, settings, es) == "aquí mismo", "adjacent targets have no distance");
	settings.unitsPerSubtile = 0.5;
	Check(DistancePhrase(8.2, settings, es) == "4 unidades", "the spoken unit follows the calibration factor");
	Check(DistancePhrase(2.2, settings, es) == "1 unidad", "the singular unit");
	Check(DistancePhrase(8.2, settings, Strings::For(Strings::Language::enUS)) == "4 units", "english wording");
}
}

int main() {
	Visibility();
	ObjectFootprints();
	LevelExitIdentity();
	CorpseIdentity();
	QuestSources();
	TowerQuest();
	UsePreference();
	Wording();
	Passed();
	return 0;
}
