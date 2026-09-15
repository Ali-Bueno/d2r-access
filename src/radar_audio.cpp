#include "radar_audio.h"
#include "async_log.h"

#include <windows.h>
#include <mmsystem.h>
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
	std::chrono::steady_clock::time_point nextCue{};
	while (g_running) {
		Radar::Events events{};
		uint64_t generation = 0;
		{
			std::unique_lock lock(g_mutex);
			g_wake.wait(lock, [] { return !g_running || !g_pending.Empty(); });
			if (!g_running) break;
			g_wake.wait_until(lock, nextCue, [] { return !g_running; });
			if (!g_running) break;
			if (g_pending.Empty()) continue;
			events = g_pending.Take();
			g_playing = events;
			generation = g_generation;
		}
		auto pcm = Synthesize(events);
		if (!g_running || generation != g_generation) continue;
		WAVEHDR header{};
		header.lpData = reinterpret_cast<char*>(pcm.data());
		header.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(int16_t));
		if (waveOutPrepareHeader(device, &header, sizeof(header)) != MMSYSERR_NOERROR) break;
		if (waveOutWrite(device, &header, sizeof(header)) != MMSYSERR_NOERROR) {
			waveOutUnprepareHeader(device, &header, sizeof(header));
			break;
		}
		// Only this worker touches the device. Cancellation is serviced within
		// 10 ms even when the game pauses; never wait on audio in a UI callback.
		constexpr DWORD CancelPollMs = 10;
		while (!(header.dwFlags & WHDR_DONE) && g_running && generation == g_generation) WaitForSingleObject(done, CancelPollMs);
		if (!(header.dwFlags & WHDR_DONE)) waveOutReset(device);
		waveOutUnprepareHeader(device, &header, sizeof(header));
		nextCue = std::chrono::steady_clock::now() + std::chrono::milliseconds(CueGapMs);
		{
			std::lock_guard lock(g_mutex);
			g_playing = {};
		}
	}
	g_available = false;
	waveOutReset(device);
	waveOutClose(device);
	CloseHandle(done);
}
}

bool Start(const D2RL::PluginContext* context) {
	if (g_thread.joinable()) return false;
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}
void Play(const Radar::Events& events) {
	if (!g_running || !g_available) return;
	std::lock_guard lock(g_mutex);
	// Bounded coalescing: retain the latest state of each direction, never a
	// backlog of obsolete cues. Directions play successively with a silent gap.
	g_pending.Push(events);
	g_wake.notify_one();
}
void Validate(const Radar::Events& observations) {
	std::lock_guard lock(g_mutex);
	g_pending.Retain(observations);
	for (size_t i = 0; i < g_playing.size(); ++i) {
		if (g_playing[i] != Radar::State::Unknown && g_playing[i] != observations[i]) {
			++g_generation;
			g_playing = {};
			break;
		}
	}
}
void Silence() {
	std::lock_guard lock(g_mutex);
	++g_generation;
	g_pending = {};
	g_playing = {};
}
void Stop() {
	g_running = false;
	Silence();
	g_wake.notify_one();
	if (g_thread.joinable()) g_thread.join();
}
bool Available() { return g_available; }
}
