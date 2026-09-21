#include "exit_review.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
using Radar::Position;

namespace {
const Strings::Table& English = Strings::For(Strings::Language::enUS);
const Strings::Table& Spanish = Strings::For(Strings::Language::esES);
const EntityRadar::Settings Scan{};

EntityRadar::Entity Exit(uint32_t id, Position position, const char* name) {
	return {id, EntityRadar::Kind::Exit, position, name};
}

// 12 is screen up, and the screen axes are the world diagonals (native
// projection 0x334DB0), so each screen cardinal is a world diagonal.
void Cardinals() {
	const Position player{100, 100};
	Check(ExitReview::ClockHour(player, {99, 99}) == 12, "up the screen is twelve o'clock");
	Check(ExitReview::ClockHour(player, {101, 99}) == 3, "right on the screen is three o'clock");
	Check(ExitReview::ClockHour(player, {101, 101}) == 6, "down the screen is six o'clock");
	Check(ExitReview::ClockHour(player, {99, 101}) == 9, "left on the screen is nine o'clock");
	Check(ExitReview::ClockHour(player, player) == 12, "the player's own cell reads as twelve, not as an error");
}

// The world axes fall between two hours; the rounding must stay symmetric
// about both twelve and six instead of drifting one way.
void Diagonals() {
	const Position player{100, 100};
	Check(ExitReview::ClockHour(player, {100, 99}) == 2, "world minus Y is between one and two, and rounds to two");
	Check(ExitReview::ClockHour(player, {101, 100}) == 5, "world plus X rounds to five");
	Check(ExitReview::ClockHour(player, {100, 101}) == 7, "world plus Y rounds to seven, mirroring five");
	Check(ExitReview::ClockHour(player, {99, 100}) == 10, "world minus X rounds to ten, mirroring two");
}

// Openings have no name of their own; level exits keep the game's own.
void Labels() {
	const Position player{100, 100};
	const std::vector<Rooms::Opening> openings{{{100, 96}, 3}};
	const auto items = ExitReview::Collect(player, openings, {Exit(7, {100, 110}, "Stairs")}, English);
	Check(items.size() == 2, "every way out is one item, opening or named exit");
	Check(items[0].label == English.roomOpening && !items[0].named, "a plain gap is spoken as a passage");
	Check(items[1].label == "Stairs" && items[1].named, "a level exit keeps the name the game gave it");
	Check(items[1].entity.id == 7, "the named exit carries its own entity for the beacon");
	Check(items[0].distance == 4 && items[1].distance == 10, "the distance is the straight line in subtiles");
}

// Nearest first, whatever order they were found in.
void NearestFirst() {
	const Position player{0, 0};
	const std::vector<Rooms::Opening> openings{{{30, 30}, 3}, {{2, 2}, 3}};
	const auto items = ExitReview::Collect(player, openings,
		{Exit(1, {10, 10}, "Far"), Exit(2, {4, 4}, "Near")}, English);
	Check(items.size() == 4, "openings and exits end up in one list");
	bool sorted = true;
	for (size_t i = 1; i < items.size(); ++i) sorted &= items[i - 1].distance <= items[i].distance;
	Check(sorted, "the list is sorted by straight-line distance");
	Check(items.front().point.x == 2 && items.back().point.x == 30, "the nearest way out is reviewed first");
}

void Cycling() {
	const Position player{0, 0};
	std::vector<Rooms::Opening> openings{{{2, 0}, 3}, {{6, 0}, 3}, {{10, 0}, 3}};
	auto items = ExitReview::Collect(player, openings, {}, English);
	ExitReview::Cycle cycle;
	Check(cycle.Next(items) == 0, "the first request reviews the nearest way out");
	Check(cycle.Next(items) == 1, "the next request moves on");
	Check(cycle.Next(items) == 2 && cycle.Next(items) == 0, "the cycle wraps round to the nearest");

	// The list is rebuilt on every request; the place in it survives that.
	Check(cycle.Next(items) == 1, "the position continues across a fresh list");
	auto reordered = ExitReview::Collect({12, 0}, openings, {}, English);
	Check(reordered.front().point.x == 10, "walking changes what is nearest");
	Check(cycle.Next(reordered) == 2, "the item under review is found again by its point, not its index");

	// The reviewed way out disappearing starts over at the nearest.
	auto shorter = ExitReview::Collect(player, {{{20, 0}, 3}}, {}, English);
	Check(cycle.Next(shorter) == 0, "a list without the reviewed item starts at the nearest again");

	const std::vector<ExitReview::Item> none;
	ExitReview::Cycle empty;
	Check(empty.Next(none) == ExitReview::NoItem, "nothing to review reports no item");
	Check(cycle.Next(none) == ExitReview::NoItem, "an emptied list resets the cycle");
	Check(cycle.Next(items) == 0, "and the next real list starts at the nearest");
}

void Spoken() {
	// The opening is straight up the screen, the stairs six o'clock and nearer.
	const Position player{100, 100};
	const auto items = ExitReview::Collect(player, {{{94, 94}, 3}}, {Exit(7, {103, 103}, "Escalera")}, Spanish);
	Check(items.size() == 2 && items[0].named && !items[1].named, "the nearer way out is the named one here");
	const auto around = ExitReview::Describe(items[0], 0, 2, Spanish.beaconReachAround, Scan, Spanish);
	Check(around == "Escalera, 4 pasos, a las 6, rodeando, 1 de 2", "Spanish reads the whole line naturally");
	const auto silent = ExitReview::Describe(items[1], 1, 2, nullptr, Scan, Spanish);
	Check(silent == "abertura, 8 pasos, a las 12, 2 de 2", "an unproved route leaves the verdict out entirely");

	const auto english = ExitReview::Collect(player, {{{94, 94}, 3}}, {}, English);
	Check(ExitReview::Describe(english[0], 0, 1, English.beaconReachStraight, Scan, English)
		== "passage, 8 steps, 12 o'clock, straight, 1 of 1", "English reads the same line");

	// Hour one is grammatically singular in Spanish ("a la 1"), unlike every other hour.
	const auto hourOne = ExitReview::Collect(Position{100, 100}, {{{99, 96}, 3}}, {}, Spanish);
	Check(hourOne[0].hour == 1, "the fixture actually lands on hour one");
	Check(ExitReview::Describe(hourOne[0], 0, 1, nullptr, Scan, Spanish) == "abertura, 4 pasos, a la 1, 1 de 1",
		"Spanish hour one reads singular");
}
}

int main() {
	Cardinals();
	Diagonals();
	Labels();
	NearestFirst();
	Cycling();
	Spoken();
	Passed();
}
