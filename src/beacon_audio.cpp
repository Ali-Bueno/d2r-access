#include "beacon_audio.h"
#include "async_log.h"
#include "process_lifetime.h"
#include "mod_sounds.h"
#include "sound_volume.h"
#include "wave_out.h"

#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numbers>
#include <thread>

namespace D2RAccess::BeaconAudio {
namespace {
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false}, g_available{false};
std::atomic<uint64_t> g_generation{0};
const D2RL::PluginContext* g_context = nullptr;
double g_gain = 1, g_panScale = 1, g_pitchScale = 1;
struct Request { bool pending = false; Sound sound = Sound::Ping; Placement placement; };
Request g_pending;
constexpr auto ReopenDelay = std::chrono::seconds(2);
constexpr DWORD CancelPollMs = 10;

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
bool PlayOne(HWAVEOUT device, HANDLE done, std::vector<int16_t>& pcm, uint64_t generation) {
	WAVEHDR header{};
	header.lpData = reinterpret_cast<char*>(pcm.data());
	header.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(int16_t));
	if (waveOutPrepareHeader(device, &header, sizeof(header)) != MMSYSERR_NOERROR) return false;
	if (waveOutWrite(device, &header, sizeof(header)) != MMSYSERR_NOERROR) {
		waveOutUnprepareHeader(device, &header, sizeof(header));
		return false;
	}
	const auto wait = WaveOut::Await(header, done, Channels * SampleRate * sizeof(int16_t), CancelPollMs,
		[&] { return g_running && generation == g_generation; });
	if (wait != WaveOut::Wait::Done) waveOutReset(device);
	waveOutUnprepareHeader(device, &header, sizeof(header));
	return wait != WaveOut::Wait::Stalled;
}
void Worker(const D2RL::PluginContext* context) {
	HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!done) { AsyncLog::Warn(context, "[beacon-audio] Completion event unavailable; beacon audio disabled."); return; }
	bool warned = false;
	while (g_running) {
		HWAVEOUT device = OpenDevice(done);
		if (!device) {
			if (!warned) AsyncLog::Warn(context, "[beacon-audio] Stereo output unavailable; retrying.");
			warned = true;
			std::unique_lock lock(g_mutex);
			g_wake.wait_for(lock, ReopenDelay, [] { return !g_running.load(); });
			continue;
		}
		warned = false;
		g_available = true;
		AsyncLog::Info(context, "[beacon-audio] Beacon output ready.");
		bool failed = false;
		while (g_running && !failed) {
			Request request;
			uint64_t generation = 0;
			{
				std::unique_lock lock(g_mutex);
				g_wake.wait(lock, [] { return !g_running || g_pending.pending; });
				if (!g_running) break;
				request = g_pending;
				g_pending = {};
				generation = g_generation;
			}
			auto pcm = Synthesize(request.sound, request.placement, g_gain);
			SoundVolume::Scale(pcm, SoundVolume::Of(SoundVolume::Family::Guidance).Factor());
			if (generation != g_generation) continue;
			failed = !PlayOne(device, done, pcm, generation);
		}
		g_available = false;
		{
			std::lock_guard lock(g_mutex);
			g_pending = {};
		}
		waveOutReset(device);
		waveOutClose(device);
		if (g_running && failed) AsyncLog::Warn(context, "[beacon-audio] Output device failed; reopening.");
	}
	CloseHandle(done);
}
}

std::vector<int16_t> Synthesize(Sound sound, Placement placement, double gain) {
	// Sound design, deliberately unlike the sonar's tin/tock so a beacon is
	// never mistaken for a wall or an opening: a soft mid-register blip.
	// Stereo placement and the rear pitch follow the ground-item loops.
	constexpr double PingHz = 620, NoteMs = 80, GapMs = 60;
	constexpr double RearPitch = 0.94; // the ground-item loops' rear factor
	constexpr double Attack = 0.004, Release = 0.012, Decay = 0.035;
	constexpr double Harmonic = 2.0, HarmonicLevel = 0.15;
	constexpr double Level = 0.15, PcmPeak = 32767;
	// Note ladders: a rise confirms a lock, a fall confirms a cancel, and
	// arrival is the rise carried one step further.
	constexpr double Fifth = 1.5, Octave = 2.0;
	std::vector<double> notes;
	switch (sound) {
	case Sound::Locked: notes = {PingHz, PingHz * Fifth}; break;
	case Sound::Cleared: notes = {PingHz * Fifth, PingHz}; break;
	case Sound::Arrived: notes = {PingHz, PingHz * Fifth, PingHz * Octave}; break;
	default: notes = {PingHz}; break;
	}
	const auto scale = [](double value) { return std::isfinite(value) ? std::max(value, 0.0) : 1.0; };
	const double pan = std::clamp(placement.right * scale(placement.panScale), -1.0, 1.0);
	const double pitch = 1 + (RearPitch - 1) * std::clamp(-placement.front, 0.0, 1.0) * scale(placement.pitchScale);
	const double left = std::sqrt((1 - pan) / 2), right = std::sqrt((1 + pan) / 2);
	const double level = Level * std::clamp(gain, 0.0, 1.0);
	const double duration = NoteMs / 1000.0;
	const size_t frames = static_cast<size_t>(duration * SampleRate);
	std::vector<int16_t> pcm;
	for (size_t note = 0; note < notes.size(); ++note) {
		if (note) pcm.resize(pcm.size() + static_cast<size_t>(GapMs / 1000.0 * SampleRate) * Channels);
		const double hz = notes[note] * pitch;
		for (size_t frame = 0; frame < frames; ++frame) {
			const double time = static_cast<double>(frame) / SampleRate;
			const double phase = 2 * std::numbers::pi * hz * time;
			const double envelope = std::min(1.0, time / Attack) * std::min(1.0, (duration - time) / Release)
				* std::exp(-time / Decay);
			const double tone = level * envelope * (std::sin(phase) + HarmonicLevel * std::sin(phase * Harmonic));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * left, -1.0, 1.0) * PcmPeak));
			pcm.push_back(static_cast<int16_t>(std::clamp(tone * right, -1.0, 1.0) * PcmPeak));
		}
	}
	return pcm;
}

bool Start(const D2RL::PluginContext* context, double gain, double panScale, double pitchScale) {
	if (g_thread.joinable()) return false;
	g_context = context;
	g_gain = std::clamp(gain, 0.0, 1.0);
	g_panScale = panScale; g_pitchScale = pitchScale;
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}
void SetStrength(double panScale, double pitchScale) {
	std::lock_guard lock(g_mutex);
	g_panScale = panScale; g_pitchScale = pitchScale;
}
double PanScale() { std::lock_guard lock(g_mutex); return g_panScale; }
double PitchScale() { std::lock_guard lock(g_mutex); return g_pitchScale; }
void Play(Sound sound, Placement placement) {
	{
		std::lock_guard lock(g_mutex);
		if (!g_running || !g_available || !ModSounds::Enabled()) return;
		// A confirmation must never be dropped for a ping that is still waiting.
		if (g_pending.pending && g_pending.sound != Sound::Ping && sound == Sound::Ping) return;
		placement.panScale = g_panScale; placement.pitchScale = g_pitchScale;
		g_pending = {true, sound, placement};
	}
	g_wake.notify_one();
}
void Silence(const char* reason) {
	{
		std::lock_guard lock(g_mutex);
		g_pending = {};
		++g_generation;
	}
	AsyncLog::InfoF(g_context, "[beacon-audio] silenced reason=%s", reason);
	g_wake.notify_one();
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
	g_context = nullptr;
}
bool Available() { return g_available; }
}
