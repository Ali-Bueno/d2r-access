#include "rooms.h"
#include "test_support.h"

#include <vector>

using namespace D2RAccess;
using Radar::Position;
using Radar::World;

namespace {
constexpr uint16_t PlayerPath = 0x1C09; // observed native player walking mask
constexpr uint16_t Wall = 1;            // a walking-mask bit: real collision
constexpr uint16_t Blank = 0x20;        // native unknown subtile, as radar_grid.cpp reads it

// One native room: a rectangle of open floor inside a one-cell wall ring, the
// shape a maze preset has before its openings are cut.
Radar::RoomGrid Cell(int x, int y, int side) {
	Radar::RoomGrid room{x, y, side, side, std::vector<uint16_t>(static_cast<size_t>(side) * side, 0)};
	for (int i = 0; i < side; ++i) {
		room.cells[static_cast<size_t>(i)] = Wall;                                              // top
		room.cells[static_cast<size_t>(side - 1) * side + i] = Wall;                            // bottom
		room.cells[static_cast<size_t>(i) * side] = Wall;                                       // left
		room.cells[static_cast<size_t>(i) * side + side - 1] = Wall;                            // right
	}
	return room;
}
void Carve(Radar::RoomGrid& room, int x, int y) {
	room.cells[static_cast<size_t>(y - room.y) * room.width + (x - room.x)] = 0;
}
World Level(std::vector<Radar::RoomGrid> rooms, Position player, int pattern = 0) {
	World world;
	world.session = world.level = 1;
	world.player = player;
	world.bodyPattern = pattern;
	world.walkingMask = PlayerPath;
	world.rooms = std::move(rooms);
	return world;
}

// Two 12x12 preset cells side by side, with a three-cell doorway cut through
// the wall they share: the shape of every maze corridor junction.
void Doorway() {
	auto left = Cell(0, 0, 12), right = Cell(12, 0, 12);
	for (int y = 5; y <= 7; ++y) { Carve(left, 11, y); Carve(right, 12, y); }
	const auto world = Level({left, right}, {5, 6});

	Rooms::Room room;
	Check(Rooms::Describe(world, world.player, room), "the player's own streamed room is found");
	Check(room.x == 0 && room.y == 0 && room.width == 12 && room.height == 12,
		"the room reported is the native room's own rectangle");
	Check(room.openings.size() == 1, "a cell walled on three sides has exactly one way out");
	Check(room.openings.front().width == 3, "the opening is as wide as the doorway that was cut");
	Check(room.openings.front().centre.x == 11, "the opening sits on the border cell it was cut through");
	Check(room.openings.front().centre.y == 6, "the opening's centre is the middle of its run");

	// Standing in the other cell sees the same doorway from its own side.
	auto other = world;
	other.player = {18, 6};
	Rooms::Room mirrored;
	Check(Rooms::Describe(other, other.player, mirrored), "the neighbouring room is found from inside it");
	Check(mirrored.x == 12 && mirrored.openings.size() == 1, "the neighbour reports the same single doorway");
	Check(mirrored.openings.front().centre.x == 12, "each side reports the opening on its own border");
}

// A gap that leads nowhere is not a way out: the cell beyond it has to be open
// floor of another streamed room, not unknown ground.
void GapToNowhere() {
	auto only = Cell(0, 0, 12);
	for (int y = 5; y <= 7; ++y) Carve(only, 11, y);
	const auto world = Level({only}, {5, 6});
	Rooms::Room room;
	Check(Rooms::Describe(world, world.player, room), "a lone room is still described");
	Check(room.openings.empty(), "a hole in the wall with no room behind it is not an exit");

	// Nor is one that opens onto collision.
	auto blocked = Cell(12, 0, 12);
	const auto walled = Level({only, blocked}, {5, 6});
	Rooms::Room second;
	Check(Rooms::Describe(walled, walled.player, second), "the room is described with a neighbour present");
	Check(second.openings.empty(), "a doorway facing the neighbour's own wall is not an exit");
}

// Unknown ground reads as neither open nor blocked and must never be an exit.
void UnknownGround() {
	auto room = Cell(0, 0, 12);
	for (int y = 5; y <= 7; ++y) Carve(room, 11, y);
	Radar::RoomGrid unmapped{12, 0, 12, 12, std::vector<uint16_t>(144, Blank)};
	const auto world = Level({room, unmapped}, {5, 6});
	Rooms::Room described;
	Check(Rooms::Describe(world, world.player, described), "the room is described beside unmapped ground");
	Check(described.openings.empty(), "unknown ground behind a gap is not proved to be a way out");
}

// The gap has to admit the body: the native footprint decides, not a guess.
void BodyFits() {
	Check(Rooms::PassableWidth(0) == 1, "a point body fits a one-cell slot");
	Check(Rooms::PassableWidth(1) == 3 && Rooms::PassableWidth(2) == 3 && Rooms::PassableWidth(5) == 3,
		"a cross or square body needs the cells beside it too");

	auto left = Cell(0, 0, 12), right = Cell(12, 0, 12);
	Carve(left, 11, 6);
	Carve(right, 12, 6);
	const auto point = Level({left, right}, {5, 6}, 0);
	Rooms::Room thin;
	Check(Rooms::Describe(point, point.player, thin) && thin.openings.size() == 1,
		"a one-cell slot is a way out for a point body");

	const auto wide = Level({left, right}, {5, 6}, 2);
	Rooms::Room tooThin;
	Check(Rooms::Describe(wide, wide.player, tooThin) && tooThin.openings.empty(),
		"the same slot is not a way out for a body that cannot fit through it");
}

// Four ways out, one per side, are reported in border order.
void Junction() {
	auto centre = Cell(12, 12, 12);
	std::vector<Radar::RoomGrid> rooms{centre, Cell(12, 0, 12), Cell(12, 24, 12), Cell(0, 12, 12), Cell(24, 12, 12)};
	const auto carve = [&](int index, int x, int y) { Carve(rooms[static_cast<size_t>(index)], x, y); };
	for (int i = -1; i <= 1; ++i) {
		carve(0, 17 + i, 12); carve(1, 17 + i, 11); // up
		carve(0, 17 + i, 23); carve(2, 17 + i, 24); // down
		carve(0, 12, 17 + i); carve(3, 11, 17 + i); // left
		carve(0, 23, 17 + i); carve(4, 24, 17 + i); // right
	}
	const auto world = Level(rooms, {17, 17});
	Rooms::Room room;
	Check(Rooms::Describe(world, world.player, room), "the middle cell of a junction is found");
	Check(room.openings.size() == 4, "a cell open on all four sides reports four ways out");
	bool widths = true;
	for (const auto& opening : room.openings) widths &= opening.width == 3;
	Check(widths, "every doorway of the junction is its own cut width");
}

void Outside() {
	const auto world = Level({Cell(0, 0, 12)}, {50, 50});
	Rooms::Room room;
	Check(!Rooms::Describe(world, {50, 50}, room), "a position no resident room covers describes nothing");
}

// The announce gate, ported from grimdark. The dwell only applies inside the
// first second after a change, so walking room to room is never delayed.
void Settling() {
	const Rooms::RoomKey a{0, 0, true}, b{12, 0, true}, offGrid{};
	Rooms::Hysteresis gate;
	Check(gate.Observe(a, 0), "the first room is announced at once, with nothing to settle from");
	Check(!gate.Observe(a, 250), "the same room is never announced twice");
	Check(gate.Observe(b, 1500), "a room entered after a settled second is announced instantly");

	// A flip inside the settle window has to hold for the dwell.
	Check(!gate.Observe(a, 1600), "a room seen 100 ms after a change starts a candidate instead of speaking");
	Check(!gate.Observe(a, 1900), "the candidate still has not held for the dwell");
	Check(gate.Observe(a, 2000), "once it has held the dwell it becomes the room and is announced");

	// Standing on a threshold straddles two rooms and must stay silent.
	Rooms::Hysteresis threshold;
	Check(threshold.Observe(a, 0), "the room stood in is announced");
	bool spoke = false;
	for (int now = 100; now <= 900; now += 200) {
		spoke |= threshold.Observe(b, now);
		spoke |= threshold.Observe(a, now + 100);
	}
	Check(!spoke, "straddling a border back and forth within the second announces nothing");
	Check(threshold.current == a, "and the room stood in is still the current one");

	// Off-grid is not a room change: not knowing is not leaving.
	Check(!threshold.Observe(offGrid, 2000), "a position no room covers announces nothing");
	Check(threshold.current == a, "and keeps the current room");
	threshold.Forget();
	Check(!threshold.current.valid, "a level change forgets the room");
	Check(threshold.Observe(a, 3000), "so the same room is announced again on arriving in it");
}

void Geometry() {
	Rooms::Room room{4, 8, 12, 12, {}};
	Check(room.Contains({4, 8}) && room.Contains({15.9, 19.9}), "the rectangle includes its own first and last cell");
	Check(!room.Contains({3.9, 8}) && !room.Contains({16, 8}), "the rectangle excludes the cells beyond it");
	Check(room.Centre().x == 4 + 5.5 && room.Centre().y == 8 + 5.5, "the centre is the middle of the cells it covers");
}
}

int main() {
	Doorway();
	GapToNowhere();
	UnknownGround();
	BodyFits();
	Junction();
	Outside();
	Settling();
	Geometry();
	Passed();
}
