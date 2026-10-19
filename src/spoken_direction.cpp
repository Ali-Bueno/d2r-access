#include "spoken_direction.h"

#include <atomic>
#include <cmath>
#include <numbers>

namespace D2RAccess::SpokenDirection {
namespace {
static_assert(Strings::CompassPointCount == Radar::ScreenSectors, "a compass point for every screen sector");
static_assert(Strings::DirectionStyleCount == static_cast<size_t>(Style::Count), "a phrase for every style");
constexpr int ClockHours = static_cast<int>(Strings::ClockHourCount);
// Each screen side, in Radar::Direction order (up, left, right, down), as a
// compass point and as an hour.
constexpr std::array<size_t, Radar::DirectionCount> SideSectors{0, 6, 2, 4};
constexpr std::array<size_t, Radar::DirectionCount> SideHours{0, 9, 3, 6};
std::atomic<Style> g_style{Style::Compass};
}

Style CurrentStyle() { return g_style.load(); }
void SetStyle(Style style) { if (style < Style::Count) g_style = style; }

int Bucket(Radar::Position offset, Style style) {
	if (style != Style::Clock) return Radar::ScreenSector(offset);
	const auto [x, y] = Radar::ScreenOffset(offset);
	const auto hour = std::lround(std::atan2(x, -y) / (2 * std::numbers::pi / ClockHours));
	return static_cast<int>((hour + ClockHours) % ClockHours);
}

const char* Of(Radar::Position offset, const Strings::Table& strings) {
	const Style style = CurrentStyle();
	const auto bucket = static_cast<size_t>(Bucket(offset, style));
	return style == Style::Clock ? strings.clockHours[bucket] : strings.compassPoints[bucket];
}

const char* OfSide(Radar::Direction side, const Strings::Table& strings) {
	const auto index = static_cast<size_t>(side);
	return CurrentStyle() == Style::Clock ? strings.clockHours[SideHours[index]] : strings.compassPoints[SideSectors[index]];
}
}
