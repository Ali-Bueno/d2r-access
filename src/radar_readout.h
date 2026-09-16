#pragma once
#include "radar.h"
#include "speech_strings.h"
#include <string>

namespace D2RAccess::RadarReadout {
int MovementDirection(Radar::Position velocity); // clockwise from up; -1 at rest
std::string Describe(const Radar::Engine& engine, const Strings::Table& text);
}
