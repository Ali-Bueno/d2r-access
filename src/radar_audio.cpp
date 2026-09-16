#include "radar_audio.h"
#include "async_log.h"
#include "radar_trace.h"

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
std::thread g_thread;
std::atomic<bool> g_running{false}, g_available{false};
std::atomic<uint64_t> g_generation{0};
CueQueue g_pending;
Radar::Events g_playing{};
const D2RL::PluginContext* g_context = nullptr;
CueSource g_playingSource{};
uint64_t g_playingStarted = 0;
std::array<Radar::CueFailure, Radar::DirectionCount> g_failed{};
std::chrono::steady_clock::time_point g_nextCue{}; // guarded by g_mutex

void Trace(const Radar::Events& events, const CueSource& source, const char* stage,
	const char* reason, unsigned result = 0, uint64_t audioMs = 0) {
	const uint64_t now = GetTickCount64();
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, events[i], source, stage, reason, now, result, audioMs);
}
// Caller holds g_mutex. Every pending event keeps its original source even if
// another direction is queued, invalidated, or replaced before playback.
void DropPending(const char* reason) {
	const uint64_t now = GetTickCount64();
	for (size_t i = 0; i < Radar::DirectionCount; ++i)
		if (g_pending.Pending()[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", reason, now);
	g_pending = {};
}
// Caller holds g_mutex. Retry only cues that never started. A partially played
// chirp is already an announcement; repeating it produces audible stuttering.
// Old events cannot overwrite a newer failure.
void Failed(const Radar::Events& events, const CueSource& source, bool submitted = false) {
	if (source.audition) return;
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown && source.eventMs > g_failed[i].eventMs)
			g_failed[i] = {events[i], source.eventMs, submitted};
}

void Worker(const D2RL::PluginContext* context) {
	HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	HWAVEOUT device = nullptr;
	WAVEFORMATEX format{};
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = Channels;
	format.nSamplesPerSec = SampleRate;
	format.wBitsPerSample = sizeof(int16_t) * 8;
	format.nBlockAlign = format.nChannels * sizeof(int16_t);
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
	if (!done || waveOutOpen(&device, WAVE_MAPPER, &format, reinterpret_cast<DWORD_PTR>(done), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
		if (done) CloseHandle(done);
		AsyncLog::Warn(context, "[radar-audio] Stereo output unavailable.");
		return;
	}
	g_available = true;
	AsyncLog::Info(context, "[radar-audio] Synthesized stereo tin/tock ready.");
	const char* exitReason = "stopped";
	while (g_running) {
		Radar::Events events{};
		CueSource source{};
		uint64_t generation = 0;
		{
			std::unique_lock lock(g_mutex);
			g_wake.wait(lock, [] { return !g_running || !g_pending.Empty(); });
			if (!g_running) break;
			g_wake.wait_until(lock, g_nextCue, [] { return !g_running; });
			if (!g_running) break;
			if (g_pending.Empty()) continue;
			events = g_pending.Take(&source);
			g_playing = events;
			g_playingSource = source;
			g_playingStarted = GetTickCount64();
			generation = g_generation;
		}
		auto pcm = Synthesize(events);
		if (!g_running || generation != g_generation) {
			Trace(events, source, "dropped", "cancelled-before-submit");
			std::lock_guard lock(g_mutex);
			Failed(events, source);
			g_playing = {};
			continue;
		}
		WAVEHDR header{};
		header.lpData = reinterpret_cast<char*>(pcm.data());
		header.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(int16_t));
		const auto prepared = waveOutPrepareHeader(device, &header, sizeof(header));
		if (prepared != MMSYSERR_NOERROR) {
			Trace(events, source, "failed", "prepare-header", prepared);
			exitReason = "device-failed";
			break;
		}
		const auto written = waveOutWrite(device, &header, sizeof(header));
		if (written != MMSYSERR_NOERROR) {
			Trace(events, source, "failed", "write-buffer", written);
			exitReason = "device-failed";
			waveOutUnprepareHeader(device, &header, sizeof(header));
			break;
		}
		const uint64_t submitted = GetTickCount64();
		Trace(events, source, "submitted", "waveOutWrite-ok");
		// Only this worker touches the device. Cancellation is serviced within
		// 10 ms even when the game pauses; never wait on audio in a UI callback.
		constexpr DWORD CancelPollMs = 10;
		while (!(header.dwFlags & WHDR_DONE) && g_running && generation == g_generation) WaitForSingleObject(done, CancelPollMs);
		const bool interrupted = !(header.dwFlags & WHDR_DONE);
		if (interrupted) waveOutReset(device);
		Trace(events, source, interrupted ? "interrupted" : "completed",
			interrupted ? "cancelled" : "buffer-returned", 0, GetTickCount64() - submitted);
		waveOutUnprepareHeader(device, &header, sizeof(header));
		{
			std::lock_guard lock(g_mutex);
			if (interrupted) Failed(events, source, true);
			g_nextCue = std::chrono::steady_clock::now() + std::chrono::milliseconds(CueGapMs);
			g_playing = {};
		}
	}
	g_available = false;
	{
		std::lock_guard lock(g_mutex);
		DropPending(exitReason);
		g_playing = {};
	}
	waveOutReset(device);
	waveOutClose(device);
	CloseHandle(done);
}
}

bool Start(const D2RL::PluginContext* context) {
	if (g_thread.joinable()) return false;
	g_context = context;
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}
void Play(const Radar::Events& events, CueSource source) {
	std::lock_guard lock(g_mutex);
	if (!g_running || !g_available) {
		Trace(events, source, "dropped", "audio-unavailable");
		return;
	}
	// Bounded coalescing: retain the latest state of each direction, never a
	// backlog of obsolete cues. Directions play successively with a silent gap.
	for (size_t i = 0; i < events.size(); ++i)
		if (events[i] != Radar::State::Unknown && g_pending.Pending()[i] != Radar::State::Unknown)
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", "superseded", GetTickCount64());
	g_pending.Push(events, source);
	Trace(events, source, "queued", "generated");
	g_wake.notify_one();
}
void Validate(const Radar::Events& observations, const Radar::Events& audible) {
	std::lock_guard lock(g_mutex);
	for (size_t i = 0; i < observations.size(); ++i)
		if (g_pending.Pending()[i] != Radar::State::Unknown && g_pending.Pending()[i] != observations[i]) {
			RadarTrace::Audio(g_context, i, g_pending.Pending()[i], g_pending.Source(i), "dropped", "observation-changed", GetTickCount64());
			Radar::Events dropped{}; dropped[i] = g_pending.Pending()[i];
			Failed(dropped, g_pending.Source(i));
		}
	g_pending.Retain(observations);
	for (size_t i = 0; i < g_playing.size(); ++i) {
		if (g_playing[i] != Radar::State::Unknown && g_playing[i] != audible[i]) {
			Trace(g_playing, g_playingSource, "cancel-requested", "observation-changed");
			++g_generation;
			g_playing = {};
			break;
		}
	}
}
void Silence(const char* reason) {
	std::lock_guard lock(g_mutex);
	DropPending(reason);
	Trace(g_playing, g_playingSource, "cancel-requested", reason);
	++g_generation;
	g_playing = {};
}
void Stop() {
	g_running = false;
	Silence("stopped");
	g_wake.notify_one();
	if (g_thread.joinable()) g_thread.join();
	g_context = nullptr;
}
bool Available() { return g_available; }
Radar::AudioLoad Load() {
	std::lock_guard lock(g_mutex);
	const auto gap = std::chrono::ceil<std::chrono::milliseconds>(g_nextCue - std::chrono::steady_clock::now()).count();
	auto result = g_pending.Playback(g_playing, GetTickCount64() - g_playingStarted, static_cast<uint64_t>(std::max<int64_t>(0, gap)));
	result.failed = g_failed;
	g_failed = {};
	return result;
}
}
