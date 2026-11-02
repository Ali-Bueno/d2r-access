#include "audio_description.h"

#include "async_log.h"
#include "generated/level_flags.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "settings_file.h"

#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
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
// The movie's subtitle box: by name under MoviePlayer, else the one embedded in MoviePlayer at
// +0x200 (the movie probe, 2026-11-01: "Subtitle" at +0x208 and the hold-to-skip box's name and text
// at +0x438 and +0x4B8, a widget's name at +0x8 and text at +0x88), taken only when it is named so.
constexpr size_t EmbeddedSubtitleOffset = 0x200;
constexpr auto SubtitleSearchEvery = std::chrono::milliseconds(250);
// A change of the video's offset smaller than this is reading jitter.
constexpr double OffsetStep = 0.1;
// The movie's own clock (the deep movie probe, 2026-11-01, 3.3.94995; four plays of the intro,
// both editions): MoviePlayer +0xC0 -> +0x160 is a table of nodes, and the object the first field
// of one of them points to, the same from movie to movie at a varying slot (+0x60, +0xC0, +0x120,
// +0x240 seen), holds the time shown:
// in ns at +0x68 (the frame's: 30 fps in 2021, 24 classic) and in ms at +0x88. Both were at zero
// 0.75 to 0.96 s after Play, where the subtitles put the video's start; it stops in a hitch.
constexpr size_t ClockOwnerOffset = 0xC0, ClockTableOffset = 0x160;
constexpr size_t ClockTableSpan = 0x1000; // the slots searched
constexpr size_t ClockNanosOffset = 0x68, ClockMillisOffset = 0x88;
constexpr int64_t ClockMillisLimit = 900000; // longer than any video
constexpr int64_t ClockAgreeMillis = 100;    // the two fields of the clock agree this closely
constexpr auto ClockSearchEvery = std::chrono::milliseconds(100);
// A slot's clock is taken when it advanced at 0.7 to 1.3 times the wall clock over at least this.
constexpr double ClockConfirmSeconds = 0.2;
constexpr double ClockRateLow = 0.7, ClockRateHigh = 1.3;
// A reading further ahead of the last one than the time between them, plus this, is a misread.
constexpr double ClockJumpLimit = 0.5;
// Subtitle lines logged with the clock, to check it against the subtitle files (live 2026-11-01:
// six lines a play, both editions, each within 0.02 s of its start).
constexpr size_t CalibrationLines = 3;

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
SpeechStarts g_speech;
DialogueGate g_gate;
SubtitleStarts g_subtitles;
SubtitleSync g_sync;
uintptr_t g_player = 0, g_subtitle = 0;
Clock::time_point g_nextSearch;
std::string g_line;
// The clock is wall + g_base + g_offset: g_base puts it on the video's own clock once found, the
// subtitles' g_offset corrects what remains.
double g_base = 0, g_offset = 0;
struct Slot { uintptr_t object = 0; int64_t millis = 0; double wall = 0; };
std::vector<Slot> g_slots;          // the table's clocks at the last search
Clock::time_point g_nextClockSearch;
uintptr_t g_clock = 0;              // the clock object, once found
std::optional<double> g_video;      // its last reading, seconds
double g_videoWall = 0;             // the wall clock at that reading
std::atomic<bool> g_videoLive{false};
std::string g_cueLocale;
Clock::time_point g_start;
double g_startDelay = 0;
size_t g_said = 0, g_dropped = 0, g_cut = 0, g_lines = 0;

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

// The movie's MoviePlayer widget and its subtitle box (by name, else the embedded one).
void FindPlayer(const D2RL::PluginContext* context, double wall) {
	const auto now = Clock::now();
	if (now < g_nextSearch) return;
	g_nextSearch = now + SubtitleSearchEvery;
	const std::string player[] = {"MoviePlayer"};
	const std::string subtitle[] = {"MoviePlayer", "Subtitle"};
	for (const char* panel : {"MoviePlayerPanel", "InGameMoviePlayerPanel"}) {
		const uintptr_t movie = NativeWidgets::Find(panel, player);
		if (!movie) continue;
		const char* how = "by name";
		uintptr_t box = NativeWidgets::Find(panel, subtitle);
		if (!box && NativeWidgets::ReadName(movie + EmbeddedSubtitleOffset) == "Subtitle") {
			how = "embedded";
			box = movie + EmbeddedSubtitleOffset;
		}
		g_player = movie;
		g_subtitle = box;
		AsyncLog::InfoF(context, "[audiodesc] %s/MoviePlayer found at %.2f; subtitles %s", panel, wall, box ? how : "not found");
		return;
	}
}

bool HeapLike(uintptr_t value) {
	return value > 0x10000 && value < 0x00007FFFFFFFFFFF && (value & 7) == 0;
}

// The object's time in ms, when its ns and ms fields agree on a plausible one.
std::optional<int64_t> ClockMillis(uintptr_t object) {
	int64_t nanos = 0, millis = 0;
	if (!NativeMemory::Peek(object + ClockNanosOffset, nanos) || !NativeMemory::Peek(object + ClockMillisOffset, millis)) return std::nullopt;
	if (millis < 0 || millis > ClockMillisLimit || nanos < 0 || std::abs(nanos / 1000000 - millis) > ClockAgreeMillis) return std::nullopt;
	return millis;
}

// Finds the clock among the table's objects: one whose time moved with the wall clock between two
// searches (a clock left from the last movie stands still until this one starts).
void FindVideoClock(const D2RL::PluginContext* context, double wall) {
	const auto now = Clock::now();
	if (now < g_nextClockSearch) return;
	g_nextClockSearch = now + ClockSearchEvery;
	uintptr_t owner = 0, table = 0;
	std::array<uintptr_t, ClockTableSpan / sizeof(uintptr_t)> slots{};
	if (!NativeMemory::Peek(g_player + ClockOwnerOffset, owner) || !HeapLike(owner) || !NativeMemory::Peek(owner + ClockTableOffset, table)
		|| !HeapLike(table) || !NativeMemory::Copy(table, slots.data(), sizeof slots)) return;
	std::vector<Slot> found;
	for (size_t i = 0; i < slots.size(); ++i) {
		uintptr_t object = 0;
		if (!HeapLike(slots[i]) || !NativeMemory::Peek(slots[i], object) || !HeapLike(object)) continue;
		const std::optional<int64_t> millis = ClockMillis(object);
		if (!millis || *millis == 0) continue;
		for (const Slot& before : g_slots) {
			if (before.object != object) continue;
			const double seconds = wall - before.wall, moved = (*millis - before.millis) / 1000.0;
			if (seconds < ClockConfirmSeconds || moved < seconds * ClockRateLow || moved > seconds * ClockRateHigh) break;
			g_clock = object;
			g_video = *millis / 1000.0;
			g_videoWall = wall;
			g_base = *g_video - wall;
			g_videoLive = true;
			g_slots.clear();
			// The lines seen so far were on the wall clock.
			g_offset = 0;
			g_sync.Restart();
			AsyncLog::InfoF(context, "[audiodesc] video clock found in slot +0x%zX at %.2f: the video is at %.2f", i * sizeof(uintptr_t), wall, *g_video);
			return;
		}
		found.push_back({object, *millis, wall});
	}
	// A slot is compared with the search before it only once enough time has passed.
	if (g_slots.empty() || wall - g_slots.front().wall >= ClockConfirmSeconds) g_slots = std::move(found);
}

// Puts the clock on the video's own: it stands still in a hitch. A reading that fails or jumps
// leaves the clock where the last good one put it.
void FollowVideoClock(const D2RL::PluginContext* context, double wall) {
	if (!g_clock) {
		FindVideoClock(context, wall);
		return;
	}
	const std::optional<int64_t> millis = ClockMillis(g_clock);
	if (!millis) return;
	const double video = *millis / 1000.0;
	if (g_video && (video < *g_video - 0.05 || video - *g_video > wall - g_videoWall + ClockJumpLimit)) return;
	g_video = video;
	g_videoWall = wall;
	g_base = video - wall;
}

// Each new subtitle line says where the video really is (SubtitleSync), on top of the video's clock
// or, without it, the wall clock.
void FollowSubtitles(const D2RL::PluginContext* context, double raw) {
	if (!g_subtitle) return;
	std::string line = NativeWidgets::ReadText(g_subtitle);
	if (line == g_line) return;
	g_line = std::move(line);
	if (g_line.empty()) return;
	if (++g_lines <= CalibrationLines && g_videoLive)
		AsyncLog::InfoF(context, "[audiodesc] line %zu shown at %.3f by the video clock", g_lines, raw);
	const std::optional<double> offset = g_sync.Seen(raw);
	if (!offset || std::abs(*offset - g_offset) < OffsetStep) return;
	AsyncLog::InfoF(context, "[audiodesc] line at %.2f: the video runs %+.2f s off the clock (was %+.2f)", raw, *offset, g_offset);
	g_offset = *offset;
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
	const auto speech = g_texts.find("speech");
	g_speech = speech == g_texts.end() ? SpeechStarts{} : ParseSpeech(speech->second);
	const auto subtitles = g_texts.find("subtitles");
	g_subtitles = subtitles == g_texts.end() ? SubtitleStarts{} : ParseSubtitles(subtitles->second);
	AsyncLog::InfoF(context, "[audiodesc] %zu files sealed in, dialogue starts for %zu videos, subtitle lines for %zu",
		g_texts.size(), g_speech.size(), g_subtitles.size());
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
	g_gate.Stop();
	g_sync.Stop();
	g_player = g_subtitle = 0;
	g_line.clear();
	g_base = g_offset = 0;
	g_slots.clear();
	g_nextClockSearch = {};
	g_clock = 0;
	g_video.reset();
	g_videoWall = 0;
	g_videoLive = false;
	g_said = g_dropped = g_cut = g_lines = 0;
	const auto now = Clock::now();
	const std::optional<Choice> choice = g_choice && now - g_choice->at < ChoiceLifetime ? g_choice : InPlayChoice(context);
	g_choice.reset();
	if (!choice) return;
	const std::string_view video = Videos[choice->video];
	const std::string_view locale = g_locale ? g_locale() : English;
	g_cues = CuesFor(g_texts, choice->edition, locale, video);
	g_cueLocale = locale;
	g_start = now;
	g_startDelay = choice->edition == Edition::Classic ? ClassicStartDelay : RemasteredStartDelay;
	g_scheduler.Start(&g_cues);
	const auto speech = g_speech.find(video);
	g_gate.Start(speech == g_speech.end() ? nullptr : &speech->second);
	// The game shows the subtitles of its own language, taken to be the mod's; English otherwise.
	if (const auto lines = g_subtitles.find(video); lines != g_subtitles.end()) {
		auto found = lines->second.find(locale);
		if (found == lines->second.end()) found = lines->second.find(English);
		g_sync.Start(found == lines->second.end() ? nullptr : &found->second);
	}
	g_nextSearch = now;
	AsyncLog::InfoF(context, "[audiodesc] %.*s (%.*s, %.*s): %zu cues%s", static_cast<int>(video.size()), video.data(),
		static_cast<int>(EditionFolder(choice->edition).size()), EditionFolder(choice->edition).data(),
		static_cast<int>(locale.size()), locale.data(), g_cues.size(), g_enabled ? "" : ", switched off");
}

void MovieEnded(const D2RL::PluginContext* context) {
	if (!g_scheduler.Running() && g_said == 0 && g_dropped == 0) return;
	g_scheduler.Stop();
	g_gate.Stop();
	g_sync.Stop();
	g_player = g_subtitle = 0;
	AsyncLog::InfoF(context, "[audiodesc] ended after %.1f s: %zu said, %zu dropped late, %zu dialogue stops; clock %s, %+.2f s on the wall clock, subtitles %+.2f s on that (%zu lines)",
		std::chrono::duration<double>(Clock::now() - g_start).count(), g_said, g_dropped, g_cut, g_videoLive ? "video" : "wall",
		g_base, g_offset, g_lines);
	g_videoLive = false;
	g_said = g_dropped = g_cut = g_lines = 0;
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
	const double wall = std::chrono::duration<double>(Clock::now() - g_start).count() - g_startDelay;
	if (!g_player) FindPlayer(context, wall);
	if (g_player) FollowVideoClock(context, wall);
	FollowSubtitles(context, wall + g_base);
	const double clock = wall + g_base + g_offset;
	// The cut comes first: no cue starts in dialogue (CTest), so a cue due now follows it.
	if (g_gate.CutDue(clock) && g_enabled) {
		ScreenReader::Stop();
		++g_cut;
		AsyncLog::InfoF(context, "[audiodesc] cut at %.2f: dialogue starts", clock);
	}
	for (const Cue* cue : g_scheduler.Due(clock, g_dropped)) {
		if (!g_enabled) continue;
		ScreenReader::Speak(cue->text, false);
		g_gate.Said(clock, EstimatedSeconds(cue->text, g_cueLocale));
		++g_said;
		AsyncLog::InfoF(context, "[audiodesc] %.2f at %.2f: %s", cue->at, clock, cue->text.c_str());
	}
}

bool Enabled() { return g_enabled; }
bool VideoClockRead() { return g_videoLive; }
void SetEnabled(bool on) { g_enabled = on; }

}
