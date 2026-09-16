#include "ground_items.h"
#include <D2RLPlugin/item.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cctype>
#include <unordered_set>

namespace D2RAccess::GroundItems {
Sound Classify(uint32_t quality, bool gold, bool consumable, uint32_t code) {
	if (gold) return Sound::Gold;
	if (consumable) return Sound::Potion;
	// Installed misc.txt: quest scrolls/potions inherit 'ques', not 'scro'/'poti'.
	using D2RL::Items::MakeItemCode;
	constexpr uint32_t QuestConsumables[] = {MakeItemCode("bks"), MakeItemCode("tr1"),
		MakeItemCode("tr2"), MakeItemCode("xyz"), MakeItemCode("ice")};
	for (const auto questCode : QuestConsumables) if (code == questCode) return Sound::Potion;
	using Q = D2RL::Items::Quality;
	switch (static_cast<Q>(quality)) {
	case Q::Magic: return Sound::Magic;
	case Q::Rare: case Q::Crafted: return Sound::Rare;
	case Q::Unique: case Q::Set: return Sound::Legendary;
	default: return Sound::Normal;
	}
}
Voice Spatialize(const Item& item, Point player) {
	const double dx = item.position.x - player.x, dy = item.position.y - player.y;
	const double distance = std::hypot(dx, dy);
	Voice voice{item.id, item.sound};
	if (!std::isfinite(distance)) return voice;
	// Native projection 0x334DB0: +X down/right, +Y down/left.
	voice.right = static_cast<float>((dx - dy) / std::numbers::sqrt2);
	voice.front = static_cast<float>(-(dx + dy) / std::numbers::sqrt2);
	const double proximity = std::clamp(1 - distance / AudibleRadius, 0.0, 1.0);
	voice.gain = static_cast<float>(LoopVolume * proximity * proximity);
	// Blend the rear pitch across one subtile to avoid chatter at the side axis.
	voice.pitch = 1 + (RearPitch - 1) * std::clamp(-voice.front, 0.0f, 1.0f);
	return voice;
}
bool MatchesLabel(std::string_view label, const std::vector<std::string>& names) {
	for (const auto& name : names) {
		if (name.empty()) continue;
		const auto pos = label.find(name);
		if (pos == std::string_view::npos) continue;
		const auto boundary = [](unsigned char c) { return c < 128 && !std::isalnum(c); };
		if ((pos == 0 || boundary(static_cast<unsigned char>(label[pos - 1])))
			&& (pos + name.size() == label.size() || boundary(static_cast<unsigned char>(label[pos + name.size()])))) return true;
	}
	return false;
}
Update Tracker::Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled, bool itemLoops) {
	if (session_ != snapshot.session) { Reset(); session_ = snapshot.session; }
	Update result;
	std::vector<std::pair<double, const Item*>> exits;
	std::unordered_set<uint32_t> seenExits;
	for (const auto& landmark : snapshot.landmarks) {
		const double distance = std::hypot(landmark.position.x - snapshot.player.x, landmark.position.y - snapshot.player.y);
		if (std::isfinite(distance) && distance < AudibleRadius && seenExits.insert(landmark.id).second) exits.emplace_back(distance, &landmark);
	}
	std::sort(exits.begin(), exits.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	for (const auto& [distance, landmark] : exits)
		if (result.voices.size() < MaxVoices) result.voices.push_back(Spatialize(*landmark, snapshot.player));
	std::vector<std::pair<double, const Item*>> nearby;
	std::unordered_set<uint32_t> present;
	for (const auto& item : snapshot.items) {
		if (!present.insert(item.id).second) continue;
		const double distance = std::hypot(item.position.x - snapshot.player.x, item.position.y - snapshot.player.y);
		if (!std::isfinite(distance)) continue;
		if (distance > RearmRadius) spoken_.erase(item.id);
		if (distance <= AudibleRadius) nearby.emplace_back(distance, &item);
	}
	std::erase_if(spoken_, [&](const auto& entry) { return !present.contains(entry.first); });
	std::sort(nearby.begin(), nearby.end(), [](const auto& a, const auto& b) {
		return a.first == b.first ? a.second->id < b.second->id : a.first < b.first;
	});
	const bool canSpeak = speechEnabled && (!hasSpoken_ || now < lastSpeech_ || now - lastSpeech_ >= SpeechGapMs);
	size_t names = 0;
	for (const auto& [distance, item] : nearby) {
		if (itemLoops && result.voices.size() < MaxVoices && distance < AudibleRadius)
			result.voices.push_back(Spatialize(*item, snapshot.player));
		if (!canSpeak || distance > SpeakRadius || item->name.empty() || names >= MaxNamesPerAnnouncement || spoken_.contains(item->id)) continue;
		if (!result.speech.empty()) result.speech += "; ";
		result.speech += item->name;
		spoken_[item->id] = true;
		++names;
	}
	if (!result.speech.empty()) { lastSpeech_ = now; hasSpoken_ = true; }
	float totalGain = 0;
	for (const auto& voice : result.voices) totalGain += voice.gain;
	if (totalGain > MixGainLimit) for (auto& voice : result.voices) voice.gain *= MixGainLimit / totalGain;
	return result;
}
void Tracker::Reset() { spoken_.clear(); session_ = lastSpeech_ = 0; hasSpoken_ = false; }
}
