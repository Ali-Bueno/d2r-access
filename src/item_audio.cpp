#include "item_audio.h"
#include "item_wave.h"
#include "item_spatial.h"
#include "async_log.h"
#include "process_lifetime.h"
#include "mod_sounds.h"
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

extern "C" IMAGE_DOS_HEADER __ImageBase; // this plugin's own module, which carries the sounds

namespace D2RAccess::ItemAudio {
namespace {
using namespace GroundItems;
constexpr DWORD UpdateMs = 20; // audio-only updates; never block the game's UI thread
constexpr float GainSmoothing = 0.25f; // approximately 80 ms fade-in/position changes
constexpr DWORD PlaybackCheckMs = 250; // one progress record per source, after fade-in
constexpr DWORD ExplorationExpiryMs = 2 * SnapshotExpiryMs; // terrain publishes every 250 ms
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false}, g_available{false};
std::vector<Voice> g_pending;
uint64_t g_updated = 0;
std::vector<Voice> g_explorationPending;
uint64_t g_explorationUpdated = 0;
struct OneShotRequest { Voice voice; uint64_t generation; };
// A finite cue on its own voice; a newer request replaces the one still playing.
struct OneShotChannel { std::optional<OneShotRequest> pending; std::atomic<uint64_t> generation{0}; };
OneShotChannel g_arrival, g_discovery;
using Samples = std::array<std::vector<int16_t>, static_cast<size_t>(Sound::Count)>;
struct Playing {
	uint32_t id = 0;
	Sound sound{};
	IXAudio2SourceVoice* source = nullptr;
	float gain = 0;
	uint64_t started = 0;
	uint64_t generation = 0;
	Voice target;
	bool checked = false;
	void Stop() { if (source) { source->DestroyVoice(); source = nullptr; } gain = 0; }
};
bool StartVoice(Playing& active, const Voice& target, const std::vector<int16_t>& sample,
	uint64_t generation, IXAudio2* engine, const WAVEFORMATEX& format, const D2RL::PluginContext* context) {
	const bool loop = !IsOneShot(target.sound);
	const HRESULT created = engine->CreateSourceVoice(&active.source, &format);
	if (FAILED(created)) {
		AsyncLog::WarnF(context, "[item-audio] source failed HRESULT=%08lx", created);
		g_running = false;
		return false;
	}
	active.id = target.id; active.sound = target.sound; active.target = target;
	active.started = GetTickCount64(); active.checked = false; active.generation = generation;
	HRESULT submitted = active.source->SetVolume(0);
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(sample.size() * sizeof(int16_t));
	buffer.pAudioData = reinterpret_cast<const BYTE*>(sample.data());
	buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
	if (SUCCEEDED(submitted)) submitted = active.source->SubmitSourceBuffer(&buffer);
	if (SUCCEEDED(submitted)) submitted = active.source->Start();
	if (FAILED(submitted)) {
		AsyncLog::WarnF(context, "[item-audio] start failed id=%u HRESULT=%08lx", target.id, submitted);
		active.Stop(); g_running = false;
		return false;
	}
	AsyncLog::InfoF(context, "[item-audio] started id=%u sound=%s right=%.1f front=%.1f loop=%u",
		target.id, SoundFiles[static_cast<size_t>(target.sound)].name, target.right, target.front, loop);
	return true;
}
void ApplySpatial(Playing& active, IXAudio2MasteringVoice* master, const D2RL::PluginContext* context) {
	const auto& target = active.target;
	const auto matrix = ItemSpatial::StereoMatrix(target);
	active.gain += (target.gain - active.gain) * GainSmoothing;
	HRESULT applied = active.source->SetOutputMatrix(master, 1, 2, matrix.data());
	if (SUCCEEDED(applied)) applied = active.source->SetFrequencyRatio(target.pitch);
	if (SUCCEEDED(applied)) applied = active.source->SetVolume(active.gain);
	if (FAILED(applied)) {
		AsyncLog::WarnF(context, "[item-audio] spatial update failed id=%u HRESULT=%08lx", target.id, applied);
		g_running = false;
		return;
	}
	if (!active.checked && GetTickCount64() - active.started >= PlaybackCheckMs) {
		XAUDIO2_VOICE_STATE state{};
		active.source->GetState(&state);
		AsyncLog::InfoF(context, "[item-audio] progress id=%u samples=%llu queued=%u gain=%.3f matrix=%.3f,%.3f pitch=%.3f",
			target.id, state.SamplesPlayed, state.BuffersQueued, active.gain, matrix[0], matrix[1], target.pitch);
		active.checked = true;
	}
}
void Request(OneShotChannel& channel, const Voice& voice) {
	if (!ModSounds::Enabled()) return;
	std::lock_guard lock(g_mutex);
	channel.pending = OneShotRequest{voice, ++channel.generation};
	g_wake.notify_one();
}
// The mix share of a requested one-shot, or of the one still playing.
float OneShotGain(const Playing& playing, const std::optional<OneShotRequest>& requested, const OneShotChannel& channel) {
	if (requested && requested->generation == channel.generation.load()) return requested->voice.gain;
	if (playing.source && playing.generation == channel.generation.load()) return std::max(playing.target.gain, playing.gain);
	return 0;
}
void ServeOneShot(Playing& playing, const std::optional<OneShotRequest>& requested, const OneShotChannel& channel,
	const Samples& samples, IXAudio2* engine, const WAVEFORMATEX& format, IXAudio2MasteringVoice* master,
	const D2RL::PluginContext* context) {
	if (playing.source && playing.generation != channel.generation.load()) playing.Stop();
	if (requested && requested->generation == channel.generation.load()) {
		playing.Stop();
		const auto& sample = samples[static_cast<size_t>(requested->voice.sound)];
		if (!sample.empty()) (void)StartVoice(playing, requested->voice, sample, requested->generation, engine, format, context);
	}
	if (!g_running || !playing.source) return;
	XAUDIO2_VOICE_STATE state{};
	playing.source->GetState(&state);
	if (!state.BuffersQueued) {
		AsyncLog::InfoF(context, "[item-audio] completed one-shot sound=%s id=%u samples=%llu",
			SoundFiles[static_cast<size_t>(playing.sound)].name, playing.id, state.SamplesPlayed);
		playing.Stop();
	}
	else ApplySpatial(playing, master, context);
}
void Worker(const D2RL::PluginContext* context) {
	const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IXAudio2* engine = nullptr;
	IXAudio2MasteringVoice* master = nullptr;
	Samples samples;
	std::array<Playing, MaxVoices> playing{};
	Playing exploration, arrival, discovery;
	try {
		// Keep all sample memory alive until its source voices have been destroyed.
		const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
		size_t loaded = 0;
		for (size_t i = 0; i < samples.size(); ++i) {
			const auto& file = SoundFiles[i];
			samples[i] = ItemWave::LoadEmbedded(module, file.folder, file.name);
			const float scale = ItemWave::Normalize(samples[i]);
			if (scale > 0) {
				++loaded;
				AsyncLog::InfoF(context, "[item-audio] normalized %s/%s gain_db=%.1f peak=%.2f", file.folder, file.name, 20 * std::log10(scale), ItemWave::LoopPeak);
			} else {
				samples[i].clear();
				AsyncLog::WarnF(context, "[item-audio] Missing/invalid/silent embedded sound: %s/%s", file.folder, file.name);
			}
		}
		HRESULT output = loaded ? XAudio2Create(&engine) : E_FAIL;
		if (SUCCEEDED(output)) output = engine->CreateMasteringVoice(&master, 2, ItemWave::SampleRate);
		if (FAILED(output)) {
			AsyncLog::WarnF(context, "[item-audio] Spatial output unavailable HRESULT=%08lx; item speech remains enabled.", output);
		} else {
			WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, ItemWave::SampleRate, ItemWave::SampleRate * sizeof(int16_t), sizeof(int16_t), 16, 0};
			g_available = true;
			AsyncLog::InfoF(context, "[item-audio] %zu sounds loaded; X3DAudio screen axes, rear pitch %.2f.", loaded, RearPitch);
			while (g_running) {
				std::vector<Voice> targets, explorationTargets;
				std::optional<OneShotRequest> requestedArrival, requestedDiscovery;
				{
					std::unique_lock lock(g_mutex);
					g_wake.wait_for(lock, std::chrono::milliseconds(UpdateMs));
					if (g_running && GetTickCount64() - g_updated <= SnapshotExpiryMs) targets = g_pending;
					if (g_running && GetTickCount64() - g_explorationUpdated <= ExplorationExpiryMs)
						explorationTargets = g_explorationPending;
					requestedArrival = std::exchange(g_arrival.pending, std::nullopt);
					requestedDiscovery = std::exchange(g_discovery.pending, std::nullopt);
				}
				const Voice* room = nullptr;
				for (const auto& target : explorationTargets)
					if (target.sound == Sound::UnexploredRoom) { room = &target; break; }
				float totalGain = 0;
				for (const auto& target : targets) {
					const auto active = std::find_if(playing.begin(), playing.end(), [&](const Playing& p) {
						return p.source && SameSource(p.target, target);
					});
					totalGain += std::max(target.gain, active == playing.end() ? 0.0f : active->gain);
				}
				if (room) totalGain += std::max(room->gain, exploration.source ? exploration.gain : 0.0f);
				totalGain += OneShotGain(arrival, requestedArrival, g_arrival) + OneShotGain(discovery, requestedDiscovery, g_discovery);
				const float mixVolume = totalGain > MixGainLimit ? MixGainLimit / totalGain : 1.0f;
				if (FAILED(master->SetVolume(mixVolume))) { g_running = false; break; }
				for (auto& active : playing) {
					if (!active.source) continue;
					const auto found = std::find_if(targets.begin(), targets.end(), [&](const Voice& target) { return SameSource(active.target, target); });
					if (found == targets.end()) {
						AsyncLog::InfoF(context, "[item-audio] stopped id=%u", active.id);
						active.Stop();
					}
				}
				for (const auto& target : targets) {
					const auto sound = static_cast<size_t>(target.sound);
					if (sound >= samples.size() || samples[sound].empty() || IsOneShot(target.sound)) continue;
					auto active = std::find_if(playing.begin(), playing.end(), [&](const Playing& p) { return p.source && SameSource(p.target, target); });
					if (active == playing.end()) {
						active = std::find_if(playing.begin(), playing.end(), [](const Playing& p) { return !p.source; });
						if (active == playing.end()) break;
						if (!StartVoice(*active, target, samples[sound], 0, engine, format, context)) break;
					}
					active->target = target;
					ApplySpatial(*active, master, context);
					if (!g_running) break;
				}
				if (!g_running) break;
				if (exploration.source && (!room || exploration.id != room->id || exploration.sound != room->sound)) {
					AsyncLog::InfoF(context, "[item-audio] stopped exploration id=%u", exploration.id);
					exploration.Stop();
				}
				if (room && !samples[static_cast<size_t>(room->sound)].empty()) {
					if (!exploration.source)
						(void)StartVoice(exploration, *room, samples[static_cast<size_t>(room->sound)], 0,
							engine, format, context);
					if (exploration.source) { exploration.target = *room; ApplySpatial(exploration, master, context); }
				}
				if (!g_running) break;
				ServeOneShot(arrival, requestedArrival, g_arrival, samples, engine, format, master, context);
				if (!g_running) break;
				ServeOneShot(discovery, requestedDiscovery, g_discovery, samples, engine, format, master, context);
			}
		}
	} catch (...) { AsyncLog::Warn(context, "[item-audio] Worker stopped after an audio initialization/playback error."); }
	g_available = false;
	for (auto& voice : playing) voice.Stop();
	exploration.Stop(); arrival.Stop(); discovery.Stop();
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
void Update(std::vector<Voice> voices) {
	if (!ModSounds::Enabled()) voices.clear();
	std::lock_guard lock(g_mutex);
	g_pending = std::move(voices); g_updated = GetTickCount64();
	g_wake.notify_one();
}
void Silence() { Update({}); }
void UpdateExploration(std::vector<Voice> voices) {
	if (!ModSounds::Enabled()) voices.clear();
	std::lock_guard lock(g_mutex);
	g_explorationPending = std::move(voices); g_explorationUpdated = GetTickCount64();
	g_wake.notify_one();
}
void PlayExplorationArrival(Voice voice) {
	voice.sound = Sound::InsideUnexploredRoom;
	Request(g_arrival, voice);
}
void PlayDiscovery() {
	Voice voice{0, Sound::Discovered};
	voice.gain = LoopVolume; // centered and as near as a source can be, like the room arrival
	Request(g_discovery, voice);
}
void SilenceExploration() {
	std::lock_guard lock(g_mutex);
	g_explorationPending.clear(); g_explorationUpdated = GetTickCount64();
	g_arrival.pending.reset(); ++g_arrival.generation;
	g_wake.notify_one();
}
void Stop() {
	g_running = false;
	g_wake.notify_one();
	if (g_thread.joinable()) g_thread.join();
	Silence(); SilenceExploration();
}
bool Available() { return g_available; }
}
