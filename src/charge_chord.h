#pragma once

#include <D2RLPlugin/api.h>

// The charge chord's voice (charge_chord_synth.h): its own XAudio2 stream and volume
// (SoundVolume::Family::Charges), on a worker so the game's thread never waits.
namespace D2RAccess::ChargeChord {
// Off, a short chord with each new charge (the user's default), or held while they last (bfbeast's idea).
enum class Mode { Off, Struck, Held, Count };
Mode CurrentMode();
int ModeLevel();
void SetModeLevel(int level);

bool Start(const D2RL::PluginContext* context);
// The first `notes` notes struck together.
void Strike(int notes);
// The first `notes` notes held; 0 lets the chord fade out.
void Hold(int notes);
// The full chord struck, whatever the mode: the sound glossary's sample.
void Preview();
void Stop();
}
