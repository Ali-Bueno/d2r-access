#pragma once

#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace D2RAccess::GroundItems {
enum class Sound { Gold, Potion, Normal, Magic, Rare, Legendary, Portal, Chest, Count };
struct Point { double x = 0, y = 0; };
struct Item {
	uint32_t id = 0, code = 0, quality = 0;
	Point position;
	Sound sound = Sound::Normal;
	std::string name, baseName;
};
// Landmarks (level exits, portals, waypoints) and containers (chests, caskets,
// urns, corpses) only loop; they are never spoken here.
struct Snapshot { uint64_t session = 0; Point player; std::vector<Item> items, landmarks, containers; };
struct Voice { uint32_t id = 0; Sound sound{}; float right = 0, front = 0, gain = 0, pitch = 1; };
struct Update { std::vector<Voice> voices; std::string speech; };

// Accessibility tuning in collision subtiles, not assumed native pickup ranges.
inline constexpr double AudibleRadius = 30, SpeakRadius = 3, RearmRadius = 6;
inline constexpr size_t MaxVoices = 8, MaxNamesPerAnnouncement = 4;
// A crypt can hold a dozen caskets. Containers take at most this share of the
// voices so a room full of them cannot silence the loot on the floor.
inline constexpr size_t MaxContainerVoices = 3;
inline constexpr uint64_t SpeechGapMs = 900, PollMs = 50, SnapshotExpiryMs = 300;
// Exits never move, so their list is refreshed slowly; only the listener position is polled fast.
inline constexpr uint64_t ExitRefreshMs = 500;
// Near-source gain for peak-normalized recordings; center panning halves each channel.
// User listening feedback: lower both individual sources and piles by about 3.5 dB.
inline constexpr float LoopVolume = 0.4f, RearPitch = 0.94f;
inline constexpr float MixGainLimit = 0.5f; // headroom even when nearby loops peak together
inline constexpr const char* SoundFiles[] = {"gold.wav", "potion.wav", "normal equipment.wav",
	"magic equipment.wav", "rare equipment.wav", "legendary equipment.wav", "portal.wav", "chest.wav"};
// The file list is indexed by Sound; a missing entry would be read past the end.
static_assert(std::size(SoundFiles) == static_cast<size_t>(Sound::Count));

Sound Classify(uint32_t quality, bool gold, bool consumable, uint32_t code = 0);
Voice Spatialize(const Item& item, Point player);
bool MatchesLabel(std::string_view label, const std::vector<std::string>& names);
class Tracker {
public:
	// Landmarks always get a voice first, then containers up to their share, then
	// items fill whatever is left when itemLoops is on.
	Update Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled, bool itemLoops = true,
		bool containerLoops = true);
	void Reset();
private:
	uint64_t session_ = 0, lastSpeech_ = 0;
	bool hasSpoken_ = false;
	std::unordered_map<uint32_t, bool> spoken_;
};
}
