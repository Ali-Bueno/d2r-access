#include "quest_service.h"
#include "quest_progress.h"
#include "quest_seals.h"
#include "localization.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <windows.h>
#include <map>

using namespace D2RAccess;
namespace {
using K = D2RL::Lifecycle::GameplayEventKind;
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
D2RL::ConsoleCommandCallback console = nullptr;
std::vector<std::string> speech, panels;
bool interrupted = false, ownsSpeech = false, available = true, visible = false;
bool serviceAvailable = true;
int removed = 0, failRegistration = -1;
uintptr_t panel = 0;
RadarUi::State ui = RadarUi::State::Playing;
std::string title = "Misiones", shortText = "El Cubil del Mal", longText = "Monstruos restantes: 3";
const std::map<std::string, std::string, std::less<>> strings{
	{"qstsa1q1", "El Cubil del Mal"}, {"qstsa1q14", "Monstruos restantes: %d"}};

auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener, D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	if (static_cast<int>(listeners.size()) == failRegistration) return D2RL::Lifecycle::Result::Unavailable;
	listeners.push_back(*listener); *handle = listeners.size(); return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	++removed; return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleService lifecycle{.serviceSize = D2RL::LifecycleServiceSize, .serviceVersion = D2RL::LifecycleService::AbiVersion,
	.registerGameplayEventListener = RegisterListener, .unregisterGameplayEventListener = UnregisterListener};
auto Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t, const void** result) noexcept -> D2RL::ServiceQueryResult {
	if (!serviceAvailable || id != D2RL::ServiceId::Lifecycle) return D2RL::ServiceQueryResult::Unavailable;
	*result = &lifecycle; return D2RL::ServiceQueryResult::Success;
}
bool RegisterCommand(const D2RL::PluginContext*, const D2RL::ConsoleCommandRegistration* command) noexcept {
	Check(std::string_view(command->name) == "access-quests", "quest toggle is registered");
	console = command->callback; return true;
}
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .registerConsoleCommand = RegisterCommand, .queryService = Query};
const D2RL::PluginContext context{.contextSize = sizeof(D2RL::PluginContext), .api = &api};

void Event(K kind, uint32_t player = 42, uint64_t session = 1) {
	const D2RL::Lifecycle::GameplayEvent event{.structSize = D2RL::Lifecycle::GameplayEventSize,
		.kind = kind, .playerId = player, .sessionGeneration = session, .questRecordId = 1};
	for (const auto& listener : listeners) if (listener.kind == kind) listener.callback(&context, &event, nullptr);
}
void Tick() { QuestService::Tick(&context, ownsSpeech); }
void Wait() {
	// Exercise real deferred service scheduling; exact timing is asserted only
	// by the pure reader tests. Poll through one documented alert window.
	const auto end = GetTickCount64() + QuestProgress::AlertDelayMs;
	while (GetTickCount64() < end) { Tick(); Sleep(1); }
	Tick();
}
auto Command(const char* arg) {
	D2RL::ConsoleCommandContext command{};
	command.plugin = &context; command.args = arg; command.argsLength = static_cast<uint32_t>(strlen(arg));
	const auto result = console(nullptr, &command, nullptr); Tick(); return result;
}
}
namespace D2RAccess::NativeWidgets {
bool IsAvailable() { return available; }
uintptr_t FindPanel(std::string_view name) { return name == "QuestLogPanelExpansion" ? panel : 0; }
std::string TextAt(std::string_view, std::span<const std::string> path) {
	if (path[0] == "Title") return title;
	if (path[0] == "ShortDescription") return shortText;
	return longText;
}
}
namespace D2RAccess::NativeUi { bool Visible(uintptr_t) { return visible; } }
namespace D2RAccess::PanelState { const std::vector<std::string>& OpenPanels() { return panels; } }
namespace D2RAccess::Localization {
std::string Text(std::string_view key) { const auto found = strings.find(key); return found == strings.end() ? std::string() : found->second; }
std::string Text(uint32_t id) { return id == 3714 ? "El Cubil del Mal" : std::string(); }
}
namespace D2RAccess::RadarUi { State Inspect(bool) { return ui; } }
namespace D2RAccess::CairnScrollService {
void Tick(const D2RL::PluginContext*, uint64_t, bool, bool) {}
void Reset() {}
}
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupted = interrupt; }
}
namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) noexcept {}
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
void Warn(const D2RL::PluginContext*, const char*) noexcept {}
}

int main() {
	Strings::Select(Strings::Language::esES);
	Check(QuestService::Register(&context) && listeners.size() == 4, "quest service registers game lifecycle and completion events");
	QuestService::Note("SetQuestAlert"); Wait(); Check(speech.empty(), "front-end quest traffic stays silent");
	Event(K::GameJoined); Event(K::LocalPlayerReady);
	QuestService::Note("SetQuestAlert"); QuestService::Note("QuestUpdate");
	ui = RadarUi::State::Dialogue; Wait(); Check(speech.empty(), "dialogue holds a real quest update");
	ui = RadarUi::State::Playing; Tick();
	Check(speech.size() == 1 && speech.back() == Strings::Current().questUpdated && !interrupted, "one localized fallback is spoken without interrupt when no panel exists");
	speech.clear(); QuestService::Note("SelectQuest"); QuestService::Note("SelectTab"); Wait();
	Check(speech.empty(), "ordinary journal navigation never creates an update alert");
	Event(K::QuestCompleted, 99); Event(K::QuestCompleted, 42, 99); Wait();
	Check(speech.empty(), "foreign players and sessions are rejected");
	Event(K::QuestCompleted); ownsSpeech = true; Wait();
	Check(speech.empty(), "the existing UI reader has speech priority");
	ownsSpeech = false; Tick();
	Check(speech.size() == 1 && speech.back() == Strings::Current().questGoalDone && !interrupted, "SDK primary goal completion has its own announcement");
	speech.clear(); Event(K::QuestCompleted); Wait(); Check(speech.empty(), "completion records are diff-gated");
	panel = 1; Wait(); Check(speech.empty(), "the first readable objective becomes a silent baseline");
	longText = "Monstruos restantes: 2"; Wait();
	Check(speech.size() == 1 && speech.back() == "El Cubil del Mal. Monstruos restantes: 2", "native text changes reach the real speech service wiring");
	speech.clear(); visible = true; panels = {"QuestLogPanelExpansion"}; QuestService::Note("SelectQuest");
	longText = "Monstruos restantes: 1"; Wait(); Check(speech.empty(), "the open journal is left to its UI reader");
	visible = false; panels.clear(); QuestService::Note("Close"); Wait(); Check(speech.empty(), "closing the journal does not replay its text");
	Check(Command("off") == D2RL::ConsoleCommandResult::Handled && speech.back() == Strings::Current().questAlertsDisabled, "announcements can be disabled");
	speech.clear(); QuestService::Note("QuestUpdate"); Wait(); Check(speech.empty(), "disabled announcements do not accumulate");
	Command("on"); Check(speech.back() == Strings::Current().questAlertsEnabled, "announcements can be reenabled"); speech.clear();
	Check(Command("invalid") == D2RL::ConsoleCommandResult::InvalidArguments, "unknown console actions are rejected");
	Wait(); speech.clear();
	QuestSeals::Observe(1, 2, false); QuestSeals::Observe(1, 2, true); ownsSpeech = true; Tick();
	Check(speech.empty(), "a seal's opening waits while the UI reader has the speech");
	ownsSpeech = false; Tick();
	Check(speech.size() == 1 && speech.back() == QuestSeals::LeftText(4, Strings::Current()) && !interrupted, "an opened seal says how many are left");
	speech.clear(); Tick(); Check(speech.empty(), "one opening is said once");
	QuestService::Note("QuestUpdate"); Event(K::GameLeft); Wait(); Check(speech.empty(), "leaving the game discards pending announcements");
	QuestService::Unregister(); Check(removed == 4, "all lifecycle subscriptions are released");
	listeners.clear(); failRegistration = 2;
	Check(!QuestService::Register(&context) && removed == 6, "partial registration cleans up successful subscriptions");
	listeners.clear(); failRegistration = -1; serviceAvailable = false;
	Check(!QuestService::Register(&context), "missing lifecycle service fails gracefully");
	Passed();
}
