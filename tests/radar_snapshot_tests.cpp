#include "radar_trace.h"
#include "async_log.h"
#include "test_support.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::vector<std::string> records;
}
namespace D2RAccess::AsyncLog {
void InfoF(const D2RL::PluginContext*, const char* format, ...) noexcept {
	char text[4096]{};
	va_list args;
	va_start(args, format);
	const auto length = std::vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	Check(length >= 0 && length < int(sizeof(text)), "snapshot and direction records fit the actual asynchronous log limit");
	records.emplace_back(text);
}
}
using namespace D2RAccess;
namespace {
void RoundTrip(bool patterned) {
	Radar::World world;
	world.player = {80, 90};
	world.subtileFraction = {.25, .75};
	world.session = 2;
	world.level = 1;
	world.bodyPattern = 1;
	world.walkingMask = 0x1C09;
	world.rooms = {{50, 60, 64, 64, std::vector<uint16_t>(64 * 64)}};
	// Worst-case changing runs include walls, live actors, doors and blank cells.
	constexpr uint16_t Flags[]{0, 1, 0x1100, 0x800, 0x400, 0x20};
	if (patterned)
		for (size_t i = 0; i < world.rooms[0].cells.size(); ++i) world.rooms[0].cells[i] = Flags[i % std::size(Flags)];
	records.clear();
	Radar::Engine engine;
	RadarTrace::Snapshot(nullptr, world, engine, 1234);
	std::vector<uint16_t> decoded;
	size_t parts = 0, directions = 0;
	for (const auto& line : records) {
		if (line.starts_with("[radar-direction]")) {
			Check(line.find("event_ms=1234 direction=") != std::string::npos
				&& line.find(std::string(" direction=") + RadarTrace::DirectionName(directions) + " ") != std::string::npos,
				"the per-direction records follow the engine's own direction order");
			Check(line.find(" depth=") != std::string::npos && line.find(" open=") != std::string::npos
				&& line.find(" close=") != std::string::npos && line.find(" available=") != std::string::npos,
				"each direction record keeps the depth, the thresholds it was judged against and its coverage");
			++directions;
			continue;
		}
		Check(line.find("event_ms=1234 session=2 level=1") != std::string::npos
			&& line.find("pattern=1 mask=1C09 position=80.25000,90.75000") != std::string::npos,
			"every map chunk retains session, body, movement mask and fractional event position");
		const auto start = std::stoull(line.substr(line.find(" start=") + 7));
		Check(start == decoded.size(), "chunk offsets detect lost or reordered collision data");
		std::string_view data(line);
		data.remove_prefix(line.find(" data=") + 6);
		while (!data.empty()) {
			const auto separator = data.find(':'), end = data.find(',');
			const auto flags = static_cast<uint16_t>(std::stoul(std::string(data.substr(0, separator)), nullptr, 16));
			const auto count = std::stoul(std::string(data.substr(separator + 1, end - separator - 1)));
			decoded.insert(decoded.end(), count, flags);
			data.remove_prefix(end + 1);
		}
		++parts;
	}
	const int radius = Radar::StructuralRadius({}, world.bodyPattern), side = 2 * radius + 1;
	Check(decoded.size() == static_cast<size_t>(side) * side, "the automatic snapshot contains the whole structural window");
	bool exact = true;
	for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x) {
		uint16_t flags = 0;
		if (world.Cell(80 - radius + x, 90 - radius + y, Radar::Layer::Movement, &flags) == Radar::State::Unknown)
			flags = 0x20;
		exact &= decoded[static_cast<size_t>(y) * side + x] == flags;
	}
	Check(exact, "replayed collision preserves actors, real barriers and unknown coverage without truncation");
	Check(directions == Radar::DirectionCount, "every event snapshot also records all four directions");
	Check(!patterned || parts > 1, "the high-entropy map exercises multiple independently numbered chunks");
}
}
int main() {
	RoundTrip(false);
	RoundTrip(true);
	Passed();
}
