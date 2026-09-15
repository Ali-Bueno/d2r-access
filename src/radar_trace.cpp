#include "radar_trace.h"
#include "async_log.h"

namespace D2RAccess::RadarTrace {
const char* DirectionName(size_t direction) {
	constexpr const char* Names[] = {"up", "left", "right", "down"};
	return direction < Radar::DirectionCount ? Names[direction] : "unknown";
}
const char* StateName(Radar::State state) {
	switch (state) {
	case Radar::State::Open: return "open";
	case Radar::State::Blocked: return "blocked";
	default: return "unknown";
	}
}
void Audio(const D2RL::PluginContext* context, size_t direction, Radar::State state,
	const RadarAudio::CueSource& source, const char* stage, const char* reason,
	uint64_t now, unsigned result, uint64_t audioMs) {
	AsyncLog::InfoF(context,
		"[radar-audio] event_ms=%llu direction=%s state=%s stage=%s reason=%s at_ms=%llu elapsed_ms=%llu audio_ms=%llu session=%llu level=%d origin=%.0f,%.0f source=%s result=%u",
		static_cast<unsigned long long>(source.eventMs), DirectionName(direction), StateName(state), stage, reason,
		static_cast<unsigned long long>(now), static_cast<unsigned long long>(now >= source.eventMs ? now - source.eventMs : 0),
		static_cast<unsigned long long>(audioMs), static_cast<unsigned long long>(source.session), source.level,
		source.position.x, source.position.y, source.audition ? "audition" : "navigation", result);
}
}
