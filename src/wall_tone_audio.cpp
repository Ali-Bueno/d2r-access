#include "wall_tone_audio.h"
#include "wall_tones.h"
#include "async_log.h"
#include "process_lifetime.h"

#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace D2RAccess::WallToneAudio {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto UpdateInterval = std::chrono::milliseconds(20);
// Twice the radar poll: a step between polls glides instead of clicking (Last Epoch mod, 25 ms).
constexpr double GlideMs = 25;
// A target older than this means the service stopped feeding it (menu, early return): fade to silence.
constexpr auto StaleAfter = std::chrono::milliseconds(400);
constexpr uint32_t SeedMultiplier = 0x9E3779B9u; // golden-ratio hash: a distinct noise per direction
constexpr int OutputChannels = 2;

std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false};
std::atomic<float> g_level{1.0f};
Gains g_targets{};
Clock::time_point g_updated{};

struct Voice {
	IXAudio2SourceVoice* source = nullptr;
	std::vector<float> loop;
	float gain = 0;
};

bool Create(Voice& voice, size_t index, IXAudio2* engine, IXAudio2MasteringVoice* master) {
	voice.loop = WallTones::BuildLoop(WallTones::BandHz[index], SeedMultiplier * static_cast<uint32_t>(index + 1));
	for (auto& sample : voice.loop) sample *= static_cast<float>(WallTones::ContactRms);
	WAVEFORMATEX format{WAVE_FORMAT_IEEE_FLOAT, 1, WallTones::SampleRate, WallTones::SampleRate * sizeof(float), sizeof(float), 32, 0};
	if (FAILED(engine->CreateSourceVoice(&voice.source, &format))) return false;
	const auto pan = WallTones::PanGains(WallTones::Pan(static_cast<Radar::Direction>(index)));
	const float matrix[OutputChannels] = {pan.left, pan.right};
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(voice.loop.size() * sizeof(float));
	buffer.pAudioData = reinterpret_cast<const BYTE*>(voice.loop.data());
	buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	// Silent voices keep looping, so a returning wall starts smoothly.
	return SUCCEEDED(voice.source->SetVolume(0)) && SUCCEEDED(voice.source->SetOutputMatrix(master, 1, OutputChannels, matrix))
		&& SUCCEEDED(voice.source->SubmitSourceBuffer(&buffer)) && SUCCEEDED(voice.source->Start());
}

void Worker(const D2RL::PluginContext* context) {
	const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IXAudio2* engine = nullptr;
	IXAudio2MasteringVoice* master = nullptr;
	std::array<Voice, Radar::DirectionCount> voices;
	try {
		HRESULT output = XAudio2Create(&engine);
		if (SUCCEEDED(output)) output = engine->CreateMasteringVoice(&master, OutputChannels, WallTones::SampleRate);
		if (FAILED(output)) {
			AsyncLog::WarnF(context, "[wall-tones] audio output unavailable HRESULT=%08lx", output);
		} else {
			bool ready = true;
			for (size_t i = 0; i < voices.size() && ready; ++i) ready = Create(voices[i], i, engine, master);
			if (!ready) AsyncLog::Warn(context, "[wall-tones] voice creation failed; wall tones stay silent.");
			else AsyncLog::Info(context, "[wall-tones] four voices playing (silent until a wall is near).");
			auto last = Clock::now();
			while (g_running && ready) {
				Gains targets;
				{
					std::unique_lock lock(g_mutex);
					g_wake.wait_for(lock, UpdateInterval);
					targets = Clock::now() - g_updated <= StaleAfter ? g_targets : Gains{};
				}
				const auto now = Clock::now();
				const double dtMs = std::chrono::duration<double, std::milli>(now - last).count();
				last = now;
				const float level = g_level.load();
				for (size_t i = 0; i < voices.size(); ++i) {
					auto& voice = voices[i];
					voice.gain = WallTones::Glide(voice.gain, targets[i], dtMs, GlideMs);
					const float volume = voice.gain * WallTones::Trim(static_cast<Radar::Direction>(i)) * level;
					if (FAILED(voice.source->SetVolume(volume))) { ready = false; break; }
				}
			}
		}
	} catch (...) { AsyncLog::Warn(context, "[wall-tones] Worker stopped after an audio error."); }
	for (auto& voice : voices) if (voice.source) voice.source->DestroyVoice();
	if (master) master->DestroyVoice();
	if (engine) engine->Release();
	if (SUCCEEDED(com)) CoUninitialize();
}
}

bool Start(const D2RL::PluginContext* context) {
	if (!context || g_thread.joinable()) return false;
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}

void Update(const Gains& gains) {
	std::lock_guard lock(g_mutex);
	for (size_t i = 0; i < gains.size(); ++i) g_targets[i] = std::clamp(gains[i], 0.0f, 1.0f);
	g_updated = Clock::now();
}

void SetLevel(float level) { g_level = std::clamp(level, 0.0f, 1.0f); }

void Silence() { Update({}); }

void Stop() {
	g_running = false;
	g_wake.notify_all();
	if (g_thread.joinable()) g_thread.join();
}
}
