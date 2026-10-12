#pragma once

#include "radar.h"

#include <array>
#include <vector>

// Wall tones: one continuous sound per screen direction whose level follows how
// near the nearest impassable cell is. Pure logic; the audio is wall_tone_audio.
// Design and sources: reference/audio-navigation/wall-sonification/.
namespace D2RAccess::WallTones {
// Policy values; none is a game constant.
inline constexpr double DefaultRange = 12;   // subtiles: silent beyond, about two and a half map tiles
inline constexpr double MinRange = 2, MaxRange = 40; // configurable bounds
inline constexpr double RayStep = 0.25;      // subtiles; the same walk step as WallBump::ContactStep
inline constexpr int LaneCount = 3;          // centre, then one body radius to each side (Grimdark lane rule)
// Body radius in subtiles: pattern 0 is a point, every other pattern a 3 subtile span.
double BodyRadius(int bodyPattern);
// Distance in subtiles from the player to the nearest impassable cell along the
// direction's screen axis, `range` when nothing blocks within it. Of the three
// lanes across the body the farthest wins, so a corner beside the way is not a wall.
double Clearance(const Radar::World& world, Radar::Direction direction, double range);
std::array<double, Radar::DirectionCount> Clearances(const Radar::World& world, double range);
// Grimdark curve: silent at the range, full at a touching wall, (1 - d/range)^2 between.
float Gain(double distance, double range);
// One-pole glide of `current` toward `target` over `dtMs`, with time constant `glideMs`.
float Glide(float current, float target, double dtMs, double glideMs);

// Pan of a direction, -1 (left) to 1 (right), from its screen side.
double Pan(Radar::Direction direction);
struct StereoGains { float left = 0, right = 0; };
// Equal-power stereo gains of a pan.
StereoGains PanGains(double pan);

// Band centre of each direction in Radar::Direction order (up, left, right, down).
// Last Epoch mod values, derived from Grimdark's walltones and lowered for warmth.
inline constexpr std::array<double, Radar::DirectionCount> BandHz{800, 450, 450, 250};
// Lift of the low band behind the player (Grimdark lifts it 3 dB by ear).
inline constexpr double BehindTrimDb = 3;
float Trim(Radar::Direction direction);

// One seamless loop of band-limited noise at unit RMS: random-phase spectrum
// shaped like two band-pass stages (Q 1.8) and a 1.8 kHz low-pass, inverse FFT.
inline constexpr int SampleRate = 48000;
inline constexpr size_t LoopSamples = 1 << 17; // 2.73 s: long enough not to read as a pulse
std::vector<float> BuildLoop(double centreHz, uint32_t seed);
// RMS of one direction at a touching wall, before the user level (-28 dBFS).
inline constexpr double ContactRms = 0.04;
}
