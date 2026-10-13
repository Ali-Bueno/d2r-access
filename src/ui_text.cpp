#include "ui_text.h"
#include "speech_strings.h"

#include <algorithm>

namespace D2RAccess::UiText {
namespace {
void Append(std::string& result, std::string_view part) {
	if (part.empty()) return;
	if (!result.empty()) result += result.ends_with('.') ? " " : ". ";
	result += part;
}

// The quest panel's own static heading, beside the ShortDescription /
// LongDescription detail boxes (game-data/index/QuestLogPanel*.json.index.json).
constexpr std::string_view QuestPanelHeading = "Title";
constexpr std::string_view QuestSpeechButton = "Speech";

bool Interactive(Kind kind) {
	return kind == Kind::Button || kind == Kind::StatButton || kind == Kind::TabButton
		|| kind == Kind::ListItem || kind == Kind::List || kind == Kind::Focusable
		|| kind == Kind::QuestButton || kind == Kind::TextInput;
}

bool Under(const Snapshot& snapshot, size_t index, int ancestor) {
	for (int current = static_cast<int>(index); current >= 0;) {
		if (current == ancestor) return true;
		const int parent = snapshot.nodes[static_cast<size_t>(current)].parent;
		if (parent >= current) break; // malformed snapshot, including cycles
		current = parent;
	}
	return false;
}

// Text that hangs under a button or list row belongs to that control, never to
// the surface's own body.
bool InsideControl(const Snapshot& snapshot, size_t index, bool includeTabs = false) {
	for (int parent = snapshot.nodes[index].parent; parent >= 0 && parent < static_cast<int>(index);) {
		const auto kind = snapshot.nodes[static_cast<size_t>(parent)].kind;
		if (Interactive(kind) || (includeTabs && kind == Kind::Tabs)) return true;
		const int next = snapshot.nodes[static_cast<size_t>(parent)].parent;
		if (next >= parent) break; // malformed snapshot, including cycles
		parent = next;
	}
	return false;
}

int FocusIndex(const Snapshot& snapshot) {
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		if (snapshot.nodes[i].id == snapshot.focus && snapshot.focus != 0) return static_cast<int>(i);
	}
	return -1;
}

std::string TextUnder(const Snapshot& snapshot, int ancestor) {
	std::string result;
	std::vector<std::string> seen;
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		if (!Under(snapshot, i, ancestor)) continue;
		bool nestedControl = false;
		for (int current = static_cast<int>(i); current > ancestor;) {
			if (Interactive(snapshot.nodes[static_cast<size_t>(current)].kind)) { nestedControl = true; break; }
			const int parent = snapshot.nodes[static_cast<size_t>(current)].parent;
			if (parent >= current) break;
			current = parent;
		}
		if (nestedControl) continue;
		const auto text = Clean(snapshot.nodes[i].text);
		if (text.empty() || std::find(seen.begin(), seen.end(), text) != seen.end()) continue;
		seen.push_back(text);
		Append(result, text);
	}
	return result;
}
}

std::string Clean(std::string_view text) {
	std::string result;
	bool space = false;
	for (size_t i = 0; i < text.size();) {
		// Diablo inline color escape: U+00FF + 'c' + palette character.
		// Accept both the UTF-8 representation and the game's legacy byte form.
		size_t marker = 0;
		if (text.substr(i, 2) == "\xC3\xBF") marker = 2;
		else if (static_cast<unsigned char>(text[i]) == 0xFF) marker = 1;
		if (marker && i + marker + 1 < text.size() && text[i + marker] == 'c') {
			i += marker + 2;
			continue;
		}
		// The game's private-use font characters are controller/button images,
		// not pronounceable text (e.g. U+E013 beside the stash target label).
		if (i + 2 < text.size()) {
			const auto a = static_cast<unsigned char>(text[i]);
			const auto b = static_cast<unsigned char>(text[i + 1]);
			const auto c = static_cast<unsigned char>(text[i + 2]);
			if (a >= 0xEE && a <= 0xEF && (b & 0xC0) == 0x80 && (c & 0xC0) == 0x80) {
				const unsigned code = ((a & 0x0F) << 12) | ((b & 0x3F) << 6) | (c & 0x3F);
				if (code >= 0xE000 && code <= 0xF8FF) {
					i += 3;
					// The font command takes exactly one index character: docs/native-tooltips.md.
					if (code == FontCommand && i < text.size() && FontIndex(text[i])) ++i;
					space = !result.empty();
					continue;
				}
			}
		}
		const unsigned char ch = static_cast<unsigned char>(text[i++]);
		if (ch <= ' ' || ch == '|') { space = !result.empty(); continue; }
		if (space) result += ' ';
		space = false;
		result += static_cast<char>(ch);
	}
	return result;
}

Readout Describe(const Snapshot& snapshot) {
	Readout result;
	result.context = snapshot.name;
	result.surface = snapshot.surface;
	int focus = FocusIndex(snapshot);
	// A child label may own focus; use its nearest action/list row as a unit.
	if (focus >= 0 && !Interactive(snapshot.nodes[static_cast<size_t>(focus)].kind)) {
		const int parent = snapshot.nodes[static_cast<size_t>(focus)].parent;
		if (parent >= 0 && parent < focus && Interactive(snapshot.nodes[static_cast<size_t>(parent)].kind)) focus = parent;
	}
	if (focus >= 0 && Interactive(snapshot.nodes[static_cast<size_t>(focus)].kind)) {
		result.option = TextUnder(snapshot, focus);
		if (!result.option.empty()) {
			const Node& node = snapshot.nodes[static_cast<size_t>(focus)];
			// Tree position survives recreation of the same row at a new address.
			result.focusKey = std::to_string(focus);
			if (!node.enabled) Append(result.option, Strings::Current().unavailable);
		}
	}
	if (snapshot.surface == Surface::Other) return result;
	if (focus >= 0 && snapshot.nodes[static_cast<size_t>(focus)].kind == Kind::QuestButton) {
		// Quest icons share one detail pane. The game clears/repopulates it after
		// moving focus; never attach the previous quest's objective to the new icon.
		std::string title, objective;
		for (const auto& node : snapshot.nodes) {
			if (node.kind != Kind::Text) continue;
			if (node.name == "ShortDescription") title = Clean(node.text);
			if (node.name == "LongDescription") objective = Clean(node.text);
		}
		if (result.option.empty()) return result;
		// Live controller navigation takes ~150 ms to repopulate the shared pane.
		result.contentSettleMs = 150;
		// Catalog names also exist for empty sockets. Only the matching live
		// heading establishes that this quest has details (docs/quests.md).
		if (title.empty() || title != Clean(snapshot.nodes[static_cast<size_t>(focus)].text)) {
			result.option = Strings::Current().slotEmpty;
			return result;
		}
		result.body = result.option;
		result.option = objective.empty() ? Strings::Current().questNoObjective : objective;
		std::vector<std::string> seenDetail{title, objective};
		for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
			const auto& node = snapshot.nodes[i];
			if (node.kind == Kind::Button && node.name == QuestSpeechButton && node.enabled) {
				result.help = TextUnder(snapshot, static_cast<int>(i));
				continue;
			}
			if (node.kind != Kind::Text && node.kind != Kind::LinkedText) continue;
			if (node.name == QuestPanelHeading || InsideControl(snapshot, i, true)) continue;
			const auto text = Clean(node.text);
			if (text.empty() || std::find(seenDetail.begin(), seenDetail.end(), text) != seenDetail.end()) continue;
			seenDetail.push_back(text);
			Append(result.option, text);
		}
		return result;
	}
	std::vector<std::string> seen;
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		const Node& node = snapshot.nodes[i];
		if (node.kind != Kind::Text && node.kind != Kind::LinkedText) continue;
		if (InsideControl(snapshot, i)) continue; // never enumerate the menu's unselected options
		const auto text = Clean(node.text);
		if (text.empty() || std::find(seen.begin(), seen.end(), text) != seen.end()) continue;
		seen.push_back(text);
		Append(result.body, text);
	}
	if (result.surface == Surface::Menu && result.body.empty()) result.body = Strings::Current().dialogueMenu;
	return result;
}

Announcement Reader::Update(const Readout& readout, uint64_t now, uint64_t settleMs) {
	if (readout.Empty()) { pending_ = false; return {}; }
	settleMs = std::max(settleMs, readout.contentSettleMs);
	// Existing menu rows already have complete labels. Moving between them
	// needs no population delay; new contexts and recycled text still settle.
	if (!readout.contentSettleMs && !spoken_.Empty() && readout.surface == Surface::Menu && readout.context == spoken_.context
		&& readout.body == spoken_.body && !readout.option.empty() && readout.focusKey != spoken_.focusKey) settleMs = 0;
	if (!pending_ || readout != candidate_) {
		candidate_ = readout;
		candidateSince_ = now;
		pending_ = true;
	}
	if (now - candidateSince_ < settleMs || readout == spoken_) return {};
	const bool contextChanged = readout.context != spoken_.context || readout.body != spoken_.body;
	std::string phrase;
	if (contextChanged) Append(phrase, readout.body);
	if (contextChanged || readout.option != spoken_.option || readout.focusKey != spoken_.focusKey) Append(phrase, readout.option);
	if (contextChanged || readout.coordinates != spoken_.coordinates || readout.focusKey != spoken_.focusKey)
		Append(phrase, readout.coordinates);
	const bool sameFocus = !contextChanged && readout.focusKey == spoken_.focusKey;
	const bool interrupt = !readout.contentSettleMs || !sameFocus;
	spoken_ = readout;
	return { std::move(phrase), interrupt, sameFocus };
}

void Reader::Reset() {
	candidate_ = {};
	spoken_ = {};
	candidateSince_ = 0;
	pending_ = false;
}

std::string Reader::Repeat() const {
	std::string phrase = spoken_.body;
	Append(phrase, spoken_.option);
	Append(phrase, spoken_.coordinates);
	return phrase;
}
}
