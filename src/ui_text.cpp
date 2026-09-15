#include "ui_text.h"
#include "speech_strings.h"

#include <algorithm>

namespace D2RAccess::UiText {
namespace {
constexpr uint64_t SettleMs = 80;

void Append(std::string& result, std::string_view part) {
	if (part.empty()) return;
	if (!result.empty()) result += ". ";
	result += part;
}

bool Interactive(Kind kind) {
	return kind == Kind::Button || kind == Kind::ListItem || kind == Kind::List || kind == Kind::Focusable;
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
	std::vector<std::string> seen;
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		const Node& node = snapshot.nodes[i];
		if (node.kind != Kind::Text) continue;
		bool inControl = false;
		for (int parent = node.parent; parent >= 0 && parent < static_cast<int>(i);) {
			if (Interactive(snapshot.nodes[static_cast<size_t>(parent)].kind)) { inControl = true; break; }
			const int next = snapshot.nodes[static_cast<size_t>(parent)].parent;
			if (next >= parent) break;
			parent = next;
		}
		if (inControl) continue; // never enumerate the menu's unselected options
		const auto text = Clean(node.text);
		if (text.empty() || std::find(seen.begin(), seen.end(), text) != seen.end()) continue;
		seen.push_back(text);
		Append(result.body, text);
	}
	if (result.surface == Surface::Menu && result.body.empty()) result.body = Strings::Current().dialogueMenu;
	return result;
}

Announcement Reader::Update(const Readout& readout, uint64_t now) {
	if (readout.Empty()) { pending_ = false; return {}; }
	if (!pending_ || readout != candidate_) {
		candidate_ = readout;
		candidateSince_ = now;
		pending_ = true;
		return {};
	}
	if (now - candidateSince_ < SettleMs || readout == spoken_) return {};
	const bool contextChanged = readout.context != spoken_.context || readout.body != spoken_.body;
	std::string phrase;
	if (contextChanged) Append(phrase, readout.body);
	if (contextChanged || readout.option != spoken_.option || readout.focusKey != spoken_.focusKey) Append(phrase, readout.option);
	spoken_ = readout;
	return { std::move(phrase), true };
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
	return phrase;
}
}
