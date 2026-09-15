#include "game_text_reader.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <windows.h>
#include <algorithm>

namespace {
using namespace D2RAccess;
std::vector<UiText::Snapshot> panels;
NativeUi::Focus focus;
std::vector<UiText::Announcement> speech;
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
std::vector<std::string> openPanels;
int stops = 0, removed = 0;

auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener, D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	listeners.push_back(*listener);
	*handle = listeners.size();
	return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	++removed;
	return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleServiceV1 lifecycle{
	.serviceSize = D2RL::LifecycleServiceV1Size, .serviceVersion = D2RL::LifecycleServiceV1Version,
	.registerGameplayEventListener = RegisterListener, .unregisterGameplayEventListener = UnregisterListener,
};
auto Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t, const void** service) noexcept -> D2RL::ServiceQueryResult {
	if (id != D2RL::ServiceId::Lifecycle) return D2RL::ServiceQueryResult::Unavailable;
	*service = &lifecycle;
	return D2RL::ServiceQueryResult::Success;
}
const D2RL::PluginApi api{ .apiSize = D2RL::PluginApiSize, .queryService = Query };
const D2RL::PluginContext context{ .contextSize = sizeof(D2RL::PluginContext), .api = &api };

void Event(D2RL::Lifecycle::GameplayEventKind kind) {
	const D2RL::Lifecycle::GameplayEvent event{ .structSize = D2RL::Lifecycle::GameplayEventSize, .kind = kind, .sessionGeneration = 1 };
	for (const auto& listener : listeners) if (listener.kind == kind) listener.callback(&context, &event, nullptr);
}
bool Settle() {
	bool owns = false;
	for (int i = 0; i < 4; ++i) {
		Sleep(60);
		owns = GameTextReader::Tick(&context);
	}
	return owns;
}

UiText::Snapshot Menu() {
	return { 1, 2, "NpcDialogPanel", UiText::Surface::Menu, {
		{1, -1, UiText::Kind::Other, "Root", "NpcDialogPanel", ""},
		{3, 0, UiText::Kind::Text, "Title", "TextBoxWidget", "Akara"},
		{2, 0, UiText::Kind::Button, "Talk", "ButtonWidget", "Hablar"},
	} };
}
}

namespace D2RAccess::NativeUi {
Type Classify(uintptr_t widget) {
	for (const auto& panel : panels) if (panel.panel == widget) return { panel.name, UiText::Kind::Other, panel.surface };
	return {};
}
bool Visible(uintptr_t widget) { return widget != 0; }
Focus CurrentFocus() { return focus; }
std::vector<uintptr_t> Panels() {
	std::vector<uintptr_t> result;
	for (const auto& panel : panels) result.push_back(panel.panel);
	return result;
}
UiText::Snapshot Snapshot(uintptr_t widget, uintptr_t focused) {
	for (const auto& panel : panels) {
		if (panel.panel == widget) { auto result = panel; result.focus = focused; return result; }
		for (const auto& node : panel.nodes) if (node.id == widget) {
			auto result = UiText::Snapshot{widget, focused, panel.name, UiText::Surface::Other, {node}};
			result.nodes[0].parent = -1;
			return result;
		}
	}
	return {};
}
void Reset() {}
}
namespace D2RAccess::NativeWidgets {
bool SupportsTree() { return true; }
std::string ReadName(uintptr_t widget) {
	for (const auto& panel : panels) if (panel.panel == widget) return panel.name;
	return {};
}
}
namespace D2RAccess::FocusTracker { void Reset() {} }
namespace D2RAccess::PanelState { const std::vector<std::string>& OpenPanels() { return openPanels; } }
namespace D2RAccess::ScreenReader {
void Stop() { ++stops; }
void Speak(std::string_view text, bool interrupt) { speech.push_back({std::string(text), interrupt}); }
}

int main() {
	using namespace D2RAccess;
	using D2RL::Lifecycle::GameplayEventKind;
	Strings::Select(Strings::Language::Spanish);
	Check(GameTextReader::Register(&context) && listeners.size() == 3, "subscribes to session lifecycle");
	panels = {Menu()}; focus = {1, 2};
	Check(!Settle() && speech.empty(), "preloaded in-game UI is silent in front end");
	Event(GameplayEventKind::GameJoined);
	Check(!Settle() && speech.empty(), "loading waits for the local player");
	Event(GameplayEventKind::LocalPlayerReady);
	Check(Settle() && speech.size() == 1 && speech.back().text == "Akara. Hablar", "ready player enables conversation and owns focus");
	speech.clear(); Settle();
	Check(speech.empty(), "unchanged menu remains silent across polls");
	panels.push_back({10, 11, "ScrollSpeechPanel", UiText::Surface::Narrative, {
		{10, -1, UiText::Kind::Other, "Root", "ScrollSpeechPanel", ""},
		{11, 0, UiText::Kind::Text, "ScrollingText", "TextBoxWidget", std::string(5000, 'a')},
	}});
	focus = {10, 11};
	Check(Settle() && speech.size() == 1 && speech.back().text.size() == 5000, "complete speech replaces NPC menu");
	speech.clear();
	GameTextReader::RequestRepeat(); Settle();
	Check(speech.size() == 1 && speech.back().text.size() == 5000, "manual repeat retains full speech");
	speech.clear(); panels.pop_back(); focus = {1, 2};
	Check(Settle() && speech.size() == 1 && speech.back().text == "Akara. Hablar", "closing dialogue restores the NPC menu");
	speech.clear();
	panels.push_back({20, 21, "ConfirmationModal", UiText::Surface::Other, {{21, -1, UiText::Kind::Button, "Confirm", "ButtonWidget", "Aplicar"}}});
	focus = {20, 21}; const int beforeStop = stops;
	Check(!Settle() && speech.empty() && stops > beforeStop, "another modal covers NPC and returns focus to existing reader");
	panels.pop_back(); focus = {1, 2}; speech.clear();
	Check(Settle() && speech.size() == 1, "return from a covering modal reannounces the NPC");
	speech.clear();
	panels.push_back({30, 0, "HUDLevelNames", UiText::Surface::Area, {{31, -1, UiText::Kind::Text, "NameText", "TextBoxWidget", "Páramo Sangriento"}}});
	Settle();
	Check(speech.size() == 1 && !speech.back().interrupt, "area name never interrupts an active conversation");
	speech.clear(); GameTextReader::SetEnabled(false);
	Check(!Settle() && speech.size() == 1 && speech.back().text == Strings::Current().gameTextDisabled, "feature can be disabled");
	speech.clear(); GameTextReader::SetEnabled(true); Settle();
	Check(!speech.empty() && speech.front().text == Strings::Current().gameTextEnabled, "reenabling announces mode and rebuilds context");
	speech.clear(); Event(GameplayEventKind::GameLeft);
	Check(!Settle() && speech.empty(), "leaving the game clears speech context");
	GameTextReader::Unregister();
	Check(removed == 3, "all lifecycle listeners are removed");
	Passed();
}
