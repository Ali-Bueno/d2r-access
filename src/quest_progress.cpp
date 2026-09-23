#include "quest_progress.h"
#include "ui_text.h"

#include <algorithm>

namespace D2RAccess::QuestProgress {
namespace {

bool Matches(std::string_view pattern, std::string_view text) {
	if (pattern == text) return !QuestLog::NeedsValue(text);
	for (const std::string_view conversion : {"%d", "%i", "%u"}) {
		const auto at = pattern.find(conversion);
		if (at == std::string_view::npos) continue;
		const auto prefix = pattern.substr(0, at), suffix = pattern.substr(at + conversion.size());
		if (!text.starts_with(prefix) || !text.ends_with(suffix)
			|| text.size() <= prefix.size() + suffix.size()) return false;
		const auto number = text.substr(prefix.size(), text.size() - prefix.size() - suffix.size());
		return std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; });
	}
	return false;
}

bool IsObjective(const Generated::Quest& quest, std::string_view text, const QuestLog::Resolver& resolve) {
	bool found = false;
	while (!text.empty()) {
		const auto end = text.find('\n');
		const auto line = UiText::Clean(text.substr(0, end));
		if (!line.empty()) {
			bool matched = false;
			for (uint8_t i = 0; i < quest.stepCount && !matched; ++i)
				matched = Matches(UiText::Clean(resolve(quest.stepKeys[i])), line);
			if (!matched) return false;
			found = true;
		}
		if (end == std::string_view::npos) break;
		text.remove_prefix(end + 1);
	}
	return found;
}

}

Reading Identify(std::string_view title, std::string_view shortText,
	std::string_view longText, const QuestLog::Resolver& resolve) {
	if (!resolve) return {};
	const auto heading = UiText::Clean(title), shortHeading = UiText::Clean(shortText);
	Reading result;
	for (const auto& quest : Generated::Quests) {
		const auto name = UiText::Clean(QuestLog::Title(quest, resolve));
		if (name.empty() || (name != heading && name != shortHeading)) continue;
		const auto objective = name == shortHeading ? longText : shortText;
		if (!IsObjective(quest, objective, resolve)) continue;
		if (!result.quest.empty()) return {}; // ambiguous localized title
		result = {name, UiText::Clean(objective)};
	}
	return result;
}

void PacketMatcher::Capture(NativePacket packet) {
	if (packet.titleId == 0 || packet.objective.empty()) return;
	packets_.push_back({std::move(packet), ++serial_});
	constexpr size_t MaxPackets = 16;
	if (packets_.size() > MaxPackets) packets_.erase(packets_.begin());
}

void PacketMatcher::Alert(uint64_t now) {
	alertAt_ = now;
	pending_ = true;
}

std::optional<NativePacket> PacketMatcher::Take(uint64_t now) {
	if (!pending_ || now < alertAt_ || now - alertAt_ < PacketWindowMs) return {};
	std::optional<NativePacket> matched;
	bool ambiguous = false;
	uint64_t lastSerial = consumed_;
	for (auto it = packets_.rbegin(); it != packets_.rend(); ++it) {
		if (it->serial <= consumed_ || it->packet.time > alertAt_ + PacketWindowMs) continue;
		if (it->packet.time < alertAt_ && alertAt_ - it->packet.time > PacketWindowMs) continue;
		lastSerial = (std::max)(lastSerial, it->serial);
		if (!matched) matched = it->packet;
		else if (matched->row != it->packet.row || matched->titleId != it->packet.titleId) ambiguous = true;
	}
	if (!matched) return {};
	consumed_ = lastSerial;
	pending_ = false;
	return ambiguous ? std::nullopt : matched;
}

void PacketMatcher::Reset() { *this = {}; }

void Reader::Schedule(uint64_t now) {
	if (!updated_ && !completed_) due_ = now + AlertDelayMs;
}

void Reader::Alert(uint64_t now) {
	if (signalled_ && now - lastSignal_ < DuplicateWindowMs) return;
	Schedule(now);
	updated_ = true;
	signalled_ = true;
	lastSignal_ = now;
}

void Reader::Completed(uint32_t row, uint32_t difficulty, uint64_t now) {
	const uint64_t key = (uint64_t{difficulty} << 32) | row;
	if (!completedRows_.insert(key).second) return;
	Schedule(now);
	completed_ = true;
}

void Reader::Navigate() {
	// Selecting a quest or tab is not progress. Forget the previous text so
	// delayed navigation writes cannot be attached to an earlier quest alert.
	sampled_ = baselined_ = false;
	detail_ = {};
	eventDetail_ = {};
}

void Reader::Correlate(Reading reading) {
	if (updated_ && !reading.open && !reading.quest.empty() && !reading.objective.empty())
		eventDetail_ = std::move(reading);
}

Announcement Reader::Update(const Reading& reading, uint64_t now, bool canSpeak) {
	if (!sampled_ || reading != candidate_) {
		candidate_ = reading;
		since_ = now;
		sampled_ = true;
		// Invalid data, opening/closing, and another quest are all new baselines.
		if (reading.quest.empty() || reading.quest != baseline_.quest || reading.open != baseline_.open) {
			baselined_ = false;
			detail_ = {};
		}
	}
	const bool settled = now - since_ >= SettleMs;
	if (settled && (!baselined_ || baseline_ != candidate_)) {
		if (baselined_ && !candidate_.open && !candidate_.quest.empty()) detail_ = candidate_;
		baseline_ = candidate_;
		baselined_ = !candidate_.quest.empty();
	}
	if (!canSpeak) return {};
	if (completed_ && now >= due_) {
		completed_ = updated_ = false;
		eventDetail_ = {};
		return {Kind::GoalDone, {}, {}};
	}
	if (settled && !detail_.quest.empty() && detail_ == candidate_) {
		const auto result = Announcement{Kind::Objective, detail_.quest, detail_.objective};
		detail_ = {};
		eventDetail_ = {};
		updated_ = false;
		return result;
	}
	if (updated_ && now >= due_) {
		updated_ = false;
		if (!eventDetail_.quest.empty()) {
			const auto result = Announcement{Kind::Objective, eventDetail_.quest, eventDetail_.objective};
			eventDetail_ = {};
			return result;
		}
		return {Kind::Updated, {}, {}};
	}
	return {};
}

void Reader::Reset() { *this = {}; }

}
