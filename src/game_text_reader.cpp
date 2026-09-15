#include "game_text_reader.h"

#include "focus_tracker.h"
#include "interaction_reader.h"
#include "generated/panel_widgets.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "ui_text.h"
#include "ui_notifications.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <vector>

namespace D2RAccess::GameTextReader {
namespace {
using UiText::Surface;
constexpr uint64_t PollMs = 50; // 20 Hz, independent of the game's render rate
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleServiceV1* g_lifecycle = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::atomic<bool> g_enabled{true}, g_repeat{false}, g_dump{false};
bool g_inGame = false, g_ownedFocus = false, g_wasEnabled = true;
bool g_interactionFocus = false;
uint64_t g_lastPoll = 0;
UiText::Reader g_reader, g_areaReader;
UiNotifications::Reader g_notices;

void Reset() {
	if (g_ownedFocus || g_interactionFocus) ScreenReader::Stop();
	g_reader.Reset();
	g_areaReader.Reset();
	g_notices.Reset();
	InteractionReader::Reset();
	g_interactionFocus = false;
	g_ownedFocus = false;
	g_lastPoll = 0;
	g_repeat.store(false);
	FocusTracker::Reset();
}

void __cdecl OnGameplay(const D2RL::PluginContext* context, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	Reset();
	g_inGame = event->kind == D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady;
	D2RL::LogInfoF(context, "[text] gameplay event %u, session %llu, player ready %s",
		static_cast<unsigned>(event->kind), static_cast<unsigned long long>(event->sessionGeneration), g_inGame ? "yes" : "no");
}

// The existing catalog remains responsible for options/values and other
// established controls. Runtime list rows and uncatalogued buttons have a
// native text fallback, shared by all in-game panels.
bool NeedsFallback(const UiText::Snapshot& snapshot) {
	if (snapshot.nodes.empty()) return false;
	const auto& widget = snapshot.nodes.front();
	if (widget.kind == UiText::Kind::ListItem) return true;
	if (widget.kind != UiText::Kind::Button) return false;
	for (const auto& panel : Generated::PanelCatalog) {
		if (snapshot.name != panel.panel) continue;
		for (size_t i = 0; i < panel.count; ++i) {
			if (widget.name == panel.widgets[i].name) return false;
		}
	}
	return true;
}

int Priority(const UiText::Snapshot& snapshot, const NativeUi::Focus& focus) {
	// The panel receiving controller input is authoritative. When an automatic
	// message has no focus, read that overlay before the NPC menu underneath it.
	int score = snapshot.surface == Surface::Tip ? 3000 : snapshot.surface == Surface::Narrative ? 2000 : 1000;
	if (snapshot.panel == focus.panel) score += 100;
	const auto& open = PanelState::OpenPanels();
	const auto it = std::find(open.begin(), open.end(), snapshot.name);
	if (it != open.end()) score += static_cast<int>(it - open.begin());
	return score;
}

void Dump(const D2RL::PluginContext* context, const UiText::Snapshot& snapshot) {
	D2RL::LogInfoF(context, "[text-tree] %s panel=%p focus=%p nodes=%zu", snapshot.name.c_str(),
		reinterpret_cast<void*>(snapshot.panel), reinterpret_cast<void*>(snapshot.focus), snapshot.nodes.size());
	for (const auto& node : snapshot.nodes) {
		D2RL::LogInfoF(context, "[text-tree] parent=%d id=%p type=%s name=%s enabled=%u text=\"%s\"",
			node.parent, reinterpret_cast<void*>(node.id), node.type.c_str(), node.name.c_str(), node.enabled ? 1u : 0u, UiText::Clean(node.text).c_str());
	}
}

void Speak(const D2RL::PluginContext* context, const UiText::Readout& readout, const UiText::Announcement& announcement, bool interrupt) {
	if (announcement.text.empty()) return;
	D2RL::LogInfoF(context, "[text] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
	ScreenReader::Speak(announcement.text, interrupt);
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context || context->QueryService(D2RL::ServiceId::Lifecycle, D2RL::LifecycleServiceV1Version, &g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceV1Field(g_lifecycle, D2RL::LifecycleServiceV1RequiredSize)) return false;
	g_context = context;
	for (auto kind : { D2RL::Lifecycle::GameplayEventKind::GameJoined, D2RL::Lifecycle::GameplayEventKind::GameLeft, D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady }) {
		const D2RL::Lifecycle::GameplayEventListener listener{
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay,
		};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) {
			Unregister();
			return false;
		}
		g_listeners.push_back(handle);
	}
	context->LogInfo("[text] In-game text reader registered; waiting for local player.");
	return true;
}

void Unregister() {
	if (g_lifecycle && g_context) {
		for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	}
	g_listeners.clear();
	g_lifecycle = nullptr;
	g_context = nullptr;
	g_inGame = false;
	Reset();
	NativeUi::Reset();
}

bool Tick(const D2RL::PluginContext* context) {
	const bool enabled = g_enabled.load();
	if (enabled != g_wasEnabled) {
		Reset();
		g_wasEnabled = enabled;
		ScreenReader::Speak(enabled ? Strings::Current().gameTextEnabled : Strings::Current().gameTextDisabled, true);
	}
	if (!g_inGame || !enabled || !NativeWidgets::SupportsTree()) return false;
	const uint64_t now = GetTickCount64();
	if (now - g_lastPoll < PollMs) return g_ownedFocus || g_interactionFocus;
	g_lastPoll = now;
	const NativeUi::Focus focus = NativeUi::CurrentFocus();
	const auto focusedSurface = NativeUi::Classify(focus.panel).surface;
	const bool dump = g_dump.exchange(false);
	UiText::Readout active, area;
	std::vector<std::string> notices;
	std::vector<UiNotifications::Delivery> deliveries;
	int best = -1;
	auto panels = NativeUi::Panels();
	if (focus.panel && std::find(panels.begin(), panels.end(), focus.panel) == panels.end()) panels.push_back(focus.panel);
	for (uintptr_t panel : panels) {
		const auto type = NativeUi::Classify(panel);
		if (type.surface == Surface::Target) continue; // shared interaction reader, passive HUD target
		if (!NativeUi::Visible(panel) || (type.surface == Surface::Other && panel != focus.panel)) continue;
		const bool notification = type.surface == Surface::Notice || type.surface == Surface::NotificationFeed;
		// A different menu/modal receiving input covers this conversation. An
		// area banner is passive and may still be announced without taking focus.
		if (!notification && type.surface != Surface::Area && focus.widget && focus.panel != panel && focusedSurface == Surface::Other) continue;
		const auto snapshot = NativeUi::Snapshot(panel, panel == focus.panel ? focus.widget : 0);
		if (snapshot.nodes.empty()) continue;
		if (dump) Dump(context, snapshot);
		if (notification) {
			const auto messages = UiNotifications::Describe(snapshot);
			notices.insert(notices.end(), messages.begin(), messages.end());
			const auto latest = UiNotifications::Deliveries(snapshot);
			deliveries.insert(deliveries.end(), latest.begin(), latest.end());
			continue;
		}
		if (snapshot.surface == Surface::Other) continue;
		const auto readout = UiText::Describe(snapshot);
		if (readout.Empty()) continue;
		if (snapshot.surface == Surface::Area) { area = readout; continue; }
		const int priority = Priority(snapshot, focus);
		if (priority > best) { best = priority; active = readout; }
	}
	if (best < 0 && focus.widget) {
		auto snapshot = NativeUi::Snapshot(focus.widget, focus.widget);
		snapshot.name = NativeWidgets::ReadName(focus.panel);
		if (dump) Dump(context, snapshot);
		if (NeedsFallback(snapshot)) active = UiText::Describe(snapshot);
	}
	const bool owns = !active.Empty();
	if (owns) {
		// Menu creation may precede initial focus. Wait for the actual option.
		if (active.surface != Surface::Menu || !active.option.empty()) Speak(context, active, g_reader.Update(active, now), true);
	} else if (g_ownedFocus) {
		ScreenReader::Stop();
		g_reader.Reset();
		FocusTracker::Reset();
	}
	g_ownedFocus = owns;
	g_interactionFocus = InteractionReader::Tick(context, now, focus, owns);
	if (!area.Empty()) Speak(context, area, g_areaReader.Update(area, now), !owns && !g_interactionFocus);
	else g_areaReader.Reset();
	for (const auto& text : g_notices.Update(notices, now, deliveries)) {
		D2RL::LogInfoF(context, "[notice] %s", text.c_str());
		ScreenReader::Speak(text, false);
	}
	if (g_repeat.exchange(false)) {
		const auto phrase = owns ? g_reader.Repeat() : !InteractionReader::Repeat().empty() ? InteractionReader::Repeat()
			: !g_notices.Repeat().empty() ? g_notices.Repeat() : g_areaReader.Repeat();
		if (!phrase.empty()) ScreenReader::Speak(phrase, true);
	}
	return owns || g_interactionFocus;
}

void SetEnabled(bool enabled) { g_enabled.store(enabled); }
void RequestRepeat() { g_repeat.store(true); }
void RequestDump() { g_dump.store(true); }
}
