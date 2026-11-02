#include "credits_reader.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "ui_text.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace D2RAccess::CreditsReader {

namespace {

constexpr std::string_view CreditsType = "CreditsWidget";
constexpr uint64_t PollMs = 100;      // a line takes many frames to scroll in
constexpr size_t MaxSearch = 512;     // widgets looked through for the credits, as NativeUi::Snapshot
// The CreditsWidget has no children: its lines are text boxes named "CreditsTextEntry" that it
// embeds in its own object, under no tree (deep probes of 2026-11-02, docs/native-ui.md: the first
// line's box at +0xB78 in two runs). They are found by that name inside the object, each checked for
// a game vtable before its name is trusted.
constexpr std::string_view EntryName = "CreditsTextEntry";
constexpr size_t EntrySearchSpan = 0x4000; // the CreditsWidget object and the boxes it embeds
constexpr uint64_t RescanMs = 1000;        // a box the credits start using later is found this soon
// Silent this long after opening: what was found is logged once.
constexpr uint64_t DiagnoseAfterMs = 3000;

std::string g_panel;                  // the active panel last looked at
uintptr_t   g_credits = 0;            // its CreditsWidget; 0 when it has none
std::vector<uintptr_t> g_entries;     // the line boxes found in it
uint64_t    g_scanned = 0;
uint64_t    g_polled = 0;
uint64_t    g_openedAt = 0;
bool        g_spoke = false;
bool        g_diagnosed = false;
std::unordered_map<uintptr_t, std::string> g_shown; // the line boxes on screen, by what they said

uintptr_t FindCredits(uintptr_t root) {
	std::vector<uintptr_t> queue{root};
	for (size_t at = 0; at < queue.size() && at < MaxSearch; ++at) {
		if (NativeUi::Classify(queue[at]).name == CreditsType) return queue[at];
		const auto children = NativeUi::Children(queue[at]);
		queue.insert(queue.end(), children.begin(), children.end());
	}
	return 0;
}

bool IsEntry(uintptr_t object) {
	return NativeWidgets::HasGameVtable(object) && NativeWidgets::ReadName(object) == EntryName;
}

std::vector<uintptr_t> FindEntries(uintptr_t credits) {
	std::vector<uintptr_t> entries;
	for (size_t offset = sizeof(uintptr_t); offset < EntrySearchSpan; offset += sizeof(uintptr_t))
		if (IsEntry(credits + offset)) entries.push_back(credits + offset);
	return entries;
}

// The line boxes on screen with their words, top first. Boxes drawn by hand may never be flagged
// shown: when none is, every box with words counts.
std::vector<std::pair<uintptr_t, std::string>> Lines() {
	struct Line { int y; uintptr_t box; std::string text; };
	const bool anyShown = std::any_of(g_entries.begin(), g_entries.end(), [](uintptr_t entry) { return NativeUi::Visible(entry); });
	std::vector<Line> lines;
	for (const uintptr_t entry : g_entries) {
		if (!IsEntry(entry) || (anyShown && !NativeUi::Visible(entry))) continue;
		std::string text = UiText::Clean(NativeWidgets::ReadText(entry));
		if (text.empty()) continue;
		const auto rect = NativeUi::LocalRect(entry);
		lines.push_back({rect ? rect->y : 0, entry, std::move(text)});
	}
	std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.y < b.y; });
	std::vector<std::pair<uintptr_t, std::string>> result;
	for (auto& line : lines) result.emplace_back(line.box, std::move(line.text));
	return result;
}

void Diagnose(const D2RL::PluginContext* context) {
	std::string boxes;
	for (const uintptr_t entry : g_entries) {
		char item[96] {};
		std::snprintf(item, sizeof(item), " +0x%zX(%s,\"%.24s\")", entry - g_credits, NativeUi::Visible(entry) ? "shown" : "hidden",
			UiText::Clean(NativeWidgets::ReadText(entry)).c_str());
		boxes += item;
	}
	AsyncLog::InfoF(context, "[credits] silent: %zu line boxes:%s", g_entries.size(), boxes.c_str());
	NativeWidgets::LogStringsDeep(context, g_credits, "credits");
}

}

void Tick(const D2RL::PluginContext* context) {
	const uint64_t now = GetTickCount64();
	if (now - g_polled < PollMs || !NativeWidgets::SupportsTree()) return;
	g_polled = now;
	const NativeMemory::ReadScope scope;
	if (const std::string_view active = PanelState::Active(); active != g_panel) {
		g_panel = active;
		g_shown.clear();
		g_openedAt = now;
		g_spoke = g_diagnosed = false;
		g_entries.clear();
		g_scanned = 0;
		const uintptr_t panel = active.empty() ? 0 : NativeWidgets::FindPanel(active);
		g_credits = panel ? FindCredits(panel) : 0;
		if (g_credits) AsyncLog::InfoF(context, "[credits] %s rolls credits: each line read as it comes on screen", g_panel.c_str());
	}
	if (!g_credits || !NativeUi::Visible(g_credits) || NativeUi::Classify(g_credits).name != CreditsType) return;
	if (now - g_scanned >= RescanMs) {
		g_scanned = now;
		if (auto entries = FindEntries(g_credits); entries.size() != g_entries.size()) {
			g_entries = std::move(entries);
			AsyncLog::InfoF(context, "[credits] %zu line boxes in the credits", g_entries.size());
		}
	}

	// A line newly on screen, or a box reused for the next line, is said; one that scrolled off
	// is forgotten, so a name the credits give twice is said both times.
	std::unordered_map<uintptr_t, std::string> shown;
	std::string said;
	for (auto& [box, text] : Lines()) {
		if (const auto before = g_shown.find(box); before == g_shown.end() || before->second != text) {
			if (!said.empty()) said += ", ";
			said += text;
		}
		shown.emplace(box, std::move(text));
	}
	g_shown = std::move(shown);
	if (!said.empty()) {
		g_spoke = true;
		ScreenReader::Speak(said, false);
	} else if (!g_spoke && !g_diagnosed && now - g_openedAt >= DiagnoseAfterMs) {
		g_diagnosed = true;
		Diagnose(context);
	}
}

}
