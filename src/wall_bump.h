#pragma once

#include "radar.h"

#include <cmath>
#include <numbers>

// The thud of pushing the stick into a wall: once per contact, from the
// game's own refusal of the move. `docs/radar-readings.md`
namespace D2RAccess::WallBump {
// Policy values, each with its origin; the turn and re-arm rules follow the
// Last Epoch mod's WallBumpDetector.
struct Settings {
	double moveDistance = Radar::Settings{}.confirmDistance; // travel that re-arms: the radar's confirming step
	double turnCosine = std::cos(std::numbers::pi / 4); // a push turned past 45 degrees meets another wall
	uint64_t releaseMs = 300; // design: no refused attempt this long ends the push; a held stick retries every frame
	uint64_t repeatMs = Radar::Settings{}.cueIntervalMs; // never more than one thud per radar cue interval
	uint64_t maxGapMs = Radar::Settings{}.maxGapMs; // a longer poll gap starts over, as the radar does
};
// One poll of the push against the terrain.
struct Sample {
	uint64_t attempts = 0; // refused stick attempts so far (WallSlideService::LastBlocked)
	Radar::Position position; // exact, including the 16.16 fraction
	Radar::Position push; // world direction of the refused push; zero when unknown
	bool contact = false; // terrain blocks that way (Contact)
	uint64_t now = 0;
};
class Detector {
public:
	explicit Detector(Settings settings = {}) : settings_(settings) {}
	// True when this poll is a new contact: the game refused the push, the
	// player stands still and terrain blocks that way.
	bool Tick(const Sample& sample);
	void Reset(); // the next sample is a silent baseline
private:
	Settings settings_;
	bool started_ = false, armed_ = true, bumped_ = false;
	uint64_t attempts_ = 0, lastTick_ = 0, lastAttempt_ = 0, lastBump_ = 0;
	Radar::Position previous_{}, anchor_{}, direction_{};
};
// The farthest edge of the widest native footprint (3x3) is one cell from its
// centre, so the wall it presses against is at most two cells away on either
// axis: two cells along a straight push, its corner's diagonal along a slanted one.
inline constexpr int ContactCells = 2;
// A quarter cell: the walk along the push enters every cell it crosses but a corner clip.
inline constexpr double ContactStep = 0.25;
// Whether terrain (never a creature) blocks the push within ContactCells, on
// either axis, of the player's exact position.
bool Contact(const Radar::World& world, Radar::Position push);
// The push's screen-horizontal share, -1 (left) to 1 (right).
double Pan(Radar::Position push);
}
