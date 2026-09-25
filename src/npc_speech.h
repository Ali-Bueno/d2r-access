#pragma once

// The game's own NPC Speech option (Sound options: audio only, text only, or
// audio and text). On audio only the voice carries the dialogue, so its text
// is not read (the user, 2026-09-25).
namespace D2RAccess::NpcSpeech {

// Whether the option currently stands on audio only; false when unknown.
bool AudioOnly();

}
