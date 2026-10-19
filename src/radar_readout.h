#pragma once
#include "radar.h"
#include "speech_strings.h"
#include <string>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity); // clockwise from up; -1 at rest
// The spoken name of one radar direction, and all four in order.
const char* DirectionName(Radar::Frame frame, size_t direction, const Strings::Table& text);
std::string DirectionList(Radar::Frame frame, const Strings::Table& text);
// The radar's four rays named as what they are: "Radar rays: north, west, east, south".
// Its frame is a choice of four fixed rays; a target's direction is told apart in eight or twelve.
std::string RaysPhrase(Radar::Frame frame, const Strings::Table& text);
std::string Describe(const Radar::Engine& engine, const Strings::Table& text);
}
