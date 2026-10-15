#include "entity_cue_audio.h"
#include "async_log.h"
#include "process_lifetime.h"
#include "mod_sounds.h"
#include "sound_volume.h"
#include "wave_out.h"

#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace D2RAccess::EntityCueAudio {
namespace {
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false}, g_available{false}, g_classLayers{true};
std::atomic<uint64_t> g_generation{0};
double g_gain = 1, g_panScale = 1, g_pitchScale = 1;
struct Request { bool pending = false; Sound sound = Sound::Npc; Placement placement; };
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
	if (!done) {
		AsyncLog::Warn(context, "[entity-audio] Completion event unavailable; entity audio disabled.");
		g_running = false;
		return;
	}
	bool warned = false;
	while (g_running) {
		HWAVEOUT device = OpenDevice(done);
		if (!device) {
			if (!warned) AsyncLog::Warn(context, "[entity-audio] Stereo output unavailable; retrying.");
			warned = true;
			std::unique_lock lock(g_mutex);
			g_wake.wait_for(lock, ReopenDelay, [] { return !g_running.load(); });
			continue;
		}
		warned = false;
		g_available = true;
		AsyncLog::Info(context, "[entity-audio] Entity output ready.");
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
			SoundVolume::Scale(pcm, SoundVolume::Of(SoundVolume::Family::Enemies).Factor());
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
		if (g_running && failed) AsyncLog::Warn(context, "[entity-audio] Output device failed; reopening.");
	}
	CloseHandle(done);
}
}

bool Start(const D2RL::PluginContext* context, double gain, double panScale, double pitchScale) {
	if (g_thread.joinable()) return false;
	g_gain = std::isfinite(gain) ? std::clamp(gain, 0.0, 1.0) : 0;
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
bool ClassLayers() { return g_classLayers; }
void SetClassLayers(bool on) { g_classLayers = on; }

void Play(Sound sound, Placement placement) {
	{
		std::lock_guard lock(g_mutex);
		if (!g_running || !g_available || !ModSounds::Enabled()) return;
		const auto feedback = [](Sound cue) {
			return cue == Sound::Reach || cue == Sound::Pickup || cue == Sound::Edge || cue == Sound::NpcAlert || cue == Sound::Corpse;
		};
		if (g_pending.pending && feedback(g_pending.sound) && !feedback(sound)) return;
		placement.panScale = g_panScale; placement.pitchScale = g_pitchScale; placement.classLayers = g_classLayers;
		g_pending = {true, sound, placement};
	}
	g_wake.notify_one();
}

void Silence() {
	{
		std::lock_guard lock(g_mutex);
		g_pending = {};
		++g_generation;
	}
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
}

bool Available() { return g_available; }
}
