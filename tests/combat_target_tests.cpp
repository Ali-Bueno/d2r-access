#include "combat_target.h"
#include "test_support.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

using namespace D2RAccess;
using CombatTarget::Move;
using EntityRadar::Entity;
using EntityRadar::Kind;
using Radar::Layer;
using Radar::Position;
using Radar::World;

namespace {
constexpr int Size = 120;
constexpr int Radius = 50;
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr uint16_t Wall = 1;

World Field(Position player) {
	World world;
	world.session = world.level = 1;
	world.player = player;
	world.walkingMask = PlayerPath;
	world.rooms = {{0, 0, Size, Size, std::vector<uint16_t>(static_cast<size_t>(Size) * Size, 0)}};
	return world;
}
void Fill(World& world, int x0, int y0, int x1, int y1, uint16_t flags) { // inclusive
	auto& room = world.rooms.front();
	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x) room.cells[static_cast<size_t>(y) * room.width + x] = flags;
}
Entity Monster(uint32_t id, Position position, const char* name = "Fallen") {
	Entity entity;
	entity.id = id;
	entity.kind = Kind::Enemy;
	entity.category = EntityRadar::Category::Enemy;
	entity.position = position;
	entity.name = name;
	return entity;
}
}

int main() {
	{
		// Open floor: nearest first, and only enemies.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		Entity npc = Monster(9, {61, 60}, "Akara");
		npc.kind = Kind::Npc;
		const auto ranked = CombatTarget::Rank(world, grid, {Monster(1, {80, 60}), Monster(2, {65, 60}), npc, Monster(3, {60, 72})});
		Check(ranked.size() == 3, "only enemies are ranked");
		Check(ranked[0].entity.id == 2 && ranked[1].entity.id == 3 && ranked[2].entity.id == 1, "the nearest comes first");
		Check(ranked[0].sight && ranked[0].steps >= 0, "a monster on open floor is in sight and can be walked to");
	}
	{
		// A monster close by but behind a long wall counts as far as the way round.
		World world = Field({60, 60});
		Fill(world, 64, 30, 64, 90, Wall);
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto ranked = CombatTarget::Rank(world, grid, {Monster(1, {68, 60}), Monster(2, {45, 60})});
		Check(ranked.size() == 2 && ranked[0].entity.id == 2, "the way round a wall counts, not the straight line");
		Check(!ranked[1].sight, "a monster behind a wall is out of sight");
		Check(ranked[1].steps > 30, "its steps are the way round");
	}
	{
		// Walled off entirely: listed last, with no steps.
		World world = Field({60, 60});
		Fill(world, 64, 0, 64, Size - 1, Wall);
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto ranked = CombatTarget::Rank(world, grid, {Monster(1, {66, 60}), Monster(2, {30, 60})});
		Check(ranked.size() == 2 && ranked[0].entity.id == 2, "a monster with no way to it comes after one with a way");
		Check(ranked[1].steps < 0, "no way, no steps");
		Check(CombatTarget::Decide(ranked[1], {false, 3}) == Move::Unreachable, "melee cannot reach a monster with no way to it");
	}
	{
		// Out of range: not offered at all.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		CombatTarget::Settings settings;
		settings.range = 20;
		Check(CombatTarget::Rank(world, grid, {Monster(1, {100, 60})}, settings).empty(), "a monster past the range is not offered");
	}
	{
		// The player's own choice steps through them and wraps.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto ranked = CombatTarget::Rank(world, grid, {Monster(1, {65, 60}), Monster(2, {70, 60}), Monster(3, {75, 60})});
		CombatTarget::Lock lock;
		Check(!lock.Held(), "nothing is held at first");
		Check(lock.Choose(ranked, 1)->entity.id == 1, "the first step with nothing held takes the nearest");
		Check(lock.Choose(ranked, 1)->entity.id == 2, "the next step takes the next farther");
		Check(lock.Choose(ranked, 1)->entity.id == 3, "and the next");
		Check(lock.Choose(ranked, 1)->entity.id == 1, "past the farthest it wraps to the nearest");
		Check(lock.Choose(ranked, -1)->entity.id == 3, "a step back from the nearest wraps to the farthest");
		Check(lock.Choose(ranked, 0)->entity.id == 1, "asking for the nearest takes it whatever is held");
		Check(lock.Choose({}, 1) == nullptr && !lock.Held(), "with no monster there is nothing to choose or hold");
	}
	{
		// A fight stays on its monster, and moves on only when that one is gone.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Entity> all{Monster(1, {65, 60}), Monster(2, {70, 60})};
		CombatTarget::Lock lock;
		bool moved = false;
		Check(lock.Keep(CombatTarget::Rank(world, grid, all), moved)->entity.id == 1 && moved, "a fight begins on the nearest, and names it");
		// The held monster backs away until the other is nearer: the fight stays on it.
		const std::vector<Entity> swapped{Monster(1, {80, 60}), Monster(2, {70, 60})};
		Check(lock.Keep(CombatTarget::Rank(world, grid, swapped), moved)->entity.id == 1 && !moved, "the fight stays on its monster when another comes nearer");
		// It dies: the fight moves to the one left.
		Check(lock.Keep(CombatTarget::Rank(world, grid, {Monster(2, {70, 60})}), moved)->entity.id == 2 && moved, "the fight moves on when its monster is gone");
		Check(lock.Keep({}, moved) == nullptr && !lock.Held(), "the last monster gone, nothing is held");
	}
	{
		// Rangefinding.
		World world = Field({60, 60});
		Fill(world, 70, 50, 70, 58, Wall);
		Fill(world, 70, 62, 70, 70, Wall); // a wall with a gap at y 59..61
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto ranked = CombatTarget::Rank(world, grid, {Monster(1, {78, 60}), Monster(2, {78, 66})});
		const CombatTarget::Reach bow{true, 25}, sword{false, 3};
		const auto& open = ranked[0].entity.id == 1 ? ranked[0] : ranked[1];
		const auto& hidden = ranked[0].entity.id == 2 ? ranked[0] : ranked[1];
		Check(open.sight && !hidden.sight, "fixture: one monster seen through the gap, one behind the wall");
		Check(CombatTarget::Decide(open, bow) == Move::Strike, "a ranged attack strikes from where it stands when the monster is in reach and in sight");
		Check(CombatTarget::Decide(hidden, bow) == Move::Close, "a ranged attack closes on a monster in reach but out of sight");
		Check(CombatTarget::Decide(open, {true, 10}) == Move::Close, "a ranged attack closes on a monster past its reach");
		Check(CombatTarget::Decide(open, sword) == Move::Close, "a melee attack closes on a monster out of arm's reach");
		const auto near = CombatTarget::Rank(world, grid, {Monster(3, {62, 60})});
		Check(CombatTarget::Decide(near[0], sword) == Move::Strike, "a melee attack strikes a monster beside the player");
	}
	{
		// A monster has closed on open floor: there is ground to fall back to.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{66, 60}};
		const CombatTarget::Spacing spacing;
		Check(CombatTarget::Closing(world.player, pack), "a monster six steps off is closing");
		Check(!CombatTarget::Closing(world.player, {{80, 60}}), "a monster twenty steps off is not");
		Check(!CombatTarget::Closing(world.player, {}), "with no monster nothing is closing");
		const auto ground = CombatTarget::FallBack(world, grid, pack, pack[0], 24);
		Check(ground.found, "open floor has ground to fall back to");
		Check(ground.cell.x < 60, "the ground lies away from the monster");
		Check(ground.clearance >= 6 + spacing.gain, "it is farther from the monster than the player stood");
		Check(std::hypot(ground.cell.x - 66, ground.cell.y - 60) <= 24, "and still within range of it");
		Check(EntityRadar::LineOfSight(world, ground.cell, pack[0]), "with a clear view of it");
	}
	{
		// Monsters on two sides: the ground lies between them, to the side.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{66, 60}, {54, 60}};
		const auto ground = CombatTarget::FallBack(world, grid, pack, pack[0], 24);
		Check(ground.found, "with monsters east and west there is ground north or south");
		Check(std::abs(ground.cell.y - 60) > std::abs(ground.cell.x - 60), "the ground lies to the side, not toward either");
		Check(ground.clearance >= 6 + CombatTarget::Spacing{}.gain, "it is farther from both");
	}
	{
		// Cornered: the back of a dead end, the monster in its mouth.
		World world = Field({57, 60});
		Fill(world, 54, 56, 64, 64, Wall);
		Fill(world, 56, 58, 64, 62, 0); // a pocket open to the east
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{63, 60}};
		Check(!CombatTarget::FallBack(world, grid, pack, pack[0], 24).found, "at the back of a dead end there is nowhere to fall back to");
	}
	{
		// A corridor closed behind the player: the only ground farther from the
		// monster lies past it, and that is not falling back.
		World world = Field({60, 60});
		Fill(world, 0, 0, Size - 1, Size - 1, Wall);
		Fill(world, 58, 58, Size - 1, 62, 0);
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{64, 60}};
		Check(!CombatTarget::FallBack(world, grid, pack, pack[0], 24).found, "ground on the far side of the monster is not fallen back to");
	}
	{
		// The attack reaches less far than the distance to keep: stand and fight.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{66, 60}};
		Check(!CombatTarget::FallBack(world, grid, pack, pack[0], 8).found, "ground out of the attack's reach is not fallen back to");
	}
	{
		// Ground from which the monster cannot be seen is no place to shoot from.
		World world = Field({60, 60});
		Fill(world, 58, 40, 58, 80, Wall); // a wall just west of the player
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const std::vector<Position> pack{{66, 60}};
		const auto ground = CombatTarget::FallBack(world, grid, pack, pack[0], 24);
		Check(!ground.found || ground.cell.x > 58, "the far side of a wall is not fallen back to");
		Check(!ground.found || EntityRadar::LineOfSight(world, ground.cell, pack[0]), "whatever is chosen has a view of the monster");
	}
	{
		// Only what is on screen is fought: the view is wider than it is tall.
		const Position player{60, 60};
		Check(CombatTarget::OnScreen(player, {80, 60}), "a monster 20 to one side of the screen is on it");
		Check(CombatTarget::OnScreen(player, {76, 44}), "a monster 32 across the screen is on it");
		Check(!CombatTarget::OnScreen(player, {78, 42}), "a monster 36 across the screen is off it");
		Check(CombatTarget::OnScreen(player, {78, 78}), "a monster 36 down the screen is on it");
		Check(!CombatTarget::OnScreen(player, {80, 80}), "a monster 40 down the screen is off it");
		Check(!CombatTarget::OnScreen(player, {20, 20}), "a monster far up the screen is off it");
		Check(CombatTarget::NearestOnScreen(player, {{70, 60}, {62, 60}, {64, 60}}) == 1u, "the nearest corpse is taken, wherever it is listed");
		Check(CombatTarget::NearestOnScreen(player, {{20, 20}, {70, 60}}) == 1u, "a corpse off the screen is passed over");
		Check(!CombatTarget::NearestOnScreen(player, {{20, 20}}), "with every corpse off the screen there is none to take");
		Check(!CombatTarget::NearestOnScreen(player, {}), "no corpses, none to take");
	}
	Passed();
	return 0;
}
