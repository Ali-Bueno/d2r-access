#include "ground_items.h"
#include <D2RLPlugin/item.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <cctype>
#include <unordered_set>

namespace D2RAccess::GroundItems {
Sound Classify(uint32_t quality, Base base, bool quest) {
	if (quest) return Sound::Quest;
	switch (base) {
	case Base::Gold: return Sound::Gold;
	case Base::Potion: return Sound::Potion;
	case Base::Scroll: return Sound::Scroll;
	case Base::Rune: return Sound::Rune;
	case Base::Gem: return Sound::Gem;
	case Base::Equipment: break;
	}
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
	voice.source = item.source;
	if (!std::isfinite(distance)) return voice;
	// Native projection 0x334DB0: +X down/right, +Y down/left.
	voice.right = static_cast<float>((dx - dy) / std::numbers::sqrt2);
	voice.front = static_cast<float>(-(dx + dy) / std::numbers::sqrt2);
	const double proximity = std::clamp(1 - distance / AudibleRange(item.sound), 0.0, 1.0);
	voice.gain = static_cast<float>(LoopVolume * proximity * (item.sound == Sound::Quest ? 1 : proximity));
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
namespace {
// Nearest sources, deduplicated by unit kind/id and capped in priority order.
void Emit(const std::vector<Item>& sources, Point player, size_t limit, Update& result) {
	std::vector<std::pair<double, const Item*>> nearest;
	std::unordered_set<uint64_t> seen;
	for (const auto& source : sources) {
		const double distance = std::hypot(source.position.x - player.x, source.position.y - player.y);
		const uint64_t identity = (static_cast<uint64_t>(source.source) << 32) | source.id;
		if (std::isfinite(distance) && distance < AudibleRange(source.sound) && seen.insert(identity).second) nearest.emplace_back(distance, &source);
	}
	std::sort(nearest.begin(), nearest.end(), [](const auto& a, const auto& b) {
		if (a.first != b.first) return a.first < b.first;
		if (a.second->source != b.second->source) return a.second->source < b.second->source;
		return a.second->id < b.second->id;
	});
	size_t emitted = 0;
	for (const auto& [distance, source] : nearest) {
		if (result.voices.size() >= MaxVoices || emitted >= limit) break;
		result.voices.push_back(Spatialize(*source, player));
		++emitted;
	}
}
// User priority for names read together: quest items, consumables, gold, then equipment last.
int SpeechRank(Sound sound) {
	switch (sound) {
	case Sound::Quest: return 0;
	case Sound::Potion: case Sound::Scroll: return 1;
	case Sound::Gold: return 2;
	default: return 3;
	}
}
}

Update Tracker::Tick(const Snapshot& snapshot, uint64_t now, bool speechEnabled, bool itemLoops, bool containerLoops, bool doorLoops) {
	if (session_ != snapshot.session) { Reset(); session_ = snapshot.session; }
	Update result;
	if (itemLoops) {
		auto quests = snapshot.questObjects;
		for (const auto& item : snapshot.items) if (item.sound == Sound::Quest) quests.push_back(item);
		Emit(quests, snapshot.player, MaxQuestVoices, result);
	}
	Emit(snapshot.landmarks, snapshot.player, MaxVoices, result);
	if (doorLoops) Emit(snapshot.doors, snapshot.player, MaxDoorVoices, result);
	if (containerLoops) Emit(snapshot.containers, snapshot.player, MaxContainerVoices, result);
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
	std::vector<const Item*> reachable;
	for (const auto& [distance, item] : nearby) {
		if (itemLoops && item->sound != Sound::Quest && result.voices.size() < MaxVoices && distance < AudibleRadius)
			result.voices.push_back(Spatialize(*item, snapshot.player));
		if (distance <= SpeakRadius && !item->name.empty() && !spoken_.contains(item->id)) reachable.push_back(item);
	}
	std::stable_sort(reachable.begin(), reachable.end(), [](const Item* a, const Item* b) {
		return SpeechRank(a->sound) < SpeechRank(b->sound);
	});
	const bool arrived = std::any_of(reachable.begin(), reachable.end(), [&](const Item* item) { return !waiting_.contains(item->id); });
	const bool canSpeak = speechEnabled
		&& (arrived || !hasSpoken_ || now < lastSpeech_ || now - lastSpeech_ >= SpeechGapMs);
	waiting_.clear();
	size_t names = 0;
	for (const Item* item : reachable) {
		if (!canSpeak || names >= MaxNamesPerAnnouncement) { waiting_.insert(item->id); continue; }
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
void Tracker::Reset() { spoken_.clear(); waiting_.clear(); session_ = lastSpeech_ = 0; hasSpoken_ = false; }
}
