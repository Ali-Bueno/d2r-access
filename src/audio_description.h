#pragma once

#include "audio_description_script.h"
#include "sealed_text.h"
#include <D2RLPlugin/api.h>
#include <string_view>

// Narration of the act cinematics between their spoken lines, in the language the mod speaks.
// docs/cinematics-audio-description.md
namespace D2RAccess::AudioDescription {

// The scripts sealed in this plugin by "<edition>/<locale>" ("hd/enUS"); empty, and logged, when
// missing or altered.
SealedText::Texts OpenBundled(const D2RL::PluginContext* context);
// `texts` as OpenBundled gives them; `locale()` names the language at each video's start.
void Load(const D2RL::PluginContext* context, SealedText::Texts texts, std::string_view (*locale)());

// The cues a video has in `locale`, its English ones where that language has none.
std::vector<Cue> CuesFor(const SealedText::Texts& texts, Edition edition, std::string_view locale, std::string_view video);

// UI thread. The Videos list's choice (`Cinematics:Play` text, "0" to "6"), read with its
// classic box, for the movie that starts next.
void MenuChoice(const D2RL::PluginContext* context, std::string_view index);
void MovieStarted(const D2RL::PluginContext* context);
void MovieEnded(const D2RL::PluginContext* context);
// The level the player is in (0 out of a game), to check a video started in play against its act.
void NoteLevel(const D2RL::PluginContext* context, int level);
void Tick(const D2RL::PluginContext* context);

// The switch, on by default.
bool Enabled();
void SetEnabled(bool on);
// The playing movie's audio count is read and holds the clock (MovieProbe looks only when it is not).
bool VideoClockRead();

}
