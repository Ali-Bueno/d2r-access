#pragma once

#include "radar.h"
#include "speech_strings.h"

// How every direction is said: a compass point, north being the top of the
// screen, or a clock hour, twelve being the top. A compass point is the nearest
// of the eight ways the stick and the arrows walk, whose diagonals are the world
// axes every wall and doorway runs along (radar.h); an hour is the screen's.
// docs/radar-readings.md#spoken-directions-2026-10-13
namespace D2RAccess::SpokenDirection {
enum class Style { Compass, Clock, Count };
Style CurrentStyle();
void SetStyle(Style style);

// What a world offset is said as: one of the eight compass points
// (Radar::ScreenSector order, walked) or one of the twelve hours, 0 being twelve.
int Bucket(Radar::Position offset, Style style);
// A world offset as it is said now.
const char* Of(Radar::Position offset, const Strings::Table& strings);
// A screen side as it is said now: north, west, east, south, or 12, 9, 3 and 6 o'clock.
const char* OfSide(Radar::Direction side, const Strings::Table& strings);
}
