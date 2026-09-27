#include "retreat.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace D2RAccess;
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
double Distance(Position a, Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
double Nearest(const Retreat::Plan& plan, Position point) {
	double nearest = 1e9;
	for (const auto cell : plan.path) nearest = std::min(nearest, Distance(cell, point));
	return nearest;
}
// Every route is a chain of single steps over open cells.
void CheckWalkable(const World& world, const Retreat::Plan& plan) {
	Position previous = world.player;
	bool steps = true, open = true;
	for (const auto cell : plan.path) {
		steps &= std::max(std::abs(cell.x - previous.x), std::abs(cell.y - previous.y)) == 1;
		open &= world.Cell(static_cast<int>(cell.x), static_cast<int>(cell.y), Layer::Terrain) == Radar::State::Open;
		previous = cell;
	}
	Check(steps, "the retreat is a chain of single steps from the player's cell");
	Check(open, "the retreat never crosses a wall");
}
}

int main() {
	const Retreat::Settings settings;
	{
		// Open floor, one way out: a straight walk that stops beside it.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto plan = Retreat::Find(grid, {{90, 60}}, {}, settings);
		Check(plan.found, "a way out on open floor is found");
		Check(!plan.threatened, "no monster, no threat");
		CheckWalkable(world, plan);
		Check(Distance(plan.path.back(), {90, 60}) <= settings.arrival, "the retreat ends beside the way out");
		Check(plan.length <= 30, "open floor is walked straight");
	}
	{
		// Two ways out: the nearer one is taken while nothing stands in the way.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto plan = Retreat::Find(grid, {{20, 60}, {80, 60}}, {}, settings);
		Check(plan.found && plan.goal == 1, "the nearer way out wins on open floor");
	}
	{
		// A pack standing on the way to the nearer exit makes the farther one cheaper.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		std::vector<Position> pack;
		for (int y = 40; y <= 80; y += 4) pack.push_back({72.0, static_cast<double>(y)});
		const auto plan = Retreat::Find(grid, {{20, 60}, {80, 60}}, pack, settings);
		Check(plan.found && plan.goal == 0, "a pack across the near way out sends the retreat to the far one");
		Check(!plan.threatened, "the far way out is reached without passing a monster");
	}
	{
		// One monster on the straight line: the route bends round it and still arrives.
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const Position monster{75, 60};
		const auto plan = Retreat::Find(grid, {{90, 60}}, {monster}, settings);
		Check(plan.found, "a monster on the line does not stop the retreat");
		CheckWalkable(world, plan);
		Check(Nearest(plan, monster) > settings.danger.front().within, "the route keeps out of the monster's reach");
		Check(!plan.threatened, "a route round the monster is not threatened");
	}
	{
		// A corridor with a monster in it is the only way: taken, and said to be threatened.
		World world = Field({60, 60});
		Fill(world, 70, 0, 72, Size - 1, Wall);
		Fill(world, 70, 59, 72, 61, 0); // a doorway three cells wide
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto plan = Retreat::Find(grid, {{90, 60}}, {{71, 60}}, settings);
		Check(plan.found, "the only way out is taken even past a monster");
		Check(plan.threatened, "a route within a blow of a monster is threatened");
		CheckWalkable(world, plan);
	}
	{
		// A way out whose own cell is wall is reached from beside it.
		World world = Field({60, 60});
		Fill(world, 90, 58, 94, 62, Wall);
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto plan = Retreat::Find(grid, {{91, 60}}, {}, settings);
		Check(plan.found, "a way out standing in wall is reached from the floor beside it");
		CheckWalkable(world, plan);
	}
	{
		// Walled off entirely: no plan, rather than a walk toward it.
		World world = Field({60, 60});
		Fill(world, 70, 0, 72, Size - 1, Wall);
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		const auto plan = Retreat::Find(grid, {{90, 60}}, {}, settings);
		Check(!plan.found, "a way out behind a closed wall has no plan");
		Check(plan.path.empty(), "no plan, no path");
	}
	{
		const World world = Field({60, 60});
		const auto grid = Navigation::Build(world, Radius, Layer::Terrain);
		Check(!Retreat::Find(grid, {}, {}, settings).found, "no way out, no plan");
		Check(!Retreat::Find(Navigation::Grid{}, {{90, 60}}, {}, settings).found, "an unread world has no plan");
	}
	Passed();
	return 0;
}
