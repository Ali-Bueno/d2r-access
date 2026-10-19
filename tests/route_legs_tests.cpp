#include "route_legs.h"
#include "test_support.h"

#include <cmath>
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

void OpenGround() {
	// The search's own way across open floor: all its diagonal steps first, then straight on.
	Walk walk;
	walk.Go(North, 10).Go(NorthWest, 10);
	const auto legs = Legs(walk, Everywhere());
	Check(legs.size() == 1, "across open floor the zigzag of the search is one straight leg");
	Check(std::abs(legs[0].length - std::hypot(20.0, 10.0)) < 1e-6, "as long as the straight line");
	const auto walled = Legs(walk, walk.Corridor());
	Check(walled.size() == 2, "the same cells between walls keep their corner");
}

void Doors() {
	Walk walk;
	walk.Go(North, 10).Go(North, 2).Go(North, 10);
	const auto legs = Legs(walk, walk.Corridor(), {10, 11});
	Check(legs.size() == 3 && !legs[0].door && legs[1].door && !legs[2].door, "a shut door on the way is a leg of its own");
	Check(RouteLegs::Speak("Cain", legs, false, English).find("north, 16; then door; then north, 16") != std::string::npos,
		"two cells of one door are said once");
}

void Jogs() {
	Walk walk;
	walk.Go(North, 20).Go(East, 1).Go(North, 20);
	const auto legs = Legs(walk, walk.Wide());
	Check(legs.size() == 1 && Bucket(legs[0]) == 0, "a one-cell jog in a corridor wider than the way is not a turn");
	Check(legs[0].delta.x == walk.at.x && legs[0].delta.y == walk.at.y, "the leg still ends where the way does");
	Walk first;
	first.Go(East, 1).Go(North, 20);
	const auto start = Legs(first, first.Wide());
	Check(start.size() == 1 && Bucket(start[0]) == 0, "nor is a jog at the very start");
	// A short first leg, cut off by one cell that walking straight past it misses.
	Walk around;
	around.Go(North, 2).Go(East, 12);
	const auto folded = Legs(around, [](int x, int y) { return !(x == -1 && y == -2); });
	Check(folded.size() == 1 && Bucket(folded[0]) == 2, "a short leg walked straight past over clear ground is no turn");
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
	Doorways();
	Wording();
	Passed();
	return 0;
}
