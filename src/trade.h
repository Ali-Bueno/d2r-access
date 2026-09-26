#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// What a player-to-player trade announces. See docs/trade.md.
namespace D2RAccess::Trade {
struct State {
	bool pending = false;   // a request is out or in, not yet agreed
	bool active = false;    // both agreed: the trade window is live
	bool requester = false; // this client asked
	int32_t partner = -1;   // the partner's unit id
	uint32_t partnerGold = 0;
	uint64_t partnerItems = 0;
	bool partnerAccepted = false, weAccepted = false;
	bool operator==(const State&) const = default;
};

enum class Event {
	PartnerItems, PartnerGold, PartnerAccepted, PartnerWithdrew, WeAccepted, WeWithdrew,
	Completed, Cancelled, Ended // Ended: no completion signal to tell the two apart
};

class Tracker {
public:
	// `completed`: a "trade completed" reply arrived since the last update;
	// `hooked`: that signal exists at all. The first update is the baseline.
	std::vector<Event> Update(const State& state, bool completed, bool hooked);
	void Reset();

private:
	std::optional<State> last_;
	bool completed_ = false;
};

std::string Phrase(Event event, const State& state, std::string_view partner);
// The trade prompt's line with the partner named, "" when `line` is neither of
// the game's own nameless prompts (`agree` = strTradeAgreeTo, `asking` =
// strWaitingForOtherPlayer).
std::string NamedPrompt(std::string_view line, std::string_view agree, std::string_view asking, std::string_view partner);
// The game appends some lines to a player's name ("canaria" + "no está
// disponible.") and esES drops their leading space; this puts it back.
std::string SpacedNameSuffix(std::string line, std::string_view suffix);
}
