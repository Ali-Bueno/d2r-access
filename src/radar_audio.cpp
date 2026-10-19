#include "radar_audio.h"
#include "async_log.h"
#include "process_lifetime.h"
#include "mod_sounds.h"
#include "radar_trace.h"
#include "sound_volume.h"
#include "wave_out.h"

#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace D2RAccess::RadarAudio {
namespace {
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false}, g_available{false};
std::atomic<uint64_t> g_generation{0};
CueQueue g_pending;
Radar::Events g_playing{};
const D2RL::PluginContext* g_context = nullptr;
CueSource g_playingSource{};
double g_gain = 1;
std::chrono::steady_clock::time_point g_nextCue{}; // guarded by g_mutex
// A missing or lost output device is retried on this cadence instead of
// silencing the radar until the game restarts.
constexpr auto ReopenDelay = std::chrono::seconds(2);
// Cancellation is serviced within this wait even when the game pauses.
constexpr DWORD CancelPollMs = 10;

void Trace(const Radar::Events& events, const CueSource& source, const char* stage,
	const char* reason, unsigned result = 0, uint64_t audioMs = 0) {
	const uint64_t now = GetTickCount64();
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, events[i], source, stage, reason, now, result, audioMs);
}
// Caller holds g_mutex. Every pending event keeps its original source.
void DropPending(const char* reason) {
	const uint64_t now = GetTickCount64();
	for (size_t i = 0; i < Radar::DirectionCount; ++i)
		if (g_pending.Pending()[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", reason, now);
	g_pending = {};
}
HWAVEOUT OpenDevice(HANDLE done) {
	WAVEFORMATEX format{};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = Channels;
	format.nSamplesPerSec = SampleRate;
	format.wBitsPerSample = sizeof(int16_t) * 8;
	format.nBlockAlign = format.nChannels * sizeof(int16_t);
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	HWAVEOUT device = nullptr;
	return waveOutOpen(&device, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(done), 0, CALLBACK_EVENT) == MMSYSERR_NOERROR ? device : nullptr;
}
// One taken direction's cue, its qualifiers from the source it was pushed with.
std::vector<int16_t> Cue(const Radar::Events& events, const CueSource& source) {
	Radar::Marks marks{}, small{};
	for (size_t i = 0; i < events.size(); ++i) {
		marks[i] = source.unexplored && events[i] != Radar::State::Unknown;
		small[i] = source.small && events[i] != Radar::State::Unknown;
	}
	return Synthesize(events, marks, g_gain, source.frame, small);
}
// Plays one sound to completion or cancellation. False means the device failed.
// `events` names the radar reading it is, for the trace; none for the wall bump.
bool PlayOne(HWAVEOUT device, HANDLE done, std::vector<int16_t>& pcm, const Radar::Events& events, const CueSource& source, uint64_t generation) {
	WAVEHDR header{};
	header.lpData = reinterpret_cast<char*>(pcm.data());
	header.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(int16_t));
	const auto prepared = waveOutPrepareHeader(device, &header, sizeof(header));
	if (prepared != MMSYSERR_NOERROR) { Trace(events, source, "failed", "prepare-header", prepared); return false; }
	const auto written = waveOutWrite(device, &header, sizeof(header));
	if (written != MMSYSERR_NOERROR) {
		Trace(events, source, "failed", "write-buffer", written);
		waveOutUnprepareHeader(device, &header, sizeof(header));
		return false;
	}
	const uint64_t submitted = GetTickCount64();
	Trace(events, source, "submitted", "waveOutWrite-ok");
	const auto wait = WaveOut::Await(header, done, Channels * SampleRate * sizeof(int16_t), CancelPollMs,
		[&] { return g_running && generation == g_generation; });
	if (wait != WaveOut::Wait::Done) waveOutReset(device);
	const bool returned = wait == WaveOut::Wait::Done, stalled = wait == WaveOut::Wait::Stalled;
	Trace(events, source, returned ? "completed" : stalled ? "failed" : "interrupted",
		returned ? "buffer-returned" : stalled ? "stalled" : "cancelled", 0, GetTickCount64() - submitted);
	waveOutUnprepareHeader(device, &header, sizeof(header));
	return !stalled;
}
void Worker(const D2RL::PluginContext* context) {
	HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!done) { AsyncLog::Warn(context, "[radar-audio] Completion event unavailable; radar audio disabled."); return; }
	bool warned = false;
	while (g_running) {
		HWAVEOUT device = OpenDevice(done);
		if (!device) {
			if (!warned) AsyncLog::Warn(context, "[radar-audio] Stereo output unavailable; retrying.");
			warned = true;
			std::unique_lock lock(g_mutex);
			g_wake.wait_for(lock, ReopenDelay, [] { return !g_running.load(); });
			continue;
		}
		warned = false;
		g_available = true;
		AsyncLog::Info(context, "[radar-audio] Synthesized stereo cues ready.");
		const char* exitReason = "stopped";
		while (g_running) {
			Radar::Events events{};
			CueSource source{};
			uint64_t generation = 0;
			double pan = 0;
			bool bump = false;
			{
				std::unique_lock lock(g_mutex);
				g_wake.wait(lock, [] { return !g_running || !g_pending.Empty(); });
				if (!g_running) break;
				g_wake.wait_until(lock, g_nextCue, [] { return !g_running.load(); });
				if (!g_running) break;
				if (g_pending.Empty()) continue;
				bump = g_pending.TakeBump(&pan, &source);
				if (!bump) events = g_pending.Take(&source);
				g_playing = events;
				g_playingSource = source;
				generation = g_generation;
			}
			if (generation != g_generation) {
				Trace(events, source, "dropped", "cancelled-before-submit");
				std::lock_guard lock(g_mutex);
				g_playing = {};
				continue;
			}
			auto pcm = bump ? SynthesizeBump(pan, g_gain) : Cue(events, source);
			SoundVolume::Scale(pcm, SoundVolume::Of(SoundVolume::Family::Radar).Factor());
			const bool ok = PlayOne(device, done, pcm, events, source, generation);
			{
				std::lock_guard lock(g_mutex);
				g_nextCue = std::chrono::steady_clock::now() + std::chrono::milliseconds(CueGapMs);
				g_playing = {};
			}
			if (!ok) { exitReason = "device-failed"; break; }
		}
		g_available = false;
		{
			std::lock_guard lock(g_mutex);
			DropPending(exitReason);
			g_playing = {};
		}
		waveOutReset(device);
		waveOutClose(device);
		if (g_running) AsyncLog::Warn(context, "[radar-audio] Output device failed; reopening.");
	}
	CloseHandle(done);
}
}

bool Start(const D2RL::PluginContext* context, double gain) {
	if (g_thread.joinable()) return false;
	g_context = context;
	g_gain = std::clamp(gain, 0.0, 1.0);
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}
void Play(const Radar::Events& events, CueSource source, const Radar::Marks& marks, const Radar::Marks& small) {
	std::lock_guard lock(g_mutex);
	if (!g_running || !g_available) {
		Trace(events, source, "dropped", "audio-unavailable");
		return;
	}
	if (!ModSounds::Enabled()) {
		Trace(events, source, "dropped", "mod-sounds-off");
		return;
	}
	// Bounded coalescing: retain the latest state of each direction, never a
	// backlog of obsolete cues. Directions play successively with a silent gap.
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown && g_pending.Pending()[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", "superseded", GetTickCount64());
	g_pending.Push(events, source, marks, small);
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, events[i], g_pending.Source(i), "queued", "generated", GetTickCount64());
	g_wake.notify_one();
}
void Bump(double pan, CueSource source) {
	std::lock_guard lock(g_mutex);
	if (!g_running || !g_available || !ModSounds::Enabled()) return;
	g_pending.PushBump(pan, source);
	g_wake.notify_one();
}
void Retain(const Radar::Events& states, const Radar::Marks& marks) {
	std::lock_guard lock(g_mutex);
	for (size_t i = 0; i < states.size(); ++i)
		if (g_pending.Pending()[i] != Radar::State::Unknown
			&& (g_pending.Pending()[i] != states[i] || g_pending.Source(i).unexplored != marks[i]))
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", "state-changed", GetTickCount64());
	g_pending.Retain(states, marks);
}
void Silence(const char* reason) {
	std::lock_guard lock(g_mutex);
	DropPending(reason);
	Trace(g_playing, g_playingSource, "cancel-requested", reason);
	++g_generation;
	g_playing = {};
}
void Stop() {
	{
		std::lock_guard lock(g_mutex);
		g_running = false;
		++g_generation;
	}
	g_wake.notify_all();
	if (g_thread.joinable()) g_thread.join();
	g_available = false;
	std::lock_guard lock(g_mutex);
	g_pending = {};
	g_playing = {};
	g_context = nullptr;
}
bool Available() { return g_available; }
}
