#include "movie_probe.h"

#include "async_log.h"
#include "audio_description.h"
#include "native_memory.h"
#include "native_widgets.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

namespace D2RAccess::MovieProbe {

namespace {
using Clock = std::chrono::steady_clock;

// Looks for the movie's playback position (docs/cinematics-audio-description.md) when the known one
// (audio_description.cpp, ClockOwnerOffset) is not found, as after a game update: the objects
// reachable from the MoviePlayer widget, four pointer levels deep, copied at three moments; a field
// that advances steadily in some unit and was at zero when the video began is a candidate, and every
// audio-rate counter is logged too. It found the clock on 2026-11-01 (depth 4).
constexpr std::array<std::chrono::milliseconds, 3> Looks{std::chrono::milliseconds(3000), std::chrono::milliseconds(5000),
	std::chrono::milliseconds(7000)};
constexpr size_t WidgetSpan = 0x1000;  // bytes of the widget
constexpr size_t ObjectSpan = 0x400;   // bytes of each object reached
constexpr size_t MaxDepth   = 4;       // pointer hops from the widget
constexpr size_t MaxObjects = 8000;
constexpr size_t MaxLines   = 60;
// Units a playback clock might count in, per second: seconds, ms, us, 100 ns, ns, audio samples, frames.
constexpr std::array<double, 10> UnitsPerSecond{1, 1000, 1e6, 1e7, 1e9, 48000, 44100, 24, 30, 60};
// Both steps between the looks within 0.85 to 1.15 of the elapsed time in that unit.
constexpr double RateLow = 0.85, RateHigh = 1.15;
// When a candidate was at zero, in seconds since Play: a playback clock starts with the first frame,
// about 1 s after Play (0.7 to 3.1 s seen).
constexpr double OriginLow = -0.5, OriginHigh = 6.0;

struct Object {
	uintptr_t            address = 0;
	std::string          path; // "w+0x228>+0x10" : the offsets followed from the widget
	size_t               span = 0;
	std::vector<uint8_t> bytes[Looks.size()];
};

const std::array<const char*, 2> Panels{"InGameMoviePlayerPanel", "MoviePlayerPanel"};

bool g_armed = false;
size_t g_looks = 0;
Clock::time_point g_start;
double g_lookAt[Looks.size()]{};
std::vector<Object> g_objects;

uintptr_t FindPlayer(const char*& panel) {
	const std::string path[] = {"MoviePlayer"};
	for (const char* name : Panels)
		if (const uintptr_t widget = NativeWidgets::Find(name, path)) {
			panel = name;
			return widget;
		}
	return 0;
}

bool HeapLike(uintptr_t value) {
	return value > 0x10000 && value < 0x00007FFFFFFFFFFF && (value & 7) == 0;
}

// Breadth first from the widget: every readable, non-code pointer is followed once.
void Walk(uintptr_t widget) {
	g_objects.clear();
	std::unordered_set<uintptr_t> seen{widget};
	std::deque<std::pair<size_t, size_t>> queue; // object index, depth
	Object root{widget, "w", WidgetSpan};
	root.bytes[0].resize(WidgetSpan);
	if (!NativeMemory::Copy(widget, root.bytes[0].data(), WidgetSpan)) return;
	g_objects.push_back(std::move(root));
	queue.push_back({0, 0});
	while (!queue.empty() && g_objects.size() < MaxObjects) {
		const auto [index, depth] = queue.front();
		queue.pop_front();
		if (depth >= MaxDepth) continue;
		const size_t span = g_objects[index].span;
		for (size_t offset = 0; offset + 8 <= span && g_objects.size() < MaxObjects; offset += 8) {
			uintptr_t value = 0;
			std::memcpy(&value, g_objects[index].bytes[0].data() + offset, sizeof value);
			if (!HeapLike(value) || !seen.insert(value).second || NativeMemory::Executable(value)) continue;
			Object next{value, "", ObjectSpan};
			next.bytes[0].resize(ObjectSpan);
			if (!NativeMemory::Copy(value, next.bytes[0].data(), ObjectSpan)) continue;
			char step[24];
			std::snprintf(step, sizeof step, "%s+0x%zX", index == 0 ? "" : ">", offset);
			next.path = g_objects[index].path + step;
			g_objects.push_back(std::move(next));
			queue.push_back({g_objects.size() - 1, depth + 1});
		}
	}
}

void Recopy(size_t look) {
	for (Object& object : g_objects) {
		object.bytes[look].resize(object.span);
		if (!NativeMemory::Copy(object.address, object.bytes[look].data(), object.span)) object.bytes[look].clear();
	}
}

template <typename T> double Field(const std::vector<uint8_t>& bytes, size_t offset) {
	T value{};
	std::memcpy(&value, bytes.data() + offset, sizeof value);
	return static_cast<double>(value);
}

// The unit in which `v` advanced steadily over the looks and was near zero when the video began.
bool Clocklike(const double (&v)[Looks.size()], double& unit, double& origin) {
	for (const double x : v) if (!std::isfinite(x) || x < 0) return false;
	const double step1 = g_lookAt[1] - g_lookAt[0], step2 = g_lookAt[2] - g_lookAt[1];
	for (const double perSecond : UnitsPerSecond) {
		const double rate1 = (v[1] - v[0]) / (step1 * perSecond), rate2 = (v[2] - v[1]) / (step2 * perSecond);
		if (rate1 < RateLow || rate1 > RateHigh || rate2 < RateLow || rate2 > RateHigh) continue;
		unit = perSecond;
		origin = g_lookAt[2] - v[2] / perSecond;
		return true;
	}
	return false;
}

void Report(const D2RL::PluginContext* context) {
	size_t lines = 0, near = 0, far = 0;
	for (const Object& object : g_objects) {
		if (object.bytes[1].empty() || object.bytes[2].empty()) continue;
		for (size_t offset = 0; offset + 8 <= object.span; offset += 4) {
			struct Kind { const char* name; bool wide; };
			for (const Kind kind : {Kind{"f32", false}, Kind{"i32", false}, Kind{"f64", true}, Kind{"i64", true}}) {
				if (kind.wide && offset % 8) continue;
				double v[Looks.size()];
				for (size_t look = 0; look < Looks.size(); ++look) {
					const auto& bytes = object.bytes[look];
					v[look] = kind.name[0] == 'f' ? (kind.wide ? Field<double>(bytes, offset) : Field<float>(bytes, offset))
						: (kind.wide ? Field<int64_t>(bytes, offset) : Field<int32_t>(bytes, offset));
				}
				double unit = 0, origin = 0;
				if (!Clocklike(v, unit, origin)) continue;
				const bool fromStart = origin >= OriginLow && origin <= OriginHigh;
				(fromStart ? near : far)++;
				// Audio sample counts are logged whatever their origin: one may run from before Play.
				const bool audio = unit == 48000 || unit == 44100;
				if ((!fromStart && !audio) || lines >= MaxLines) continue;
				++lines;
				AsyncLog::InfoF(context, "[movie-probe] clock %s+0x%zX %s: %.4f %.4f %.4f (%g/s), zero %.2f s after Play; object %p",
					object.path.c_str(), offset, kind.name, v[0], v[1], v[2], unit, origin, reinterpret_cast<void*>(object.address));
			}
		}
	}
	AsyncLog::InfoF(context, "[movie-probe] %zu objects: %zu clocks from the video's start, %zu other steady counters",
		g_objects.size(), near, far);
}
}

void Started() {
	g_armed = true;
	g_looks = 0;
	g_start = Clock::now();
	g_objects.clear();
}

void Ended() {
	g_armed = false;
	g_objects.clear();
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_armed) return;
	const auto now = Clock::now();
	if (now - g_start < Looks[g_looks]) return;
	if (g_looks == 0) {
		// The known chain reads: nothing to look for. It runs again when a game update moves it.
		if (AudioDescription::VideoClockRead()) {
			g_armed = false;
			return;
		}
		const char* panel = "";
		const uintptr_t widget = FindPlayer(panel);
		if (!widget) {
			AsyncLog::Info(context, "[movie-probe] no MoviePlayer widget");
			g_armed = false;
			return;
		}
		Walk(widget);
		AsyncLog::InfoF(context, "[movie-probe] %s/MoviePlayer at %p: %zu objects within %zu hops", panel,
			reinterpret_cast<void*>(widget), g_objects.size(), MaxDepth);
	} else {
		Recopy(g_looks);
	}
	g_lookAt[g_looks] = std::chrono::duration<double>(Clock::now() - g_start).count();
	if (++g_looks < Looks.size()) return;
	Report(context);
	g_armed = false;
	g_objects.clear();
}

}
