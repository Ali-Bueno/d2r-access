#include "auto_walk.h"
#include "beacon.h"
#include "retreat_collision.h"
#include "test_support.h"

#include <cmath>
#include <vector>

using namespace D2RAccess;
using AutoWalk::Event;
using Radar::Position;
using Radar::World;

namespace {
constexpr int Size = 120;
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr uint16_t Wall = 1;

World Field(Position player, int pattern = 0) {
	World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = pattern;
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
}

int main() {
	const AutoWalk::Settings settings;
	const Beacon::Settings routing;
    {
        // A monster arrives on an order already under way: change direction
        // at the order interval instead of waiting for the stall timeout.
        World world = Field({20, 60}, 1);
        Beacon::Settings retreatRoute;
        retreatRoute.layer = Radar::Layer::Movement;
        AutoWalk::Settings retreatWalk;
        retreatWalk.layer = Radar::Layer::Movement;
        AutoWalk::Walker walker;
        walker.Start();
        const Position target{80, 60};
        const auto first = walker.Tick(world, Beacon::FindRoute(world, target, retreatRoute).path, 1000, retreatWalk);
        Check(first.event == Event::Order, "retreat gives its first order immediately");
        Fill(world, 25, 60, 25, 60, 0x1100);
        Check(AutoWalk::Straight(world, world.player, first.order), "ordinary terrain routes ignore a transient monster");
        Check(!AutoWalk::Straight(world, world.player, first.order, Radar::Layer::Movement), "retreat sees the monster blocking its order");
        const auto route = Beacon::FindRoute(world, target, retreatRoute);
        Check(route.reachable, "retreat finds a detour around the monster");
        const auto changed = walker.Tick(world, route.path, 1300, retreatWalk);
        Check(changed.event == Event::Order && changed.order.y != first.order.y, "retreat changes the blocked order before the stall timeout");
        Check(AutoWalk::Straight(world, world.player, changed.order, Radar::Layer::Movement), "new retreat order fits beside the monster");
    }
    {
        World world = Field({20, 60}, 1);
        Fill(world, 18, 58, 22, 62, 0x1100);
        Beacon::Settings retreat;
        retreat.layer = Radar::Layer::Movement;
        const auto route = Beacon::FindRoute(world, {80, 60}, retreat);
        Check(!route.known && route.path.empty(), "a completely surrounded player is not promised an escape through monsters");
    }
    {
        World world;
        world.player = {22675.06561, 14313.21709};
        world.bodyPattern = 1;
        world.walkingMask = PlayerPath;
        Radar::RoomGrid room{22647, 14285, 57, 57, {}};
        for (const auto [flags, count] : RetreatCollision) room.cells.insert(room.cells.end(), count, flags);
        world.rooms.push_back(std::move(room));
        const Position entrance{22722, 14430};
        const auto terrain = Beacon::FindRoute(world, entrance, routing);
        Beacon::Settings retreatRoute;
        retreatRoute.layer = Radar::Layer::Movement;
        AutoWalk::Settings retreatWalk;
        retreatWalk.layer = Radar::Layer::Movement;
        const auto retreat = Beacon::FindRoute(world, entrance, retreatRoute);
        Check(terrain.known && retreat.known, "captured death encounter has a locally usable retreat route");
        for (const auto point : retreat.path)
            Check(world.Body(static_cast<int>(point.x), static_cast<int>(point.y), Radar::Layer::Movement) == Radar::State::Open,
                "captured retreat detour never walks through a monster or wall");
        const auto leg = AutoWalk::Leg(world, retreat.path, retreatWalk);
        Check(AutoWalk::Straight(world, world.player, leg, Radar::Layer::Movement), "captured retreat order follows the live open ground");
        Check(Distance(world.player, retreat.path.back()) > 10, "captured retreat gets beyond the immediate pack");
    }

	{
		const World world = Field({20, 20});
		Check(AutoWalk::Straight(world, {20, 20}, {40, 33}), "open floor is walked straight");
		World walled = Field({20, 20});
		Fill(walled, 30, 0, 30, Size - 1, Wall);
		Check(!AutoWalk::Straight(walled, {20, 20}, {40, 20}), "a wall across the line stops a straight walk");
		// Two walls meeting at a corner leave no way between them.
		World corner = Field({20, 20});
		Fill(corner, 21, 20, 21, 20, Wall);
		Fill(corner, 20, 21, 20, 21, Wall);
		Check(!AutoWalk::Straight(corner, {20, 20}, {21, 21}), "a diagonal between two wall corners is not a way through");
	}
	{
		// A long straight route: the order is as far as the reach allows, no farther.
		const World world = Field({20, 60});
		const auto route = Beacon::FindRoute(world, {100, 60}, routing);
		Check(route.reachable, "fixture: the far side is reachable");
		const auto leg = AutoWalk::Leg(world, route.path, settings);
		Check(Distance(world.player, leg) <= settings.reach, "an order is never given past the reach");
		Check(Distance(world.player, leg) >= settings.reach - 2, "on open floor the order is as far as the reach allows");
	}
	{
		// Round a wall: the order stops at the corner, where the straight walk ends.
		World world = Field({50, 60});
		Fill(world, 60, 40, 60, 80, Wall);
		const auto route = Beacon::FindRoute(world, {70, 60}, routing);
		Check(route.reachable, "fixture: the far side of the wall is reachable");
		const auto leg = AutoWalk::Leg(world, route.path, settings);
		Check(AutoWalk::Straight(world, world.player, leg), "the order is a point the body walks straight to");
		Check(leg.x < 60, "the order does not lie beyond the wall");
	}
	{
		// The walker: one order, silence while it is walked, the next as it is neared.
		World world = Field({20, 60});
		const Position target{100, 60};
		AutoWalk::Walker walker;
		Check(walker.Tick(world, Beacon::FindRoute(world, target, routing).path, 0, settings).event == Event::None,
			"a walker not started gives no order");
		walker.Start();
		const auto first = walker.Tick(world, Beacon::FindRoute(world, target, routing).path, 1000, settings);
		Check(first.event == Event::Order, "a started walker gives its first order at once");
		world.player = {25, 60};
		Check(walker.Tick(world, Beacon::FindRoute(world, target, routing).path, 1300, settings).event == Event::None,
			"no new order while the last one is still far ahead");
		world.player = {first.order.x - 3, 60};
		const auto second = walker.Tick(world, Beacon::FindRoute(world, target, routing).path, 1600, settings);
		Check(second.event == Event::Order, "the next order is given as the last one is neared");
		Check(second.order.x > first.order.x, "the next order lies farther along the route");
		walker.Stop();
		Check(!walker.Active(), "a stopped walker is not active");
	}
	{
		// Standing still: the order is given again, then the walk is blocked.
		const World world = Field({20, 60});
		const auto path = Beacon::FindRoute(world, {100, 60}, routing).path;
		AutoWalk::Walker walker;
		walker.Start();
		uint64_t now = 1000;
		Check(walker.Tick(world, path, now, settings).event == Event::Order, "fixture: first order");
		int orders = 0;
		Event last = Event::None;
		for (int i = 0; i < 100 && last != Event::Blocked; ++i) {
			now += 100;
			last = walker.Tick(world, path, now, settings).event;
			if (last == Event::Order) ++orders;
		}
		Check(last == Event::Blocked, "a walk that covers no ground ends blocked");
		Check(orders == settings.stallOrders - 1, "the order is given again before the walk gives up");
		Check(now - 1000 >= settings.stallMs * settings.stallOrders, "giving up takes the full patience");
	}
	{
		// Slow progress is progress: no block while ground is covered.
		World world = Field({20, 60});
		const Position target{100, 60};
		AutoWalk::Walker walker;
		walker.Start();
		bool blocked = false;
		for (int i = 0; i < 60; ++i) {
			world.player.x = 20 + i;
			const auto step = walker.Tick(world, Beacon::FindRoute(world, target, routing).path, 1000 + i * 1000ULL, settings);
			blocked |= step.event == Event::Blocked;
		}
		Check(!blocked, "a slow walk that keeps covering ground is never blocked");
	}
	Passed();
	return 0;
}
