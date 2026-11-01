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
