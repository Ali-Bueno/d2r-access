#pragma once

#include "radar.h"
#include <D2RLPlugin/api.h>

namespace D2RAccess::RadarAudio {
// Small PCM cues, synthesized on the audio thread. Windows' built-in mixer;
// no new native library or loose sound files.
inline constexpr unsigned SampleRate = 48000, Channels = 2;
inline constexpr unsigned CueGapMs = static_cast<unsigned>(Radar::CueGapMs);
// The engine's monotonic tick plus direction identifies one generated cue.
// Position is its origin, not the player's later position during playback.
struct CueSource {
	uint64_t eventMs = 0, session = 0;
	int level = 0;
	Radar::Position position{};
	bool audition = false;
};
// Bounded, latest-state queue. The worker plays one direction at a time;
// confirmed states invalidate queued cues before the player hears stale ones.
class CueQueue {
public:
	void Push(const Radar::Events& events, CueSource source = {});
	void Retain(const Radar::Events& states);
	Radar::Events Take(CueSource* source = nullptr);
	bool Empty() const;
	const auto& Pending() const { return pending_; }
	const CueSource& Source(size_t direction) const { return sources_[direction]; }
private:
	Radar::Events pending_{};
	std::array<CueSource, Radar::DirectionCount> sources_{};
	size_t next_ = 0;
};
// gain 0..1 scales the compiled peak level.
std::vector<int16_t> Synthesize(const Radar::Events& events, double gain = 1);
bool Start(const D2RL::PluginContext* context, double gain);
void Play(const Radar::Events& events, CueSource source);
// Drop queued cues that no longer describe the confirmed state of their
// direction. A cue already sounding is left to finish.
void Retain(const Radar::Events& states);
void Silence(const char* reason = "suspended");
void Stop();
bool Available();
}
