#include "tab_reader.h"
#include "native_widgets.h"

#include <algorithm>
#include <unordered_map>

namespace D2RAccess::TabReader {
namespace {
struct Panel { uintptr_t id; std::vector<uintptr_t> tabs; };
std::vector<Panel> g_panels;
// A bar's selection is its index plus caption: two tabs may share a caption.
struct Selection { int index = -1; std::string text; };
std::unordered_map<uintptr_t, Selection> g_selected;
bool g_observing = false;
std::string g_detail;
constexpr std::string_view SwitcherTabs = "UISwitcherTabs";

void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += ". ";
	text += part;
}
}

std::string Update(std::span<const uintptr_t> panels, NativeUi::Focus focus, bool suspended) {
	if (!focus.panel) {
		Reset();
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
	std::unordered_map<uintptr_t, Selection> next;
	std::string changed;
	for (const auto& panel : g_panels) {
		for (uintptr_t tab : panel.tabs) {
			Selection now { NativeUi::SelectedTab(tab), UiText::Clean(NativeUi::SelectedTabText(tab)) };
			if (now.text.empty() || next.contains(tab)) continue;
			const auto prior = g_selected.find(tab);
			const bool isSwitcher = NativeWidgets::ReadName(tab) == SwitcherTabs;
			// The switcher's dialogue tabs (quest log) name themselves; keep the
			// old baseline so the tab after them is named on return.
			if (suspended && isSwitcher && prior != g_selected.end()) {
				next.emplace(tab, prior->second);
				continue;
			}
			const bool differs = prior == g_selected.end() || prior->second.index != now.index || prior->second.text != now.text;
			if (g_observing && differs) {
				Append(changed, now.text);
				g_detail = "bar " + NativeWidgets::ReadName(tab) + " idx=" + std::to_string(now.index) + " text=" + now.text;
			}
			next.emplace(tab, std::move(now));
		}
	}
	g_selected = std::move(next);
	g_observing = true; // entering a menu already has a title/item announcement
	return changed;
}

std::string Detail() { return g_detail; }

std::string Selected(uintptr_t panel) {
	std::string result;
	for (const auto& entry : g_panels) if (entry.id == panel) {
		for (uintptr_t tab : entry.tabs) {
			const auto it = g_selected.find(tab);
			if (it != g_selected.end()) Append(result, it->second.text);
		}
	}
	return result;
}

void Reset() { g_panels.clear(); g_selected.clear(); g_observing = false; g_detail.clear(); }
}
