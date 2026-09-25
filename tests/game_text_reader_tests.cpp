#include "game_text_reader.h"
#include "interaction_reader.h"
#include "tab_reader.h"
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
std::string tabChange; // what TabReader reports this tick
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
std::vector<std::string> openPanels;
int stops = 0, removed = 0;
bool inventoryOwns = false;
int interactionTicks = 0;
int inventorySnapshots = 0;
bool npcAudioOnly = false;
std::string questHelp;
int questHelpReads = 0;
bool automaticHints = false;

auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener, D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	listeners.push_back(*listener);
	*handle = listeners.size();
	return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	++removed;
	return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleService lifecycle{
	.serviceSize = D2RL::LifecycleServiceSize, .serviceVersion = D2RL::LifecycleService::AbiVersion,
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
	if (widget == 80 || widget == 81 || widget == 82) ++inventorySnapshots;
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
namespace D2RAccess::InteractionReader {
bool HandlesFocus(NativeUi::Focus current) { return current.widget == 81 || current.widget == 82; }
bool Tick(const D2RL::PluginContext*, uint64_t, NativeUi::Focus, bool conversation, std::string_view) { ++interactionTicks; return inventoryOwns && !conversation; }
void Reset() {}
std::string Repeat() { return {}; }
}
namespace D2RAccess::TabReader {
std::string Update(std::span<const uintptr_t>, NativeUi::Focus, bool) { return tabChange; }
void Reset() {}
}
namespace D2RAccess::NativeControllerHelp {
std::string ForFocus(NativeUi::Focus current) {
	++questHelpReads;
	return current.panel == 90 && current.widget == 91 ? questHelp : std::string{};
}
}
namespace D2RAccess::ControlHints {
void SpeakAfter(const D2RL::PluginContext*, std::string_view text) {
	if (automaticHints && !text.empty()) speech.push_back({std::string(text), false});
}
}
namespace D2RAccess::PanelState { const std::vector<std::string>& OpenPanels() { return openPanels; } }
namespace D2RAccess::NpcSpeech { bool AudioOnly() { return npcAudioOnly; } }
namespace D2RAccess::ScreenReader {
void Stop() { ++stops; }
void Speak(std::string_view text, bool interrupt) { speech.push_back({std::string(text), interrupt}); }
}

int main() {
	using namespace D2RAccess;
	using D2RL::Lifecycle::GameplayEventKind;
	Strings::Select(Strings::Language::esES);
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
	speech.clear(); panels.back().nodes.back().text = "Otra cosa que decir";
	npcAudioOnly = true; Settle();
	Check(std::none_of(speech.begin(), speech.end(), [](const auto& said) { return said.text == "Otra cosa que decir"; }),
		"NPC Speech set to audio only leaves the dialogue text unread");
	npcAudioOnly = false; panels.back().nodes.back().text = std::string(5000, 'a'); Settle();
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
	panels.clear(); focus = {}; Settle(); speech.clear();
	const std::string warning = "Solo disponible en partidas de clásico y de expansión";
	panels.push_back({40, 0, "HUDMessagePanel", UiText::Surface::NotificationFeed, {
		{40, -1, UiText::Kind::Other, "Root", "HUDMessagePanel", ""},
		{41, 0, UiText::Kind::NotificationList, "Messages", "ChatListWidget", ""},
		{42, 1, UiText::Kind::ListItem, "ChatEntryTemplate", "ListItemWidget", ""},
		{43, 2, UiText::Kind::LinkedText, "MessageText", "TextBoxWithLinksWidget", warning},
	}});
	Check(!Settle() && speech.size() == 1 && speech.back().text == warning && !speech.back().interrupt, "G-style notice reads without controller focus and does not interrupt");
	panels[0].nodes[1].text = warning;
	panels[0].nodes[1].contentId = 100;
	speech.clear(); Settle();
	Check(speech.empty(), "persistent G warning is not repeated");
	panels[0].nodes[1].contentId = 101;
	Check(!Settle() && speech.size() == 1 && speech.back().text == warning, "second G delivery reads even with identical visible text");
	speech.clear(); Settle();
	Check(speech.empty(), "second G delivery does not repeat on later polls");
	GameTextReader::RequestRepeat(); Settle();
	Check(speech.size() == 1 && speech.back().text == warning, "repeat works for the current notification");
	panels.push_back(Menu()); focus = {1, 2}; Settle(); speech.clear();
	panels[0].nodes.back().text = "A new system message";
	Check(Settle() && speech.size() == 1 && speech.back().text == "A new system message" && !speech.back().interrupt, "notifications coexist with an NPC conversation without stealing focus");
	panels.pop_back(); focus = {};
	panels[0].nodes.clear(); Settle(); speech.clear();
	panels.push_back({50, 0, "NotificationPanel", UiText::Surface::Notice, {{51, -1, UiText::Kind::Text, "Prompt", "TextBoxWidget", "An informative notice"}}});
	Check(!Settle() && speech.size() == 1 && speech.back().text == "An informative notice", "unfocused temporary information panel is readable");
	speech.clear(); GameTextReader::SetEnabled(false); Settle(); speech.clear();
	panels.back().nodes[0].text = "Disabled notice";
	Check(!Settle() && speech.empty(), "disabling game text also disables notifications");
	GameTextReader::SetEnabled(true); Settle();
	panels = {{80, 81, "PlayerInventoryExpansionLayout", UiText::Surface::Other,
		{{81, -1, UiText::Kind::InventoryGrid, "grid", "InventoryGridWidget", ""}}}};
	focus = {80, 81}; inventoryOwns = true;
	Check(Settle() && GameTextReader::Tick(&context), "inventory retains focus ownership between text polls");
	Check(inventorySnapshots == 0, "conversation reader does not walk and discard the inventory tree or grid");
	focus.widget = 82;
	Check(Settle() && inventorySnapshots == 0, "focused inventory button avoids the discarded full panel/fallback walks");
	focus.widget = 81;
	GameTextReader::RequestDump(); Settle();
	Check(inventorySnapshots == 1, "explicit diagnostics can still inspect the inventory tree");
	const int beforeDisable = stops;
	GameTextReader::SetEnabled(false); Settle();
	const int disabledTicks = interactionTicks;
	Settle();
	Check(stops > beforeDisable && interactionTicks == disabledTicks, "disabling text stops inventory speech and polling");
	inventoryOwns = false; GameTextReader::SetEnabled(true); Settle();
	panels = {{90, 91, "QuestLogPanelExpansion", UiText::Surface::Menu, {
		{90, -1, UiText::Kind::Other, "Root", "QuestLogPanel", ""},
		{91, 0, UiText::Kind::QuestButton, "Quest0", "QuestLogButtonWidget", "Den of Evil"},
		{92, 0, UiText::Kind::Text, "ShortDescription", "TextBoxWidget", "Den of Evil"},
		{93, 0, UiText::Kind::Text, "LongDescription", "TextBoxWidget", "Monsters left: 3"},
	}}};
	focus = {90, 91}; speech.clear(); questHelp = "Y: Play Quest Dialogue";
	Check(Settle() && speech.size() == 1 && speech.back().text == "Den of Evil. Monsters left: 3",
		"quest selection speaks its details; the controller action is a hint, not part of them");
	GameTextReader::RequestRepeat(); speech.clear(); Settle();
	Check(speech.size() == 1 && speech.back().text == "Den of Evil. Monsters left: 3",
		"repeat reads the same details");
	panels[0].nodes[3].text = "Monsters left: 2"; speech.clear();
	Check(Settle() && speech.size() == 1 && speech.back().text == "Monsters left: 2" && !speech.back().interrupt,
		"journal counter updates do not repeat the title or unchanged action hint");
	questHelp.clear(); speech.clear();
	Check(Settle() && speech.empty(), "a footer disappearing does not interrupt the quest objective");
	questHelp = "Y: Play Quest Dialogue"; speech.clear(); automaticHints = true;
	Check(Settle() && speech.size() == 1 && speech.back().text == "Y: Play Quest Dialogue" && !speech.back().interrupt,
		"with autodetail and control tooltips on a newly available action hint is queued once");
	automaticHints = false;
	Check(questHelpReads > 0, "controller help is consulted for a populated quest selection");
	// A tab switch: the name leads. The window behind it says nothing at all on
	// that tick -- it used to start speaking and be cut off mid-word by the name
	// a moment later -- and follows on the next tick, queued behind it.
	panels[0].nodes[3].text = "Monsters left: 1"; speech.clear(); tabChange = "Quest Log";
	Settle();
	Check(speech.empty(), "the window says nothing on the tick its tab changed");
	tabChange.clear();
	Check(Settle() && speech.size() == 1 && speech.back().text == "Monsters left: 1" && !speech.back().interrupt,
		"and follows on the next tick without interrupting the name it queues behind");
	const int populatedQuestHelpReads = questHelpReads;
	panels[0].nodes[2].text.clear(); speech.clear(); Settle();
	Check(questHelpReads == populatedQuestHelpReads, "an unpopulated quest detail pane does not acquire controller actions");
	speech.clear(); Event(GameplayEventKind::GameLeft);
	Check(!Settle() && speech.empty(), "leaving the game clears speech context");
	GameTextReader::Unregister();
	Check(removed == 3, "all lifecycle listeners are removed");
	Passed();
}
