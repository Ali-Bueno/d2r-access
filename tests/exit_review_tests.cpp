#include "exit_review.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace D2RAccess;
using Radar::Position;

// Adapted from upstream's tests (origin/main tests/exit_review_tests.cpp): the
// same list, order and cycle; the spoken line is this branch's.
namespace {
const Strings::Table& English = Strings::For(Strings::Language::enUS);
const Strings::Table& Spanish = Strings::For(Strings::Language::esES);
const EntityRadar::Settings Scan{};

EntityRadar::Entity Exit(uint32_t id, Position position, const char* name) {
	return {id, EntityRadar::Kind::Exit, position, name};
}

// Openings have no name of their own; level exits keep the game's own.
void Labels() {
	const Position player{100, 100};
	const std::vector<Rooms::Opening> openings{{{100, 96}, 3}};
	const auto items = ExitReview::Collect(player, openings, {Exit(7, {100, 110}, "Stairs")}, English);
	Check(items.size() == 2, "every way out is one item, opening or named exit");
	Check(items[0].label == English.roomOpening && !items[0].named, "a plain gap is spoken as a passage");
	Check(items[1].label == "Stairs" && items[1].named, "a level exit keeps the name the game gave it");
	Check(items[1].entity.id == 7, "the named exit carries its own entity for guidance");
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

// One side at a time: the scan's quarter of the plane. Screen up is world
// (-X,-Y), so each screen side is a world diagonal.
void Sides() {
	const Position player{100, 100};
	const std::vector<Rooms::Opening> openings{{{94, 94}, 3}, {{90, 90}, 3}, {{106, 106}, 3}, {{94, 106}, 3}};
	const auto items = ExitReview::Collect(player, openings, {Exit(7, {104, 96}, "Stairs")}, English);
	const auto ahead = ExitReview::Toward(items, player, Radar::Direction::Up);
	Check(ahead.size() == 2 && ahead[0].point.x == 94 && ahead[1].point.x == 90,
		"pushing up names only what is ahead, nearest first");
	Check(ExitReview::Toward(items, player, Radar::Direction::Down).size() == 1, "behind has its own");
	Check(ExitReview::Toward(items, player, Radar::Direction::Left).size() == 1, "and so has the left");
	const auto right = ExitReview::Toward(items, player, Radar::Direction::Right);
	Check(right.size() == 1 && right[0].named && right[0].label == "Stairs", "a level exit is found on its side by name");

	// "1 push is the nearest exit, subsequent push is second nearest", and
	// round again, with no timer.
	ExitReview::Cycle cycle;
	Check(cycle.Next(ahead) == 0 && cycle.Next(ahead) == 1 && cycle.Next(ahead) == 0,
		"two ways out on one side alternate, nearest first");
}

// Spoken as the scan speaks: the name and the distance, never a count.
void Spoken() {
	const Position player{100, 100};
	const auto items = ExitReview::Collect(player, {{{94, 94}, 3}}, {Exit(7, {103, 103}, "Escalera")}, Spanish);
	Check(items.size() == 2 && items[0].named && !items[1].named, "the nearer way out is the named one here");
	Check(ExitReview::Describe(items[0], Scan, Spanish) == "Escalera, 4 pasos", "a named exit reads its name and distance");
	Check(ExitReview::Describe(items[1], Scan, Spanish) == "abertura, 8 pasos", "an opening reads as an opening");
	const auto english = ExitReview::Collect(player, {{{94, 94}, 3}}, {}, English);
	const auto line = ExitReview::Describe(english[0], Scan, English);
	Check(line == "passage, 8 steps", "English reads the same line");
	Check(line.find(" of ") == std::string::npos, "no position in the list is ever spoken");
	Check(ExitReview::Nothing(Radar::Direction::Left, English) == "Nothing to the left", "an empty side says so as the scan does");
}
}

int main() {
	Labels();
	NearestFirst();
	Cycling();
	Sides();
	Spoken();
	Passed();
}
