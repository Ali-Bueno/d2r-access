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
	bool unexplored = false; // the cue carries the unexplored tail
	bool small = false; // a wall cue for a small obstacle: its own knock
	Radar::Frame frame = Radar::Frame::Cardinal; // whose rays the direction index names
};
// Bounded, latest-state queue. The worker plays one direction at a time;
// confirmed states invalidate queued cues before the player hears stale ones.
class CueQueue {
public:
	void Push(const Radar::Events& events, CueSource source = {}, const Radar::Marks& marks = {}, const Radar::Marks& small = {});
	void Retain(const Radar::Events& states, const Radar::Marks& marks);
	Radar::Events Take(CueSource* source = nullptr);
	// The wall bump plays ahead of every pending reading; a newer one replaces it.
	void PushBump(double pan, CueSource source = {});
	bool TakeBump(double* pan, CueSource* source = nullptr);
	bool Empty() const;
	const auto& Pending() const { return pending_; }
	const CueSource& Source(size_t direction) const { return sources_[direction]; }
private:
	Radar::Events pending_{};
	std::array<CueSource, Radar::DirectionCount> sources_{};
	size_t next_ = 0;
	bool bump_ = false;
	double bumpPan_ = 0;
	CueSource bumpSource_{};
};
// gain 0..1 scales the compiled peak level. A marked direction ends in the
// unexplored tail: the same short tone after the exit's tin or the floor's breath.
// Stereo position and pitch follow the screen side of the frame's ray. A wall
// marked `small` is a small obstacle's own knock.
std::vector<int16_t> Synthesize(const Radar::Events& events, const Radar::Marks& marks, double gain = 1,
	Radar::Frame frame = Radar::Frame::Cardinal, const Radar::Marks& small = {});
inline std::vector<int16_t> Synthesize(const Radar::Events& events, double gain = 1) { return Synthesize(events, {}, gain); }
// The thud of walking into a wall, `pan` from -1 (screen left) to 1 (right).
std::vector<int16_t> SynthesizeBump(double pan, double gain = 1);
bool Start(const D2RL::PluginContext* context, double gain);
void Play(const Radar::Events& events, CueSource source, const Radar::Marks& marks = {}, const Radar::Marks& small = {});
void Bump(double pan, CueSource source);
// Drop queued cues that no longer describe the confirmed state of their
// direction. A cue already sounding is left to finish.
void Retain(const Radar::Events& states, const Radar::Marks& marks);
void Silence(const char* reason = "suspended");
void Stop();
bool Available();
}
