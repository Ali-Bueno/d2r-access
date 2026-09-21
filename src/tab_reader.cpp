#include "tab_reader.h"
#include "native_widgets.h"

#include <algorithm>
#include <unordered_map>

namespace D2RAccess::TabReader {
namespace {
struct Panel { uintptr_t id; std::vector<uintptr_t> tabs; };
std::vector<Panel> g_panels;
std::unordered_map<uintptr_t, std::string> g_selected;
bool g_observing = false;
// The menu switcher's label as last seen, kept while a dialogue suspends this
// reader. The quest log is one of the switcher's tabs and suspends it, so the
// tab after it came back as a silent baseline -- and the Options tab has no
// window title to name it instead (the user, 2026-09-21: Options was heard
// only as its first button, "Save and Exit").
struct Held { uintptr_t tab = 0; std::string text; };
Held g_switcher;
constexpr std::string_view SwitcherTabs = "UISwitcherTabs";

void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}
}

std::string Update(std::span<const uintptr_t> panels, NativeUi::Focus focus, bool suspended) {
	if (suspended || !focus.panel) {
		const Held kept = g_switcher;
		Reset();
		if (suspended) g_switcher = kept;
		return {};
	}
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
	bool switcherSeen = false;
	for (const auto& panel : g_panels) {
		for (uintptr_t tab : panel.tabs) {
			const auto text = UiText::Clean(NativeUi::SelectedTabText(tab));
			if (text.empty() || !next.emplace(tab, text).second) continue;
			const auto prior = g_selected.find(tab);
			if (g_observing && (prior == g_selected.end() || prior->second != text)) Append(changed, text);
			if (NativeWidgets::ReadName(tab) != SwitcherTabs) continue;
			// Back from a dialogue: the switcher moved on while this was not looking.
			if (!g_observing && g_switcher.tab == tab && !g_switcher.text.empty() && g_switcher.text != text) Append(changed, text);
			g_switcher = {tab, text};
			switcherSeen = true;
		}
	}
	if (!switcherSeen) g_switcher = {};
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

void Reset() { g_panels.clear(); g_selected.clear(); g_observing = false; g_switcher = {}; }
}
