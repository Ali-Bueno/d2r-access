#include "multiplayer_reader.h"

#include "async_log.h"
#include "multiplayer_status.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "ui_text.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::MultiplayerReader {
namespace {
constexpr std::string_view PanelName = "D2RLoaderMultiplayerPanel";
constexpr uint64_t PollMs = 160;
constexpr size_t MaxDetailNodes = 64;
constexpr size_t MaxDetailTexts = 12;
MultiplayerStatus::Reader g_reader;
uint64_t g_lastPoll = 0;
bool g_wasOpen = false;
bool g_enabled = true;

uintptr_t Child(uintptr_t parent, std::string_view name) {
	return parent ? NativeWidgets::FindChild(parent, name) : 0;
}

std::string VisibleText(uintptr_t widget) {
	if (!NativeUi::Visible(widget)) return {};
	return UiText::Clean(NativeWidgets::ReadText(widget));
}

bool SensitiveName(std::string_view name) {
	std::string lower(name);
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower.find("password") != std::string::npos || lower.find("address") != std::string::npos
		|| lower.find("publicip") != std::string::npos || lower.find("localip") != std::string::npos;
}

std::vector<std::string> SelectedDetails(uintptr_t container) {
	std::vector<std::string> result;
	if (!NativeUi::Visible(container)) return result;
	std::vector<uintptr_t> pending{container};
	for (size_t i = 0; i < pending.size() && pending.size() <= MaxDetailNodes && result.size() < MaxDetailTexts; ++i) {
		const uintptr_t widget = pending[i];
		if (!NativeUi::Visible(widget)) continue;
		const auto name = NativeWidgets::ReadName(widget);
		if (name == "SelectedGameEmptyText" || SensitiveName(name)) continue;
		if (NativeUi::Classify(widget).kind == UiText::Kind::Text) {
			auto text = VisibleText(widget);
			if (!text.empty() && std::find(result.begin(), result.end(), text) == result.end())
				result.push_back(std::move(text));
		}
		for (uintptr_t child : NativeUi::Children(widget)) {
			if (pending.size() >= MaxDetailNodes) break;
			pending.push_back(child);
		}
	}
	return result;
}
}

void Initialize(const D2RL::PluginContext* context) {
	Reset();
	g_enabled = RadarConfig::ReadEnabled(context, "[multiplayer]").value_or(true);
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_enabled || !NativeWidgets::SupportsTree()) return;
	const uint64_t now = GetTickCount64();
	if (now - g_lastPoll < PollMs) return;
	g_lastPoll = now;
	const NativeMemory::ReadScope readScope;
	const uintptr_t panel = NativeWidgets::FindPanel(PanelName);
	if (!NativeUi::Visible(panel)) {
		if (g_wasOpen) g_reader.Reset();
		g_wasOpen = false;
		return;
	}
	g_wasOpen = true;
	const auto focus = NativeUi::CurrentFocus();
	if (focus.panel && focus.panel != panel) return;
	if (NativeUi::Visible(Child(panel, "DirectJoinContainer"))) return;

	MultiplayerStatus::Snapshot snapshot;
	const uintptr_t status = Child(panel, "HostStatusStrip");
	snapshot.hostingLabel = VisibleText(Child(status, "HostingStatusLabel"));
	snapshot.hostingValue = VisibleText(Child(status, "HostingStatusText"));
	const uintptr_t browse = Child(panel, "BrowseContainer");
	const uintptr_t empty = Child(Child(browse, "BrowseLeftColumn"), "BrowseEmptyState");
	snapshot.browseEmpty = VisibleText(empty);
	if (NativeUi::Visible(browse)) {
		const uintptr_t details = Child(browse, "SelectedGameDetails");
		if (!NativeUi::Visible(Child(details, "SelectedGameEmptyText")))
			snapshot.selectedDetails = SelectedDetails(details);
	}
	for (const auto& phrase : g_reader.Update(snapshot, now)) {
		if (phrase.empty()) continue;
		AsyncLog::InfoF(context, "[multiplayer] %s", phrase.c_str());
		ScreenReader::Speak(phrase, false);
	}
}

void Reset() {
	g_reader.Reset();
	g_lastPoll = 0;
	g_wasOpen = false;
}

}
