#include "beacon_service.h"
#include "beacon_audio.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "native_world.h"
#include "pad_filter.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "test_support.h"

#include <windows.h>
#include <cstring>
#include <vector>

using namespace D2RAccess;
namespace {
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
D2RL::ConsoleCommandCallback console = nullptr;
bool playing = true;
int silences = 0, plays = 0;
std::vector<std::string> speech;

auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener,
	D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	listeners.push_back(*listener); *handle = listeners.size(); return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleService lifecycle{.serviceSize = D2RL::LifecycleServiceSize,
	.serviceVersion = D2RL::LifecycleService::AbiVersion, .registerGameplayEventListener = RegisterListener,
	.unregisterGameplayEventListener = UnregisterListener};
auto Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t, const void** service) noexcept -> D2RL::ServiceQueryResult {
	if (id != D2RL::ServiceId::Lifecycle) return D2RL::ServiceQueryResult::Unavailable;
	*service = &lifecycle; return D2RL::ServiceQueryResult::Success;
}
bool RegisterCommand(const D2RL::PluginContext*, const D2RL::ConsoleCommandRegistration* registration) noexcept {
	console = registration->callback; return true;
}
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .registerConsoleCommand = RegisterCommand, .queryService = Query};
const D2RL::PluginContext context{.contextSize = sizeof(D2RL::PluginContext), .api = &api};

void Event(D2RL::Lifecycle::GameplayEventKind kind) {
	const D2RL::Lifecycle::GameplayEvent event{.structSize = D2RL::Lifecycle::GameplayEventSize, .kind = kind,
		.playerId = 42, .sessionGeneration = 1};
	for (const auto& listener : listeners) if (listener.kind == kind) listener.callback(&context, &event, nullptr);
}
void Command(const char* text) {
	D2RL::ConsoleCommandContext command{}; command.plugin = &context; command.args = text;
	command.argsLength = static_cast<uint32_t>(std::strlen(text));
	Check(console(nullptr, &command, nullptr) == D2RL::ConsoleCommandResult::Handled, "beacon command accepted");
}
}

namespace D2RAccess::RadarUi { bool InPlay() { return playing; } }
namespace D2RAccess::BeaconAudio {
bool Start(const D2RL::PluginContext*, double) { return true; }
void Stop() {}
bool Available() { return true; }
void Play(Sound, Placement) { ++plays; }
void Silence(const char*) { ++silences; }
}
namespace D2RAccess::InputDirection { bool TakeRightStickClick() { return false; } }
namespace D2RAccess::PadFilter { void ClaimRightThumb(const D2RL::PluginContext*, bool) {} }
namespace D2RAccess::RadarConfig { Result Read(const D2RL::PluginContext*) { return {}; } }
namespace D2RAccess::NativeWorld {
bool Read(uint32_t, uint64_t, int, Radar::World&, Diagnostics*) { return false; }
uintptr_t PlayerRoom() noexcept { return 0; }
}
namespace D2RAccess::NativeRooms {
Bounds Subtiles(const Room&) { return {}; }
bool Read(uintptr_t, std::vector<Room>&) { return false; }
size_t Containing(const std::vector<Room>&, Radar::Position) { return None; }
}
namespace D2RAccess::NativeGroundItems { bool Read(uint32_t, uint64_t, GroundItems::Snapshot&) { return false; } }
namespace D2RAccess::NativeEntities {
bool Read(uint32_t, uint64_t, std::vector<EntityRadar::Entity>&, bool, bool) { return false; }
}
namespace D2RAccess::EntityRadar { bool SameTarget(const Entity& a, const Entity& b) { return a.id == b.id; } }
namespace D2RAccess::NativeLevelExits { uint64_t Identity(Radar::Position) { return 1; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool) { speech.emplace_back(text); } }

int main() {
	using Kind = D2RL::Lifecycle::GameplayEventKind;
	Check(BeaconService::Register(&context), "beacon service registers with lifecycle");
	BeaconService::Tick(&context);
	Check(silences == 0, "playing baseline does not cancel audio");

	playing = false;
	BeaconService::Tick(&context);
	Check(silences == 1, "an unlocked beacon cancels pending confirmation audio on the blocked edge");
	BeaconService::Tick(&context);
	Check(silences == 1, "blocked ticks do not repeat cancellation or logging");
	playing = true;
	BeaconService::Tick(&context);
	Check(silences == 1, "returning to play rearms without cancelling");

	Event(Kind::LocalPlayerReady);
	BeaconService::Tick(&context); // consume lifecycle revision before offering a target
	const int lifecycleSilences = silences;
	BeaconService::OfferPoint({10, 10}, "exit", 0, GetTickCount64());
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == 1, "locking plays its confirmation and arms guidance");
	playing = false;
	BeaconService::Tick(&context);
	Check(silences == lifecycleSilences + 1, "pause cancels a locked beacon before its guidance cadence");
	playing = true;
	BeaconService::Tick(&context);
	Command("status");
	BeaconService::Tick(&context);
	Check(!speech.empty() && speech.back().find("exit") != std::string::npos,
		"resume retains the lock and status still names its target");

	BeaconService::Unregister();
	Passed();
}
