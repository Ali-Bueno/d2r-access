#include "exploration_cues.h"
#include "test_support.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

using namespace D2RAccess;

namespace {
using NativeLevelExits::Layout;
using NativeLevelExits::LevelRoom;
using NativeLevelExits::TileRect;
using Radar::Position;

Layout MakeLayout(TileRect bounds, std::initializer_list<LevelRoom> rooms) {
	Layout layout;
	layout.level = 5;
	layout.rect = bounds;
	layout.rooms = rooms;
	return layout;
}

void FirstObservationAndSequentialVisits() {
	const auto layout = MakeLayout({0, 0, 6, 2}, {
		{{0, 0, 2, 2}, true}, {{2, 0, 2, 2}, true}, {{4, 0, 2, 2}, true}});
	ExploredRooms::Memory memory;
	memory.Mark(5, 2, 0);
	memory.Mark(5, 4, 0);
	auto selected = ExplorationCues::Select(layout, {5, 5}, memory, true);
	Check(selected.entered && selected.entered->rect.x == 0 && selected.entered->position.x == 5,
		"first observation announces the room under the player, even before it is marked seen");
	Check(selected.nearest && selected.nearest->rect.x == 2 && selected.nearest->position.x == 10,
		"the nearest unvisited active room is selected even when already seen");
	Check(!memory.Visited(5, 0, 0), "selection does not change exploration memory");
	memory.Visit(5, selected.entered->rect.x, selected.entered->rect.y);
	selected = ExplorationCues::Select(layout, {5, 5}, memory, true);
	Check(!selected.entered && selected.nearest && selected.nearest->rect.x == 2,
		"recording the first arrival prevents an arrival repeat");
	selected = ExplorationCues::Select(layout, {10, 5}, memory, true);
	Check(selected.entered && selected.entered->rect.x == 2
		&& selected.nearest && selected.nearest->rect.x == 4 && selected.nearest->position.x == 20,
		"walking into the selected room starts the next room cue and announces arrival");
	memory.Visit(5, selected.entered->rect.x, selected.entered->rect.y);
	selected = ExplorationCues::Select(layout, {5, 5}, memory, true);
	Check(!selected.entered && selected.nearest && selected.nearest->rect.x == 4,
		"returning to a visited room does not repeat arrival or target a visited room");
	ExploredRooms::Memory restored;
	restored.Merge(memory.Serialize());
	selected = ExplorationCues::Select(layout, {10, 5}, restored, true);
	Check(!selected.entered && selected.nearest && selected.nearest->rect.x == 4,
		"a saved and reloaded visit remains silent while other rooms remain targets");
	restored.Visit(5, 4, 0);
	selected = ExplorationCues::Select(layout, {10, 5}, restored, true);
	Check(!selected.entered && !selected.nearest, "all visited rooms leave no cue");
}

void MovementAndTieOrder() {
	const auto layout = MakeLayout({0, 0, 12, 2}, {
		{{0, 0, 2, 2}, true}, {{2, 0, 8, 2}, true}, {{10, 0, 2, 2}, true}});
	ExploredRooms::Memory memory;
	memory.Visit(5, 2, 0);
	auto selected = ExplorationCues::Select(layout, {25, 5}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 0 && selected.nearest->position.x == 9,
		"nearest point inside the left room wins from the left side of the current room");
	selected = ExplorationCues::Select(layout, {49, 5}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 10 && selected.nearest->position.x == 50,
		"moving within the same room changes the nearest target");

	auto tied = MakeLayout({10, 10, 4, 4}, {
		{{10, 10, 2, 2}, true}, {{12, 10, 2, 2}, true}, {{10, 12, 2, 2}, true}});
	selected = ExplorationCues::Select(tied, {55, 55}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 10 && selected.nearest->rect.y == 12,
		"equal-distance rooms use their coordinates as a deterministic tie break");
	std::reverse(tied.rooms.begin(), tied.rooms.end());
	selected = ExplorationCues::Select(tied, {55, 55}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 10 && selected.nearest->rect.y == 12,
		"the tie winner is independent of native room enumeration");
}

void ActiveBoundsAndRange() {
	ExploredRooms::Memory memory;
	auto layout = MakeLayout({0, 0, 8, 1}, {
		{{1, 0, 1, 1}, true}, {{2, 0, 1, 1}, false}, {{7, 0, 1, 1}, true}});
	memory.Visit(5, 1, 0);
	auto selected = ExplorationCues::Select(layout, {5, 2}, memory, true);
	Check(!selected.nearest && !selected.entered,
		"inactive rooms are ignored and a room exactly at the audible limit is silent");
	selected = ExplorationCues::Select(layout, {5.1, 2}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 7 && selected.nearest->position.x == 35,
		"a room just inside the audible limit is selected");
	layout.rooms[1].active = true;
	selected = ExplorationCues::Select(layout, {5, 2}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 2 && selected.nearest->position.x == 10,
		"an active closer room takes priority");

	layout = MakeLayout({0, 0, 2, 1}, {{{0, 0, 1, 1}, true}, {{1, 0, 1, 1}, true}});
	selected = ExplorationCues::Select(layout, {5, 2}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 0 && selected.nearest->position.x == 4,
		"the nearest point stops at width times five minus one, inside the room");
	selected = ExplorationCues::Select(layout, {4.9, 2}, ExploredRooms::Memory{}, true);
	Check(selected.nearest && selected.nearest->rect.x == 1 && selected.nearest->position.x == 5,
		"the current room is excluded even when unvisited");
}

void InvalidAndNonmaze() {
	ExploredRooms::Memory memory;
	auto layout = MakeLayout({0, 0, 4, 2}, {{{0, 0, 2, 2}, true}, {{2, 0, 2, 2}, true}});
	auto empty = [&](Position player, bool maze) {
		const auto selected = ExplorationCues::Select(layout, player, memory, maze);
		return !selected.nearest && !selected.entered;
	};
	Check(empty({5, 5}, false), "native room partition is ignored outside maze levels");
	Check(empty({25, 5}, true), "off-layout player position produces no stale cue");
	layout.rooms[0].active = false;
	Check(empty({5, 5}, true), "standing in an inactive room produces no cue");
	layout.rooms[0].active = true;
	Check(empty({std::numeric_limits<double>::quiet_NaN(), 5}, true)
		&& empty({std::numeric_limits<double>::infinity(), 5}, true), "nonfinite positions are rejected");
	layout.level = 0;
	Check(empty({5, 5}, true), "an unidentified level is rejected");
	layout.level = 5;
	layout.rect.width = 0;
	Check(empty({5, 5}, true), "an empty level rectangle is rejected");
	layout.rect.width = 4;
	layout.rooms[1].rect.width = -1;
	Check(empty({5, 5}, true), "a malformed room invalidates the layout");
	layout.rooms[1].rect = {13107, 0, 2, 1};
	Check(empty({5, 5}, true), "a room whose subtile coordinates overflow is rejected");
	layout.rooms[1].rect = {4, 0, 1, 1};
	const auto selected = ExplorationCues::Select(layout, {5, 5}, memory, true);
	Check(selected.nearest && selected.nearest->rect.x == 4,
		"room rectangles remain authoritative when the level rectangle temporarily disagrees");
}
}

int main() {
	FirstObservationAndSequentialVisits();
	MovementAndTieOrder();
	ActiveBoundsAndRange();
	InvalidAndNonmaze();
	Passed();
	return 0;
}
