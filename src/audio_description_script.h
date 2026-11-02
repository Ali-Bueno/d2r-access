#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Timed narration for the act cinematics, one file per edition and language.
// docs/cinematics-audio-description.md
namespace D2RAccess::AudioDescription {

// The cinematics in the order of the main menu's Videos list (`Cinematics:Play:<index>`).
inline constexpr std::array<std::string_view, 7> Videos{
	"d2intro", "Act02start", "Act03start", "Act04start", "Act04end", "D2x_Intro", "D2x_Out"};

// The 2021 videos, or the 2000 ones the Videos list's "classic" box plays.
enum class Edition { Remastered, Classic };

struct Cue {
	double      at = 0; // seconds into the video
	std::string text;
};
// Cues by video name as `Videos` spells it, each list in time order.
using Script = std::map<std::string, std::vector<Cue>, std::less<>>;

struct Parsed {
	Script              script;
	std::vector<size_t> rejected; // 1-based line numbers
};
// "<video> TAB <seconds> TAB <text>" lines, # starts a note. A line with an unknown video (any
// case), a bad time or no text is rejected.
Parsed Parse(std::string_view text);

// The video's index in `Videos`, any case.
std::optional<size_t> VideoIndex(std::string_view name);

// The `Videos` index of the act video the game started in play, from ProgressionHelper's index
// (the game's own movie table, one entry ahead of `Videos`: Act II's opening is 2, live 2026-11-01)
// and the Levels.txt act the player is in (0 for Act I); nothing when that video cannot start there.
std::optional<size_t> InPlayVideo(int64_t progressionIndex, int act);

// Characters a second the scripts are timed for, by locale: a fast screen reader (20, the user's
// choice 2026-11-01; CJK scaled from the earlier 7, 4.5 and 6.5 at 14). tools/audiodesc.py check
// uses the same numbers.
double Pace(std::string_view locale);
// Seconds `text` takes at `Pace(locale)`, counting code points.
double EstimatedSeconds(std::string_view text, std::string_view locale);

// When each video's dialogue starts, from speech.tsv (sealed in as "speech"): "<video> TAB <start>
// TAB <end>" lines; notes, the length lines and bad lines are skipped. Each list in time order.
using SpeechStarts = std::map<std::string, std::vector<double>, std::less<>>;
SpeechStarts ParseSpeech(std::string_view text);

// When each subtitle line starts, by video then locale: subtitles.tsv ("<video> TAB <locale> TAB
// <start>,<start>,..."; sound tags included), sealed in as "subtitles". Each list in time order.
using LocaleStarts = std::map<std::string, std::vector<double>, std::less<>>;
using SubtitleStarts = std::map<std::string, LocaleStarts, std::less<>>;
SubtitleStarts ParseSubtitles(std::string_view text);

// Follows the video's own clock through its subtitles. The game shows each line when the video
// reaches it, so the offset that puts the recent lines (seen at `raw` wall-clock seconds) on line
// starts is how far the video lags (a hitch, a slow start) or leads. Two of the last `Recent` lines
// must agree before an offset is given, so a line missed, an extra one or a sound tag the game does
// not show cannot move the clock on its own.
class SubtitleSync {
public:
	static constexpr size_t Recent = 3;
	static constexpr double Behind = 8.0; // the most the video is taken to lag, seconds
	static constexpr double Ahead = 1.0;  // the most it is taken to lead (a faster start)
	static constexpr double Match = 0.3;  // a line this close to a start is on it
	void Start(const std::vector<double>* starts);
	void Stop() { Start(nullptr); }
	// Forgets the lines seen, as when the raw clock changes basis.
	void Restart() { seen_.clear(); }
	// A new line came up at `raw`; the offset to add to the raw clock, when two lines agree on one.
	std::optional<double> Seen(double raw);
private:
	bool Near(double t) const;
	const std::vector<double>* starts_ = nullptr;
	std::vector<double>        seen_;
};

// Cuts the narration where dialogue starts over it: a voice slower than `Pace` would still be
// talking. A cue counts as speaking for `SlowFactor` times its estimate.
class DialogueGate {
public:
	static constexpr double SlowFactor = 2.0;
	void Start(const std::vector<double>* starts);
	void Stop() { Start(nullptr); }
	// A cue began at `clock`, `seconds` long at `Pace`; queued after any cue still speaking.
	void Said(double clock, double seconds);
	// True once when dialogue starts at or before `clock` while the last cue may still be speaking.
	bool CutDue(double clock);
private:
	const std::vector<double>* starts_ = nullptr;
	size_t                     next_ = 0;
	double                     cueAt_ = 0, speakingUntil_ = -1;
};

// Speaks a video's cues on its clock: each once, in order; one due more than `LateLimit` ago (a
// late start, a skip) is dropped, never said late.
class Scheduler {
public:
	static constexpr double LateLimit = 2.0;
	void Start(const std::vector<Cue>* cues);
	void Stop();
	bool Running() const { return cues_ != nullptr; }
	// The cues due at `clock` seconds into the video; `dropped` counts the late ones skipped.
	std::vector<const Cue*> Due(double clock, size_t& dropped);
private:
	const std::vector<Cue>* cues_ = nullptr;
	size_t                  next_ = 0;
};

}
