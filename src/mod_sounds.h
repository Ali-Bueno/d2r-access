#pragma once

// One switch over every sound the mod makes itself: the sonar, the beacon, the
// entity cues, and every loop and chime of the item mixer. Each of those keeps
// its own switch as well; this one silences them all without changing any.
// The game's own sounds are not the mod's and are never touched.
namespace D2RAccess::ModSounds {
bool Enabled();
// Turning it off also stops what is already queued or looping, at once.
void SetEnabled(bool enabled);
}
