#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace D2RAccess::GroundItems {
enum class Sound { Gold, Potion, Normal, Magic, Rare, Legendary, Count };
struct Point { double x = 0, y = 0; };
struct Item {
	uint32_t id = 0, code = 0, quality = 0;
	Point position;
	Sound sound = Sound::Normal;
	std::string name, baseName;
};
struct Snapshot { uint64_t session = 0; Point player; std::vector<Item> items; };
struct Voice { uint32_t id = 0; Sound sound{}; float right = 0, front = 0, gain = 0, pitch = 1; };
struct Update { std::vector<Voice> voices; std::string speech; };

// Accessibility tuning in collision subtiles, not assumed native pickup ranges.
inline constexpr double AudibleRadius = 30, SpeakRadius = 3, RearmRadius = 6;
inline constexpr size_t MaxVoices = 8, MaxNamesPerAnnouncement = 4;
inline constexpr uint64_t SpeechGapMs = 900, PollMs = 50, SnapshotExpiryMs = 300;
// Near-source gain for peak-normalized recordings; center panning halves each channel.
inline constexpr float LoopVolume = 0.6f, RearPitch = 0.94f;
inline constexpr float MixGainLimit = 0.75f; // headroom even when nearby loops peak together
inline constexpr const char* SoundFiles[] = {"gold.wav", "potion.wav", "normal equipment.wav",
	"magic equipment.wav", "rare equipment.wav", "legendary equipment.wav"};

Sound Classify(uint32_t quality, bool gold, bool consumable, uint32_t code = 0);
Voice Spatialize(const Item& item, Point player);
bool MatchesLabel(std::string_view label, const std::vector<std::string>& names);
class Tracker {
public:
	Update Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled);
	void Reset();
private:
	uint64_t session_ = 0, lastSpeech_ = 0;
	bool hasSpoken_ = false;
	std::unordered_map<uint32_t, bool> spoken_;
};
}
