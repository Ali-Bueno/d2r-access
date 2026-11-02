#include "charge_chord.h"
#include "async_log.h"
#include "charge_chord_synth.h"
#include "item_wave.h"
#include "mod_sounds.h"
#include "process_lifetime.h"
#include "sound_volume.h"
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace D2RAccess::ChargeChord {
namespace {
constexpr DWORD BlockMs = 20; // one buffer of the stream
constexpr UINT32 QueuedBlocks = 3; // a strike is heard within about this many blocks
constexpr DWORD IdleMs = 100; // the worker's sleep while silent; a volume change is heard within this
std::mutex g_mutex;
std::condition_variable g_wake;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
std::atomic<bool> g_running{false};
std::atomic<int> g_mode{static_cast<int>(Mode::Struck)};
// Under g_mutex: what the game's thread asked for since the worker last looked.
int g_strike = 0, g_hold = 0;
bool g_pending = false;

void Worker(const D2RL::PluginContext* context) {
	const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	IXAudio2* engine = nullptr;
	IXAudio2MasteringVoice* master = nullptr;
	IXAudio2SourceVoice* voice = nullptr;
	try {
		const WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, ItemWave::SampleRate, ItemWave::SampleRate * sizeof(int16_t), sizeof(int16_t), 16, 0};
		HRESULT output = XAudio2Create(&engine);
		if (SUCCEEDED(output)) output = engine->CreateMasteringVoice(&master, 2, ItemWave::SampleRate);
		if (SUCCEEDED(output)) output = engine->CreateSourceVoice(&voice, &format);
		if (SUCCEEDED(output)) output = voice->Start();
		if (FAILED(output)) {
			AsyncLog::WarnF(context, "[charge-chord] unavailable HRESULT=%08lx", output);
		} else {
			Synth synth(ItemWave::SampleRate);
			// One more block than may be queued, so the one written next has always played.
			std::array<std::vector<int16_t>, QueuedBlocks + 1> blocks;
			for (auto& block : blocks) block.resize(ItemWave::SampleRate * BlockMs / 1000);
			size_t next = 0;
			while (g_running) {
				XAUDIO2_VOICE_STATE state{};
				voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				int strike = 0, hold = 0;
				{
					std::unique_lock lock(g_mutex);
					const DWORD wait = synth.Silent() ? IdleMs : state.BuffersQueued >= QueuedBlocks ? BlockMs / 2 : 0;
					if (wait) g_wake.wait_for(lock, std::chrono::milliseconds(wait), [] { return g_pending || !g_running; });
					strike = std::exchange(g_strike, 0);
					hold = g_hold;
					g_pending = false;
				}
				if (!ModSounds::Enabled()) {
					if (!synth.Silent()) { synth = Synth(ItemWave::SampleRate); voice->FlushSourceBuffers(); }
					continue;
				}
				if (FAILED(master->SetVolume(static_cast<float>(SoundVolume::Of(SoundVolume::Family::Charges).Factor())))) break;
				synth.Hold(hold);
				if (strike) synth.Strike(strike);
				voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				if (synth.Silent() || state.BuffersQueued >= QueuedBlocks) continue;
				auto& block = blocks[next++ % blocks.size()];
				synth.Render(block);
				XAUDIO2_BUFFER buffer{};
				buffer.AudioBytes = static_cast<UINT32>(block.size() * sizeof(int16_t));
				buffer.pAudioData = reinterpret_cast<const BYTE*>(block.data());
				if (FAILED(voice->SubmitSourceBuffer(&buffer))) { AsyncLog::Warn(context, "[charge-chord] could not play"); break; }
			}
		}
	} catch (...) { AsyncLog::Warn(context, "[charge-chord] Worker stopped after an audio error."); }
	if (voice) voice->DestroyVoice();
	if (master) master->DestroyVoice();
	if (engine) engine->Release();
	if (SUCCEEDED(com)) CoUninitialize();
}

// Wakes the worker for a change, unless `change` reports there was none.
template <class Change>
void Ask(Change change) {
	{
		std::lock_guard lock(g_mutex);
		if (!change()) return;
		g_pending = true;
	}
	g_wake.notify_one();
}
}

Mode CurrentMode() { return static_cast<Mode>(g_mode.load()); }
int ModeLevel() { return g_mode; }
void SetModeLevel(int level) { g_mode = std::clamp(level, 0, static_cast<int>(Mode::Count) - 1); }

bool Start(const D2RL::PluginContext* context) {
	if (!context || g_thread.joinable()) return false;
	g_running = true;
	try { g_thread = std::thread(Worker, context); }
	catch (...) { g_running = false; return false; }
	return true;
}
void Strike(int notes) {
	Ask([notes] { g_strike = std::max(g_strike, std::min(notes, NoteCount)); return notes > 0; });
}
void Hold(int notes) {
	const int wanted = std::clamp(notes, 0, NoteCount);
	Ask([wanted] { return std::exchange(g_hold, wanted) != wanted; });
}
void Preview() { Strike(NoteCount); }
void Stop() {
	g_running = false;
	g_wake.notify_one();
	if (g_thread.joinable()) g_thread.join();
	std::lock_guard lock(g_mutex);
	g_strike = g_hold = 0;
	g_pending = false;
}
}
