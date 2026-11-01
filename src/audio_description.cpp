#include "audio_description.h"

#include "async_log.h"
#include "generated/level_flags.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "settings_file.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <optional>

namespace D2RAccess::AudioDescription {

namespace {
using Clock = std::chrono::steady_clock;

constexpr std::string_view English = "enUS";
// A Videos list choice older than this when a movie starts was not what started it.
constexpr auto ChoiceLifetime = std::chrono::seconds(5);
// Seconds between `MoviePlayerMessage:Play` and the video's first frame, by edition: screenshots
// matched to the intro's frames, 1.05 s (2021) and 0.70 s (classic), three each (live, 2026-11-01).
constexpr double RemasteredStartDelay = 1.05;
constexpr double ClassicStartDelay    = 0.70;
// A video started in play: `InGameMoviePlayerPanel/ProgressionHelper` is a widget (0x88 bytes) plus
// its state (+0x88: 0 idle, 1 playing, 2 finished) and the video's index in `Videos` order (+0x8C).
// RE of its message handler on 3.3.93787 (docs/cinematics-audio-description.md).
constexpr size_t ProgressionStateOffset = 0x88;
constexpr size_t ProgressionIndexOffset = 0x8C;
constexpr int32_t ProgressionLastState = 2;

struct Choice {
	size_t            video = 0;
	Edition           edition = Edition::Remastered;
	Clock::time_point at;
};

std::atomic<bool> g_enabled{true};
SealedText::Texts g_texts;
std::string_view (*g_locale)() = nullptr;
std::optional<Choice> g_choice;
int g_level = 0;
std::vector<Cue> g_cues;
Scheduler g_scheduler;
Clock::time_point g_start;
double g_startDelay = 0;
size_t g_said = 0, g_dropped = 0;

std::string_view EditionFolder(Edition edition) {
	return edition == Edition::Classic ? "classic" : "hd";
}

const std::string* Text(const SealedText::Texts& texts, Edition edition, std::string_view locale) {
	const auto found = texts.find(std::string(EditionFolder(edition)) + "/" + std::string(locale));
	return found == texts.end() ? nullptr : &found->second;
}

// The Videos list's classic box, read as the choice is made; the 2021 videos when unreadable.
Edition ReadEdition() {
	const std::string path[] = {"LegacyCheckbox"};
	const uintptr_t box = NativeWidgets::Find("CinematicsModal", path);
	return NativeUi::Toggled(box).value_or(false) ? Edition::Classic : Edition::Remastered;
}

// ProgressionHelper's state and index; false when the panel or the read fails.
bool ReadProgression(int32_t& state, int32_t& index) {
	const std::string path[] = {"ProgressionHelper"};
	const uintptr_t helper = NativeWidgets::Find("InGameMoviePlayerPanel", path);
	return helper && NativeMemory::Peek(helper + ProgressionStateOffset, state) && NativeMemory::Peek(helper + ProgressionIndexOffset, index);
}

// The act video the game started in play, when its index fits the act the player is in. The
// classic one is assumed to follow Legacy Graphics mode (inferred from the RE, unverified).
std::optional<Choice> InPlayChoice(const D2RL::PluginContext* context) {
	int32_t state = -1, index = -1;
	if (!ReadProgression(state, index)) {
		AsyncLog::Info(context, "[audiodesc] a movie started in play, no ProgressionHelper to name it: nothing said");
		return std::nullopt;
	}
	SettingsFile::Reload();
	const bool legacy = SettingsFile::Number("legacygraphicsmode").value_or(0) != 0;
	const int act = Generated::FindLevelFlags(g_level).act;
	const std::optional<size_t> video = state >= 0 && state <= ProgressionLastState ? InPlayVideo(index, act) : std::nullopt;
	AsyncLog::InfoF(context, "[audiodesc] in play: ProgressionHelper state %d, index %d; level %d (act %d), legacy graphics %d%s",
		state, index, g_level, act + 1, legacy ? 1 : 0, video ? "" : ": does not fit, nothing said");
	if (!video) return std::nullopt;
	return Choice{*video, legacy ? Edition::Classic : Edition::Remastered, Clock::now()};
}
}

std::vector<Cue> CuesFor(const SealedText::Texts& texts, Edition edition, std::string_view locale, std::string_view video) {
	for (const std::string_view language : {locale, English}) {
		const std::string* text = Text(texts, edition, language);
		if (!text) continue;
		Parsed parsed = Parse(*text);
		if (const auto found = parsed.script.find(video); found != parsed.script.end()) return std::move(found->second);
	}
	return {};
}

void Load(const D2RL::PluginContext* context, SealedText::Texts texts, std::string_view (*locale)()) {
	g_texts = std::move(texts);
	g_locale = locale;
	AsyncLog::InfoF(context, "[audiodesc] %zu scripts sealed in", g_texts.size());
}

void MenuChoice(const D2RL::PluginContext* context, std::string_view index) {
	size_t video = 0;
	const auto [end, error] = std::from_chars(index.data(), index.data() + index.size(), video);
	if (error != std::errc{} || end != index.data() + index.size() || video >= Videos.size()) {
		AsyncLog::InfoF(context, "[audiodesc] unknown Videos list choice \"%.*s\"", static_cast<int>(index.size()), index.data());
		g_choice.reset();
		return;
	}
	g_choice = Choice{video, ReadEdition(), Clock::now()};
}

void MovieStarted(const D2RL::PluginContext* context) {
	g_scheduler.Stop();
	g_said = g_dropped = 0;
	const auto now = Clock::now();
	const std::optional<Choice> choice = g_choice && now - g_choice->at < ChoiceLifetime ? g_choice : InPlayChoice(context);
	g_choice.reset();
	if (!choice) return;
	const std::string_view video = Videos[choice->video];
	const std::string_view locale = g_locale ? g_locale() : English;
	g_cues = CuesFor(g_texts, choice->edition, locale, video);
	g_start = now;
	g_startDelay = choice->edition == Edition::Classic ? ClassicStartDelay : RemasteredStartDelay;
	g_scheduler.Start(&g_cues);
	AsyncLog::InfoF(context, "[audiodesc] %.*s (%.*s, %.*s): %zu cues%s", static_cast<int>(video.size()), video.data(),
		static_cast<int>(EditionFolder(choice->edition).size()), EditionFolder(choice->edition).data(),
		static_cast<int>(locale.size()), locale.data(), g_cues.size(), g_enabled ? "" : ", switched off");
}

void MovieEnded(const D2RL::PluginContext* context) {
	if (!g_scheduler.Running() && g_said == 0 && g_dropped == 0) return;
	g_scheduler.Stop();
	AsyncLog::InfoF(context, "[audiodesc] ended after %.1f s: %zu said, %zu dropped late",
		std::chrono::duration<double>(Clock::now() - g_start).count(), g_said, g_dropped);
	g_said = g_dropped = 0;
}

void NoteLevel(const D2RL::PluginContext* context, int level) {
	if (level > 0 && g_level <= 0) {
		int32_t state = -1, index = -1;
		if (ReadProgression(state, index)) AsyncLog::InfoF(context, "[audiodesc] ProgressionHelper at rest: state %d, index %d", state, index);
		else AsyncLog::Info(context, "[audiodesc] ProgressionHelper not found on entering the game");
	}
	g_level = level;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_scheduler.Running()) return;
	const double clock = std::chrono::duration<double>(Clock::now() - g_start).count() - g_startDelay;
	for (const Cue* cue : g_scheduler.Due(clock, g_dropped)) {
		if (!g_enabled) continue;
		ScreenReader::Speak(cue->text, false);
		++g_said;
		AsyncLog::InfoF(context, "[audiodesc] %.2f at %.2f: %s", cue->at, clock, cue->text.c_str());
	}
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }

}
