#include "wall_tones.h"
#include "test_support.h"

#include <cmath>
#include <vector>

using namespace D2RAccess;
using namespace D2RAccess::WallTones;
using Radar::Direction;
namespace {
constexpr int Size = 80, Centre = 40;
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Monster = 0x100, NoPath = 0x1000;
constexpr double Tolerance = 1e-6;

struct Room {
	Radar::World world;
	Room() {
		world.player = {Centre, Centre};
		world.subtileFraction = {0.5, 0.5};
		world.walkingMask = PlayerPath;
		world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size)});
	}
	void Put(int x, int y, uint16_t flags) { world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags; }
	double Clear(Direction direction, double range = DefaultRange) const { return Clearance(world, direction, range); }
};

void GainCurve() {
	Check(Gain(DefaultRange, DefaultRange) == 0 && Gain(DefaultRange + 5, DefaultRange) == 0, "silent at and beyond the range");
	Check(std::abs(Gain(0, DefaultRange) - 1) < Tolerance && std::abs(Gain(-3, DefaultRange) - 1) < Tolerance, "full at a touching wall");
	Check(std::abs(Gain(6, 12) - 0.25) < Tolerance, "half way is a quarter (quadratic)");
	Check(Gain(3, 12) > Gain(6, 12) && Gain(6, 12) > Gain(9, 12), "closer is louder");
	Check(Gain(NAN, 12) == 0 && Gain(1, 0) == 0 && Gain(1, NAN) == 0, "bad input is silent");
}

void GlideAndPan() {
	Check(Glide(0, 1, 0, 25) == 0, "no time passed, no change");
	Check(Glide(0, 1, 25, 0) == 1, "no glide jumps");
	const float step = Glide(0, 1, 25, 25);
	Check(std::abs(step - (1 - std::exp(-1.0))) < 1e-5, "one time constant covers 63 percent");
	Check(Glide(0, 1, 1000, 25) > 0.99f, "and settles");
	Check(Pan(Direction::Left) == -1 && Pan(Direction::Right) == 1 && Pan(Direction::Up) == 0 && Pan(Direction::Down) == 0,
		"sides hard-panned, up and down centred");
	const auto centre = PanGains(0), left = PanGains(-1), right = PanGains(1);
	Check(std::abs(centre.left - centre.right) < Tolerance && std::abs(centre.left * centre.left + centre.right * centre.right - 1) < Tolerance,
		"centre is equal power");
	Check(left.left > 0.99f && left.right < 0.01f && right.right > 0.99f && right.left < 0.01f, "hard pan sends to one side");
	Check(Trim(Direction::Down) > 1.4f && Trim(Direction::Down) < 1.42f && Trim(Direction::Up) == 1.0f, "only the low band behind is lifted 3 dB");
	Check(BandHz[0] > BandHz[1] && BandHz[1] > BandHz[3], "up is high, sides mid, behind low");
}

void ClearanceInOpenAndAgainstWalls() {
	Room room;
	for (auto direction : {Direction::Up, Direction::Left, Direction::Right, Direction::Down})
		Check(room.Clear(direction) == DefaultRange, "open ground reads the whole range");
	// Screen up runs along -x,-y: a wall row at y=36 stops it about (40.5-37)/0.707 along.
	for (int x = 0; x < Size; ++x) room.Put(x, 36, Wall);
	const double up = room.Clear(Direction::Up);
	Check(up > 4.5 && up < 5.5, "a wall row on the up ray is found about 5 subtiles away");
	Check(room.Clear(Direction::Down) == DefaultRange, "the other direction is still open");
}

void SideRaysCross() {
	Room room;
	for (int x = 0; x < Size; ++x) room.Put(x, 36, Wall);
	// Left is (-x,+y) and right (+x,-y): right climbs toward the row, left leaves it.
	Check(room.Clear(Direction::Right) < DefaultRange, "right runs into the row");
	Check(room.Clear(Direction::Left) == DefaultRange, "left runs away from it");
}

void LaneRule() {
	Room room;
	room.world.bodyPattern = 2;
	room.Put(Centre - 4, Centre - 4, Wall);
	Check(room.Clear(Direction::Up) == DefaultRange, "a lone cell on the centre line is passed by the side lanes");
	Room point;
	point.Put(Centre - 4, Centre - 4, Wall);
	Check(point.Clear(Direction::Up) < DefaultRange, "a point body has only the centre lane");
	Room wide;
	wide.world.bodyPattern = 2;
	for (int x = 0; x < Size; ++x) wide.Put(x, 36, Wall);
	Check(wide.Clear(Direction::Up) < DefaultRange, "a wall across every lane is a wall");
}

void WhatBlocks() {
	Room room;
	room.Put(Centre - 3, Centre - 3, Monster | NoPath);
	Check(room.Clear(Direction::Up) == DefaultRange, "a creature is no wall");
	room.Put(Centre - 3, Centre - 3, 0x0400);
	Check(room.Clear(Direction::Up) < DefaultRange, "an object that blocks the walking mask is");
	Room door;
	door.Put(Centre - 3, Centre - 3, 0x0800);
	Check(door.Clear(Direction::Up) < DefaultRange, "a closed door is");
}

void OutsideTheMap() {
	Room room;
	room.world.player = {3, 40};
	room.world.subtileFraction = {0.5, 0.5};
	// Up runs toward x<0, outside the loaded room: the map edge reads as a wall.
	const double up = Clearance(room.world, Direction::Up, DefaultRange);
	Check(up > 3 && up < 6, "cells in no room count as an obstacle");
	Room blank;
	blank.Put(Centre - 3, Centre - 3, Radar::Collision::Blank);
	Check(blank.Clear(Direction::Up) < DefaultRange, "a blank cell reads as an obstacle too");
	Room lost;
	lost.world.player = {500, 500};
	Check(lost.Clear(Direction::Up) == DefaultRange, "no known player cell, nothing to measure: silent");
	Check(Clearance(room.world, Direction::Up, 0) == 0 && Clearances(room.world, DefaultRange)[0] == up, "zero range and the four-way helper");
}

void Loops() {
	const auto a = BuildLoop(BandHz[1], 7), again = BuildLoop(BandHz[1], 7), other = BuildLoop(BandHz[1], 8);
	Check(a.size() == LoopSamples && a == again && a != other, "loops are deterministic per seed");
	double energy = 0, biggestStep = 0, biggestSample = 0;
	for (size_t i = 0; i < a.size(); ++i) {
		energy += static_cast<double>(a[i]) * a[i];
		biggestSample = std::max(biggestSample, static_cast<double>(std::abs(a[i])));
		const double step = std::abs(a[i] - a[(i + 1) % a.size()]); // includes the wrap from the last sample to the first
		biggestStep = std::max(biggestStep, step);
	}
	Check(std::abs(std::sqrt(energy / a.size()) - 1) < 1e-3, "unit RMS");
	Check(biggestStep < biggestSample, "the seam is no larger a step than the sound makes anywhere else");
	const auto crossings = [](const std::vector<float>& loop) {
		int count = 0;
		for (size_t i = 1; i < loop.size(); ++i) count += (loop[i - 1] < 0) != (loop[i] < 0);
		return count;
	};
	const int low = crossings(BuildLoop(BandHz[3], 1)), mid = crossings(a), high = crossings(BuildLoop(BandHz[0], 1));
	Check(low < mid && mid < high, "the bands are ordered in pitch");
}
}

int main() {
	GainCurve();
	GlideAndPan();
	ClearanceInOpenAndAgainstWalls();
	SideRaysCross();
	LaneRule();
	WhatBlocks();
	OutsideTheMap();
	Loops();
	Passed();
}
