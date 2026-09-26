#include "trade.h"
#include "speech_strings.h"
#include <cstdio>

namespace D2RAccess::Trade {
namespace {
std::string Format(const char* format, std::string_view first, const std::string& second = {}) {
	char phrase[256]{};
	std::snprintf(phrase, sizeof(phrase), format, std::string(first).c_str(), second.c_str());
	return phrase;
}
}

std::vector<Event> Tracker::Update(const State& state, bool completed, bool hooked) {
	std::vector<Event> events;
	if (completed) completed_ = true;
	if (!last_) { last_ = state; return events; }
	const State& last = *last_;
	// A new trade starts from nothing offered; an old completion is not its own.
	if (!last.active && state.active) completed_ = false;
	else if (last.active && state.active) {
		if (state.partnerItems != last.partnerItems) events.push_back(Event::PartnerItems);
		if (state.partnerGold != last.partnerGold) events.push_back(Event::PartnerGold);
		if (state.partnerAccepted != last.partnerAccepted)
			events.push_back(state.partnerAccepted ? Event::PartnerAccepted : Event::PartnerWithdrew);
		if (last.weAccepted && !state.weAccepted) events.push_back(Event::WeWithdrew);
	} else if (last.active && !state.active) {
		events.push_back(completed_ ? Event::Completed : hooked ? Event::Cancelled : Event::Ended);
		completed_ = false;
	}
	last_ = state;
	return events;
}

void Tracker::Reset() {
	last_.reset();
	completed_ = false;
}

std::string Phrase(Event event, const State& state, std::string_view partner) {
	const auto& strings = Strings::Current();
	if (partner.empty()) partner = strings.tradePartnerUnknown;
	switch (event) {
	case Event::PartnerItems: return Format(strings.tradePartnerItems, partner);
	case Event::PartnerGold: return Format(strings.tradePartnerGold, partner, std::to_string(state.partnerGold));
	case Event::PartnerAccepted: return Format(strings.tradePartnerAccepted, partner);
	case Event::PartnerWithdrew: return Format(strings.tradePartnerWithdrew, partner);
	case Event::WeWithdrew: return strings.tradeWeWithdrew;
	case Event::Completed: return strings.tradeCompleted;
	case Event::Cancelled: return strings.tradeCancelled;
	case Event::Ended: return strings.tradeEnded;
	}
	return {};
}

std::string NamedPrompt(std::string_view line, std::string_view agree, std::string_view asking, std::string_view partner) {
	if (partner.empty() || line.empty()) return {};
	if (line == agree) return Format(Strings::Current().tradeRequested, partner);
	if (line == asking) return Format(Strings::Current().tradeAsking, partner);
	return {};
}

std::string SpacedNameSuffix(std::string line, std::string_view suffix) {
	if (suffix.empty() || suffix.front() == ' ' || line.size() <= suffix.size() || !line.ends_with(suffix)) return line;
	const size_t at = line.size() - suffix.size();
	if (line[at - 1] != ' ') line.insert(at, 1, ' ');
	return line;
}
}
