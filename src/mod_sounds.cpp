#include "mod_sounds.h"
#include "beacon_audio.h"
#include "entity_cue_audio.h"
#include "item_audio.h"
#include "radar_audio.h"

#include <atomic>

namespace D2RAccess::ModSounds {
namespace { std::atomic<bool> enabled{true}; }

// Each audio engine asks before it queues a sound.
bool Enabled() { return enabled.load(std::memory_order_relaxed); }
void SetEnabled(bool on) {
	if (enabled.exchange(on, std::memory_order_relaxed) == on || on) return;
	RadarAudio::Silence("mod-sounds-off");
	BeaconAudio::Silence("mod-sounds-off");
	EntityCueAudio::Silence();
	ItemAudio::Silence();
	ItemAudio::SilenceExploration();
}
}
