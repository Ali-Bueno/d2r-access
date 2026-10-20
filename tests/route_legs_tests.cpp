#include "route_legs.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace D2RAccess;
using RouteLegs::Leg;
using RouteLegs::Position;
namespace {
// World steps of the ways the stick walks: north (screen up) is the world's -X,-Y
// diagonal, and the stick's diagonals are the world axes.
constexpr Position North{-1, -1}, East{1, -1}, South{1, 1}, NorthWest{-1, 0}, NorthEast{0, -1}, SouthEast{1, 0};
constexpr auto Compass = SpokenDirection::Style::Compass, Clock = SpokenDirection::Style::Clock;
const auto& English = Strings::For(Strings::Language::enUS);

struct Walk {
	std::vector<Position> path;
	std::set<std::pair<int, int>> cells{{0, 0}};
	Position at{};
	Walk& Go(Position step, int count) {
		for (int i = 0; i < count; ++i) {
			at = {at.x + step.x, at.y + step.y};
			path.push_back(at);
			cells.insert({static_cast<int>(at.x), static_cast<int>(at.y)});
		}
		return *this;
	}
	// Only the way's own cells are floor: a corridor exactly as wide as the way.
	RouteLegs::Passable Corridor() const { return [cells = cells](int x, int y) { return cells.contains({x, y}); }; }
	// A corridor one cell wider than the way on every side.
	RouteLegs::Passable Wide() const {
		return [cells = cells](int x, int y) {
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx)
					if (cells.contains({x + dx, y + dy})) return true;
			return false;
		};
	}
};
RouteLegs::Passable Everywhere() { return [](int, int) { return true; }; }
std::vector<Leg> Legs(const Walk& walk, const RouteLegs::Passable& passable, std::vector<size_t> doors = {},
	SpokenDirection::Style style = SpokenDirection::Style::Compass) {
	return RouteLegs::Describe({0, 0}, walk.path, doors, passable, style);
}
int Bucket(const Leg& leg) { return SpokenDirection::Bucket(leg.delta, Compass); }

void Corners() {
	Walk walk;
	walk.Go(North, 20).Go(East, 15);
	const auto legs = Legs(walk, walk.Corridor());
	Check(legs.size() == 2, "a corridor that turns once is two legs");
	Check(Bucket(legs[0]) == 0 && Bucket(legs[1]) == 2, "north, then east");
	Check(std::abs(legs[0].length - 20 * std::sqrt(2.0)) < 1e-6, "a leg's length is the straight line it covers, in units");
	Check(RouteLegs::Speak("Akara", legs, false, English) == "Route to Akara, 49 units: north, 28; then east, 21",
		"the route says its whole length, then each leg");
}

// Whether `legs` walk from `from` one cell at a time over `passable` ground, each
// one of the eight ways, a diagonal step only with the body fitting on both
// cells beside it.
bool Walkable(Position from, const std::vector<Leg>& legs, const RouteLegs::Passable& passable) {
	int x = static_cast<int>(from.x), y = static_cast<int>(from.y);
	for (const auto& leg : legs) {
		if (leg.door) continue;
		const int dx = static_cast<int>(leg.delta.x), dy = static_cast<int>(leg.delta.y);
		if (dx && dy && std::abs(dx) != std::abs(dy)) return false;
		const int steps = std::max(std::abs(dx), std::abs(dy)), sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
		for (int i = 0; i < steps; ++i) {
			if (!passable(x + sx, y + sy) || (sx && sy && (!passable(x + sx, y) || !passable(x, y + sy)))) return false;
			x += sx; y += sy;
		}
	}
	return true;
}

void OpenGround() {
	// The search's own way across open floor: all its diagonal steps first, then straight on.
	Walk walk;
	walk.Go(North, 10).Go(NorthWest, 10);
	const auto legs = Legs(walk, Everywhere());
	Check(legs.size() == 2 && Walkable({0, 0}, legs, Everywhere()), "across open floor a way is legs the stick walks");
	Check(std::abs(RouteLegs::Length(legs) - (10 * std::sqrt(2.0) + 10)) < 1e-6, "as long as the way");
	Walk zigzag;
	for (int i = 0; i < 10; ++i) zigzag.Go(North, 1).Go(NorthWest, 1);
	const auto straight = Legs(zigzag, Everywhere());
	Check(straight.size() == 2 && Walkable({0, 0}, straight, Everywhere()), "a zigzag of the search is two legs, not twenty");
	const auto walled = Legs(walk, walk.Corridor());
	Check(walled.size() == 2, "the same cells between walls keep their corner");
	// A way some hundreds of cells long is laid out a stretch at a time.
	Walk far;
	for (int i = 0; i < 40; ++i) far.Go(North, 3).Go(NorthWest, 2).Go(NorthEast, 2);
	const RouteLegs::End there{static_cast<int>(far.at.x) - 3, static_cast<int>(far.at.y) - 3,
		static_cast<int>(far.at.x) + 3, static_cast<int>(far.at.y) + 3};
	const auto across = RouteLegs::Describe({0, 0}, far.path, {}, Everywhere(), Compass, &there);
	Position end{};
	for (const auto& leg : across) end = {end.x + leg.delta.x, end.y + leg.delta.y};
	Check(across.size() <= 2 && Walkable({0, 0}, across, Everywhere()), "a long staircase over open floor is a leg or two");
	Check(end.x >= there.minX && end.x <= there.maxX && end.y >= there.minY && end.y <= there.maxY, "and comes to the end");
}

void Doors() {
	Walk walk;
	walk.Go(North, 10).Go(North, 2).Go(North, 10);
	const auto legs = Legs(walk, walk.Corridor(), {10, 11});
	Check(legs.size() == 3 && !legs[0].door && legs[1].door && !legs[2].door, "a shut door on the way is a leg of its own");
	Check(RouteLegs::Speak("Cain", legs, false, English).find("north, 14; then door; then north, 17") != std::string::npos,
		"a door is opened from the cell before it, and its two cells are said once");
}

void Jogs() {
	// A corridor along a world axis, one cell wider than the way on each side.
	Walk walk;
	walk.Go(NorthEast, 20).Go(SouthEast, 1).Go(NorthEast, 20);
	const RouteLegs::End beside{0, -41, 2, -39};
	const auto legs = RouteLegs::Describe({0, 0}, walk.path, {}, walk.Wide(), Compass, &beside);
	Check(legs.size() == 1 && Bucket(legs[0]) == 1, "a jog the corridor leaves room to walk past is not a turn");
	const auto exact = Legs(walk, walk.Wide());
	Check(RouteLegs::Length(exact) > 40 && Walkable({0, 0}, exact, walk.Wide()), "to the way's very cell the legs still walk");
	Position end{};
	for (const auto& leg : exact) end = {end.x + leg.delta.x, end.y + leg.delta.y};
	Check(end.x == walk.at.x && end.y == walk.at.y, "and end where the way does");
	Walk first;
	first.Go(SouthEast, 1).Go(NorthEast, 20);
	const RouteLegs::End ahead{0, -21, 2, -19};
	const auto start = RouteLegs::Describe({0, 0}, first.path, {}, first.Wide(), Compass, &ahead);
	Check(start.size() == 1 && Bucket(start[0]) == 1, "nor is a jog at the very start");
}

// A wall along the world's X axis at y = -5, with a doorway the body just fits through.
bool Floor(int x, int y) { return y != -5 || std::abs(x) <= 1; }
bool Body(int x, int y) { return Floor(x, y) && Floor(x - 1, y) && Floor(x + 1, y) && Floor(x, y - 1) && Floor(x, y + 1); }

void Wall() {
	// The search's own way cuts through the doorway on a diagonal.
	Walk walk;
	walk.at = {6, 0};
	walk.Go(North, 3).Go(NorthWest, 2).Go(NorthEast, 1).Go(North, 1).Go(North, 1).Go(North, 6).Go(NorthWest, 1);
	const auto legs = RouteLegs::Describe({6, 0}, walk.path, {}, Body, Compass);
	Check(Walkable({6, 0}, legs, Body), "every leg walks clear of the wall, the body and all");
	Position at{6, 0};
	bool through = false;
	for (const auto& leg : legs) {
		const Position next{at.x + leg.delta.x, at.y + leg.delta.y};
		if (at.y > -5 && next.y < -5) through = leg.delta.x == 0 && Bucket(leg) == 1;
		at = next;
	}
	Check(through, "the doorway is walked through along the world axis: northeast, as the stick's diagonal");
	Check(at.x == walk.at.x && at.y == walk.at.y, "to where the way ends");
}

void Doorways() {
	// Along a wall, through a doorway as wide as the way, and on along the wall.
	Walk walk;
	walk.Go(NorthEast, 10).Go(SouthEast, 2).Go(NorthEast, 10);
	const auto legs = Legs(walk, walk.Corridor());
	Check(legs.size() == 3, "the step through a doorway is not folded into the walk along the wall");
	Check(RouteLegs::Speak("Kashya", legs, false, English)
		== "Route to Kashya, 22 units: northeast, 10; then southeast, 2; then northeast, 10",
		"a doorway is crossed along a world axis, said as a diagonal");
	// A little off a world axis, as a leg straightened through a doorway is.
	const Position doorway{12, -3};
	Check(Radar::ScreenSector(doorway) == 2, "on the screen's 2:1 projection it is east");
	Check(SpokenDirection::Bucket(doorway, Compass) == 3, "the stick walks it as southeast, and so it is said");
	Check(SpokenDirection::Bucket(North, Compass) == 0 && SpokenDirection::Bucket(East, Compass) == 2,
		"the stick's cardinals are the screen's");
	Check(SpokenDirection::Bucket(doorway, Clock) == 4, "the clock keeps the screen's hours: an axis at 4 o'clock");
}

void Wording() {
	Walk walk;
	walk.Go(North, 20).Go(East, 15);
	SpokenDirection::SetStyle(SpokenDirection::Style::Clock);
	const auto legs = Legs(walk, walk.Corridor(), {}, SpokenDirection::Style::Clock);
	Check(RouteLegs::Speak("Akara", legs, false, English) == "Route to Akara, 49 units: 12 o'clock, 28; then 3 o'clock, 21",
		"the clock style names legs by their hours");
	SpokenDirection::SetStyle(SpokenDirection::Style::Compass);
	Check(RouteLegs::Speak("Akara", legs, true, English).ends_with(". Part of it crosses ground not seen yet"),
		"a way over ground not seen yet says so");
	Check(RouteLegs::Speak("Akara", legs, false, Strings::For(Strings::Language::esES))
		== "Ruta hasta Akara, 49 unidades: norte, 28; luego este, 21", "Spanish wording");
	Walk winding;
	for (int i = 0; i < 4; ++i) winding.Go(North, 5).Go(East, 5);
	const auto many = Legs(winding, winding.Corridor());
	Check(many.size() == 8, "a winding corridor keeps every turn");
	Check(RouteLegs::Speak("Rakanishu", many, false, English).ends_with("; then north, 7; then east, 7; then 2 more legs"),
		"past six legs the rest is counted");
	Check(RouteLegs::Describe({0, 0}, {}, {}, Everywhere(), SpokenDirection::Style::Compass).empty(), "no way, no legs");
	Walk back;
	back.Go(South, 4);
	Check(RouteLegs::Length(Legs(back, back.Corridor())) > 5, "the length adds up the legs");
}
}

int main() {
	Corners();
	OpenGround();
	Doors();
	Jogs();
	Wall();
	Doorways();
	Wording();
	Passed();
	return 0;
}
