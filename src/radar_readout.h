#pragma once
#include "radar.h"
#include "speech_strings.h"
#include <string>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity); // clockwise from up; -1 at rest
// The spoken name of one radar direction, and all four in order.
const char* DirectionName(Radar::Frame frame, size_t direction, const Strings::Table& text);
std::string DirectionList(Radar::Frame frame, const Strings::Table& text);
std::string Describe(const Radar::Engine& engine, const Strings::Table& text);
}
