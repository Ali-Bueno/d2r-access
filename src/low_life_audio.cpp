#include "low_life_audio.h"
#include "async_log.h"
#include "ground_items.h"
#include "item_wave.h"
#include "mod_sounds.h"
#include "process_lifetime.h"
#include "sound_volume.h"
#include <windows.h>
#include <xaudio2.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase; // this plugin's own module, which carries the sounds

namespace D2RAccess::LowLifeAudio {
namespace {
constexpr DWORD UpdateMs = 100; // a volume change is heard within this
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false}, g_wanted{false};

XAUDIO2_BUFFER Buffer(const std::vector<int16_t>& samples, bool loop) {
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(int16_t));
	buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());
	if (loop) buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	return buffer;
}

void Worker(const D2RL::PluginContext* context) {
	const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IXAudio2* engine = nullptr;
	IXAudio2MasteringVoice* master = nullptr;
	IXAudio2SourceVoice* voice = nullptr;
	try {
		const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
		auto start = ItemWave::LoadEmbedded(module, "vitals", "low life start.wav");
		auto beat = ItemWave::LoadEmbedded(module, "vitals", "low life.wav");
		HRESULT output = ItemWave::Normalize(start) > 0 && ItemWave::Normalize(beat) > 0 ? XAudio2Create(&engine) : E_FAIL;
		if (SUCCEEDED(output)) output = engine->CreateMasteringVoice(&master, 2, ItemWave::SampleRate);
		if (FAILED(output)) {
			AsyncLog::WarnF(context, "[low-life] heartbeat unavailable HRESULT=%08lx", output);
		} else {
			const WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, ItemWave::SampleRate, ItemWave::SampleRate * sizeof(int16_t), sizeof(int16_t), 16, 0};
			bool playing = false;
			while (g_running) {
				{
					std::unique_lock lock(g_mutex);
					g_wake.wait_for(lock, std::chrono::milliseconds(UpdateMs));
				}
				// The level of the item loops, which the recordings are normalized with.
				const auto volume = GroundItems::LoopVolume * SoundVolume::Of(SoundVolume::Family::LowLife).Factor();
				if (FAILED(master->SetVolume(static_cast<float>(volume)))) break;
				const bool wanted = g_running && g_wanted.load() && ModSounds::Enabled();
				if (wanted == playing) continue;
				playing = wanted;
				if (voice) { voice->DestroyVoice(); voice = nullptr; }
				if (!wanted) { AsyncLog::Info(context, "[low-life] heartbeat stopped"); continue; }
				const auto first = Buffer(start, false), loop = Buffer(beat, true);
				if (FAILED(engine->CreateSourceVoice(&voice, &format)) || FAILED(voice->SubmitSourceBuffer(&first))
					|| FAILED(voice->SubmitSourceBuffer(&loop)) || FAILED(voice->Start())) {
					if (voice) { voice->DestroyVoice(); voice = nullptr; }
					AsyncLog::Warn(context, "[low-life] heartbeat could not start");
					continue;
				}
				AsyncLog::Info(context, "[low-life] heartbeat started");
			}
		}
	} catch (...) { AsyncLog::Warn(context, "[low-life] Worker stopped after an audio error."); }
	if (voice) voice->DestroyVoice();
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
void Play(bool on) {
	if (g_wanted.exchange(on) != on) g_wake.notify_one();
}
void Stop() {
	g_running = false;
	g_wanted = false;
	g_wake.notify_one();
	if (g_thread.joinable()) g_thread.join();
}
}
