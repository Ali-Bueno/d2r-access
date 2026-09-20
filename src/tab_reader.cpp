#include "tab_reader.h"

#include "speech_strings.h"

#include <cstdio>

#include <algorithm>
#include <unordered_map>

namespace D2RAccess::TabReader {
namespace {
struct Panel { uintptr_t id; std::vector<uintptr_t> tabs; };
std::vector<Panel> g_panels;
std::unordered_map<uintptr_t, std::string> g_selected;
bool g_observing = false;

void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}
}

std::string Update(std::span<const uintptr_t> panels, NativeUi::Focus focus, bool suspended) {
	if (suspended || !focus.panel) { Reset(); return {}; }
	std::vector<uintptr_t> visible;
	for (uintptr_t panel : panels) {
		if (NativeUi::Visible(panel) && NativeUi::Classify(panel).surface == UiText::Surface::Other) visible.push_back(panel);
	}
	std::erase_if(g_panels, [&](const Panel& panel) { return std::find(visible.begin(), visible.end(), panel.id) == visible.end(); });
	for (uintptr_t panel : visible) {
		if (std::none_of(g_panels.begin(), g_panels.end(), [=](const Panel& entry) { return entry.id == panel; })) {
			// Include initially hidden controls in discovery: shared-stash sub-tabs
			// can appear later without changing the focused inventory widget.
			g_panels.push_back({panel, NativeUi::FindControls(panel, UiText::Kind::Tabs)});
		}
	}
	std::unordered_map<uintptr_t, std::string> next;
	std::string changed;
	for (const auto& panel : g_panels) {
		for (uintptr_t tab : panel.tabs) {
			const auto text = UiText::Clean(NativeUi::SelectedTabText(tab));
			if (text.empty() || !next.emplace(tab, text).second) continue;
			const auto prior = g_selected.find(tab);
			if (!g_observing || (prior != g_selected.end() && prior->second == text)) continue;
			// Say where along the strip the highlight landed, not just its
			// name: on a tab bar the position is the orientation. The count
			// walks the bar's label children, so it is only read here, when
			// something actually moved.
			std::string phrase = text;
			if (const auto position = NativeUi::SelectedTab(tab); position.index >= 0 && position.count > 1) {
				char suffix[80]{};
				std::snprintf(suffix, sizeof(suffix), Strings::Current().tabOfCount, position.index + 1, position.count);
				phrase += ", ";
				phrase += suffix;
			}
			Append(changed, phrase);
		}
	}
	g_selected = std::move(next);
	g_observing = true; // entering a menu already has a title/item announcement
	return changed;
}

std::string Selected(uintptr_t panel) {
	std::string result;
	for (const auto& entry : g_panels) if (entry.id == panel) {
		for (uintptr_t tab : entry.tabs) {
			const auto it = g_selected.find(tab);
			if (it != g_selected.end()) Append(result, it->second);
		}
	}
	return result;
}

void Reset() { g_panels.clear(); g_selected.clear(); g_observing = false; }
}
