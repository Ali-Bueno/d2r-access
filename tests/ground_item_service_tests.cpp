#include "ground_item_service.h"
#include "ground_items.h"
#include "item_audio.h"
#include "native_ground_items.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "test_support.h"
#include <windows.h>
#include <vector>

using namespace D2RAccess;
namespace {
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
D2RL::ConsoleCommandCallback console = nullptr;
std::vector<std::string> speech;
bool interrupted = false, supported = true, readable = true;
int removed = 0, reads = 0;
RadarUi::State ui = RadarUi::State::Playing;
GroundItems::Snapshot snapshot{1, {}, {{.id = 9, .position = {1, 1}, .name = "Poción", .baseName = "Poción"}}};
std::vector<GroundItems::Voice> voices;
auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener, D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	listeners.push_back(*listener); *handle = listeners.size(); return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	++removed; return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleServiceV1 lifecycle{.serviceSize = D2RL::LifecycleServiceV1Size, .serviceVersion = D2RL::LifecycleServiceV1Version,
	.registerGameplayEventListener = RegisterListener, .unregisterGameplayEventListener = UnregisterListener};
auto Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t, const void** service) noexcept -> D2RL::ServiceQueryResult {
	if (id != D2RL::ServiceId::Lifecycle) return D2RL::ServiceQueryResult::Unavailable;
	*service = &lifecycle; return D2RL::ServiceQueryResult::Success;
}
bool RegisterCommand(const D2RL::PluginContext*, const D2RL::ConsoleCommandRegistration* command) noexcept { console = command->callback; return true; }
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .registerConsoleCommand = RegisterCommand, .queryService = Query};
const D2RL::PluginContext context{.contextSize = sizeof(D2RL::PluginContext), .api = &api};
void Event(D2RL::Lifecycle::GameplayEventKind kind) {
	const D2RL::Lifecycle::GameplayEvent event{.structSize = D2RL::Lifecycle::GameplayEventSize, .kind = kind, .playerId = 42, .sessionGeneration = snapshot.session};
	for (const auto& listener : listeners) if (listener.kind == kind) listener.callback(&context, &event, nullptr);
}
void Tick(bool owns = false) {
	// Sleep's timer resolution can wake before the service's GetTickCount64 deadline.
	const uint64_t deadline = GetTickCount64() + GroundItems::PollMs;
	while (GetTickCount64() < deadline) Sleep(1);
	GroundItemService::Tick(&context, owns);
}
void Command(const char* text) {
	D2RL::ConsoleCommandContext command{}; command.plugin = &context; command.args = text; command.argsLength = static_cast<uint32_t>(strlen(text));
	Check(console(nullptr, &command, nullptr) == D2RL::ConsoleCommandResult::Handled, "item command accepted"); Tick();
}
}
namespace D2RAccess::NativeGroundItems {
bool Initialize(const D2RL::PluginContext*) { return supported; }
bool Read(uint32_t player, uint64_t session, GroundItems::Snapshot& result) {
	++reads; Check(player == 42 && session == snapshot.session, "reader uses active lifecycle identity"); result = snapshot; return readable;
}
}
namespace D2RAccess::ItemAudio {
bool Start(const D2RL::PluginContext*) { return true; }
void Update(std::vector<GroundItems::Voice> value) { voices = std::move(value); }
void Silence() { voices.clear(); }
void Stop() { voices.clear(); }
bool Available() { return true; }
}
namespace D2RAccess::RadarUi { State Inspect(bool) { return ui; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupted = interrupt; } }
int main() {
	using K = D2RL::Lifecycle::GameplayEventKind;
	Check(GroundItemService::Register(&context) && listeners.size() == 6, "loot service subscribes to lifecycle independently of radar");
	Tick(); Check(reads == 0 && voices.empty(), "front end never reads world pointers");
	Event(K::GameJoined); Tick(); Check(reads == 0, "loading waits for local player ready");
	Event(K::LocalPlayerReady); Tick();
	Check(voices.size() == 1 && speech.size() == 1 && !interrupted, "nearby item creates a spatial source and noninterrupting speech");
	Check(GroundItemService::OwnsWorldLabel("Poción"), "nearby world tooltip defers to the proximity announcement");
	Check(!GroundItemService::OwnsWorldLabel("Charsi"), "NPC labels are not suppressed");
	speech.clear(); Tick(); Check(speech.empty(), "continuous updates do not repeat the item");
	for (auto state : {RadarUi::State::Dialogue, RadarUi::State::Paused, RadarUi::State::Inactive, RadarUi::State::Unavailable}) {
		ui = state; Tick(); Check(voices.empty() && !GroundItemService::OwnsWorldLabel("Poción"), "dialogue, pause, lost focus and missing UI silence loot cues");
	}
	ui = RadarUi::State::Playing; Tick(); Check(voices.size() == 1 && speech.empty(), "resume restores audio without repeating stationary loot");
	snapshot.items[0].id = 10; Tick(true);
	Check(voices.size() == 1 && speech.empty(), "nonpausing inventory retains cues but does not compete with menu speech");
	Sleep(static_cast<DWORD>(GroundItems::SpeechGapMs)); Tick(); Check(speech.size() == 1, "closing nonpausing menu permits newly nearby speech");
	Command("sound-off"); Check(voices.empty(), "sound toggle immediately cancels sources");
	snapshot.items[0].id = 11; Sleep(static_cast<DWORD>(GroundItems::SpeechGapMs)); speech.clear(); Tick();
	Check(voices.empty() && speech.size() == 1, "speech works with loops disabled");
	Command("sound-on"); Check(voices.size() == 1, "loops can be reenabled");
	Command("off"); snapshot.items[0].id = 12; speech.clear(); Tick();
	Check(voices.size() == 1 && speech.empty(), "loops work with proximity speech disabled");
	readable = false; Tick(); Check(voices.empty(), "failed native snapshot clears audio"); readable = true;
	Event(K::GameLeft); const int before = reads; Tick(); Check(voices.empty() && reads == before, "leaving a game stops all native reads and loops");
	Command("on"); snapshot.session++; Event(K::GameJoined); Event(K::LocalPlayerReady); speech.clear(); Tick();
	Check(voices.size() == 1 && speech.size() == 1, "new session does not inherit speech suppression or stale voices");
	snapshot.items.clear(); Tick(); Check(voices.empty(), "pickup removes its live source on the next snapshot");
	GroundItemService::Unregister(); Check(removed == 6 && voices.empty(), "unload detaches all listeners and audio");
	Passed();
}
