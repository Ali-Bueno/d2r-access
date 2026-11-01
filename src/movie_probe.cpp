#include "movie_probe.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_widgets.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace D2RAccess::MovieProbe {

namespace {
using Clock = std::chrono::steady_clock;

// Two looks at the playing widget, far enough apart that a clock field has visibly moved.
constexpr auto FirstLook  = std::chrono::milliseconds(500);
constexpr auto SecondLook = std::chrono::milliseconds(2500);
constexpr size_t WidgetSpan  = 0x1000; // bytes of the widget object compared
constexpr size_t PointeeSpan = 0x400;  // bytes of each object it points to
constexpr size_t MaxLines    = 40;     // per kind of finding, so one movie cannot flood the log
constexpr size_t PathChars   = 260;    // Windows' MAX_PATH
constexpr size_t HelperDumpBytes = 0x100;
// Units a playback clock might count in, per second: seconds, ms, us, 100 ns, audio samples, frames.
constexpr std::array<double, 9> UnitsPerSecond{1, 1000, 1e6, 1e7, 48000, 44100, 24, 30, 60};
// A field counts as a clock when it moved by 0.8 to 1.2 of the elapsed time in one of those units.
constexpr double RateLow = 0.8, RateHigh = 1.2;

struct Block {
	uintptr_t            address = 0;
	size_t               owner = 0; // offset in the widget of the pointer that led here; SIZE_MAX for the widget
	std::vector<uint8_t> bytes;
};

const std::array<const char*, 2> Panels{"InGameMoviePlayerPanel", "MoviePlayerPanel"};

bool g_armed = false;
int g_looks = 0;
Clock::time_point g_start, g_firstLookAt;
std::vector<Block> g_first;

uintptr_t FindPlayer(const char*& panel) {
	const std::string path[] = {"MoviePlayer"};
	for (const char* name : Panels)
		if (const uintptr_t widget = NativeWidgets::Find(name, path)) {
			panel = name;
			return widget;
		}
	return 0;
}

bool CopyBlock(uintptr_t address, size_t span, size_t owner, Block& out) {
	out = {address, owner, std::vector<uint8_t>(span)};
	return NativeMemory::Copy(address, out.bytes.data(), span);
}

bool HeapLike(uintptr_t value) {
	return value > 0x10000 && value < 0x00007FFFFFFFFFFF && (value & 7) == 0;
}

std::string Lower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool NamesMovie(const std::string& text) {
	const std::string lower = Lower(text);
	return lower.find("video") != std::string::npos || lower.find(".webm") != std::string::npos
		|| lower.find(".flac") != std::string::npos || lower.find(".srt") != std::string::npos;
}

std::string Where(const Block& block, size_t offset) {
	char text[64];
	if (block.owner == SIZE_MAX) std::snprintf(text, sizeof text, "widget+0x%zX", offset);
	else std::snprintf(text, sizeof text, "*(widget+0x%zX)+0x%zX", block.owner, offset);
	return text;
}

// The widget and every object its fields point to.
std::vector<Block> Snapshot(uintptr_t widget) {
	std::vector<Block> blocks(1);
	if (!CopyBlock(widget, WidgetSpan, SIZE_MAX, blocks[0])) return {};
	for (size_t offset = 8; offset + 8 <= WidgetSpan; offset += 8) {
		uintptr_t value = 0;
		std::memcpy(&value, blocks[0].bytes.data() + offset, sizeof value);
		Block pointee;
		if (HeapLike(value) && !NativeMemory::Executable(value) && CopyBlock(value, PointeeSpan, offset, pointee))
			blocks.push_back(std::move(pointee));
	}
	return blocks;
}

void LogNames(const D2RL::PluginContext* context, const std::vector<Block>& blocks) {
	size_t lines = 0;
	for (const Block& block : blocks) {
		const size_t span = block.owner == SIZE_MAX ? WidgetSpan : PointeeSpan;
		for (const auto& found : NativeWidgets::ScanStrings(block.address, span)) {
			if (block.owner != SIZE_MAX && !NamesMovie(found.value)) continue;
			if (lines++ < MaxLines) AsyncLog::InfoF(context, "[movie-probe] string %s = \"%s\"", Where(block, found.offset).c_str(), found.value.c_str());
		}
		if (block.owner != SIZE_MAX) {
			const std::string raw = NativeMemory::CString(block.address, PathChars);
			if (NamesMovie(raw) && lines++ < MaxLines) AsyncLog::InfoF(context, "[movie-probe] chars at %s = \"%s\"", Where(block, 0).c_str(), raw.c_str());
		}
	}
	AsyncLog::InfoF(context, "[movie-probe] %zu name candidates in %zu objects", lines, blocks.size());
}

// The in-game panel's ProgressionVideoHelperWidget, which may know which act video plays: its
// strings and first bytes, to compare between two videos.
void LogHelper(const D2RL::PluginContext* context) {
	const std::string path[] = {"ProgressionHelper"};
	const uintptr_t helper = NativeWidgets::Find(Panels[0], path);
	if (!helper) return;
	for (const auto& found : NativeWidgets::ScanStrings(helper, PointeeSpan))
		AsyncLog::InfoF(context, "[movie-probe] helper+0x%zX = \"%s\"", found.offset, found.value.c_str());
	std::array<uint8_t, HelperDumpBytes> bytes{};
	if (!NativeMemory::Copy(helper, bytes.data(), bytes.size())) return;
	for (size_t row = 0; row < bytes.size(); row += 16) {
		char line[16 * 3 + 1]{};
		for (size_t i = 0; i < 16; ++i) std::snprintf(line + i * 3, 4, "%02X ", bytes[row + i]);
		AsyncLog::InfoF(context, "[movie-probe] helper+0x%03zX %s", row, line);
	}
}

template <typename T> double Field(const std::vector<uint8_t>& bytes, size_t offset) {
	T value{};
	std::memcpy(&value, bytes.data() + offset, sizeof value);
	return static_cast<double>(value);
}

bool LooksLikeClock(double before, double after, double seconds, double& unit) {
	if (!std::isfinite(before) || !std::isfinite(after) || before < 0 || after <= before) return false;
	for (const double perSecond : UnitsPerSecond) {
		const double rate = (after - before) / (seconds * perSecond);
		if (rate >= RateLow && rate <= RateHigh) {
			unit = perSecond;
			return true;
		}
	}
	return false;
}

void LogClocks(const D2RL::PluginContext* context, const std::vector<Block>& second, double seconds, double sincePlay) {
	size_t lines = 0;
	for (const Block& before : g_first) {
		const auto after = std::find_if(second.begin(), second.end(), [&](const Block& b) { return b.address == before.address && b.owner == before.owner; });
		if (after == second.end()) continue;
		for (size_t offset = 0; offset + 8 <= before.bytes.size(); offset += 4) {
			struct Kind { const char* name; double a, b; };
			const Kind kinds[] = {
				{"f32", Field<float>(before.bytes, offset), Field<float>(after->bytes, offset)},
				{"i32", Field<int32_t>(before.bytes, offset), Field<int32_t>(after->bytes, offset)},
				{"f64", offset % 8 ? NAN : Field<double>(before.bytes, offset), offset % 8 ? NAN : Field<double>(after->bytes, offset)},
				{"i64", offset % 8 ? NAN : Field<int64_t>(before.bytes, offset), offset % 8 ? NAN : Field<int64_t>(after->bytes, offset)},
			};
			for (const Kind& kind : kinds) {
				double unit = 0;
				if (LooksLikeClock(kind.a, kind.b, seconds, unit) && lines++ < MaxLines)
					AsyncLog::InfoF(context, "[movie-probe] clock %s %s: %.3f -> %.3f (%g/s; %.3f s since Play)",
						Where(before, offset).c_str(), kind.name, kind.a, kind.b, unit, sincePlay);
			}
		}
	}
	AsyncLog::InfoF(context, "[movie-probe] %zu clock candidates over %.3f s", lines, seconds);
}
}

void Started() {
	g_armed = true;
	g_looks = 0;
	g_start = Clock::now();
	g_first.clear();
}

void Ended() {
	g_armed = false;
	g_first.clear();
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_armed) return;
	const auto now = Clock::now();
	if (now - g_start < (g_looks == 0 ? FirstLook : SecondLook)) return;
	const char* panel = "";
	const uintptr_t widget = FindPlayer(panel);
	if (!widget) {
		AsyncLog::Info(context, "[movie-probe] no MoviePlayer widget");
		g_armed = false;
		return;
	}
	auto blocks = Snapshot(widget);
	if (g_looks++ == 0) {
		AsyncLog::InfoF(context, "[movie-probe] %s/MoviePlayer at %p", panel, reinterpret_cast<void*>(widget));
		LogNames(context, blocks);
		LogHelper(context);
		g_first = std::move(blocks);
		g_firstLookAt = now;
		return;
	}
	LogClocks(context, blocks, std::chrono::duration<double>(now - g_firstLookAt).count(), std::chrono::duration<double>(now - g_start).count());
	g_armed = false;
	g_first.clear();
}

}
