#include "entity_cue_service.h"
#include "entity_cue_audio.h"
#include "entity_radar.h"
#include "native_entities.h"
#include "native_world.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "test_support.h"

#include <windows.h>
#include <cstring>
#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
D2RL::ConsoleCommandCallback console = nullptr;
std::vector<std::string> speech;
std::vector<EntityRadar::Entity> entities{{.id = 7, .kind = EntityRadar::Kind::Npc, .position = {1, -1}, .name = "Kashya"}};
bool playing = true, worldReadable = true, entitiesReadable = true, nativeReady = true;
bool audioStartSucceeds = true, audioReady = true;
int worldReads = 0, entityReads = 0, starts = 0, stops = 0, silences = 0;
int expectedLevel = 0;
uint64_t session = 1;
struct Played { EntityCueAudio::Sound sound; EntityCueAudio::Placement place; };
std::vector<Played> plays;

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

void Event(D2RL::Lifecycle::GameplayEventKind kind, int level = 0) {
	const D2RL::Lifecycle::GameplayEvent event{.structSize = D2RL::Lifecycle::GameplayEventSize, .kind = kind,
		.playerId = 42, .sessionGeneration = session, .currentValue = level};
	for (const auto& listener : listeners) if (listener.kind == kind) listener.callback(&context, &event, nullptr);
}
void Command(const char* word) {
	D2RL::ConsoleCommandContext command{}; command.plugin = &context; command.args = word;
	command.argsLength = static_cast<uint32_t>(std::strlen(word));
	Check(console(nullptr, &command, nullptr) == D2RL::ConsoleCommandResult::Handled, "entity cue command accepted");
	EntityCueService::Tick(&context);
}
void Poll() {
	const uint64_t deadline = GetTickCount64() + 100;
	while (GetTickCount64() < deadline) Sleep(1);
	EntityCueService::Tick(&context);
}
}

namespace D2RAccess::NativeWorld {
bool Initialize(const D2RL::PluginContext*) { return nativeReady; }
bool Read(uint32_t player, uint64_t generation, int level, Radar::World& world, Diagnostics*) {
	++worldReads;
	Check(player == 42 && generation == session && level == expectedLevel, "world read uses current lifecycle identity and level");
	world.player = {0, 0}; world.level = level; world.session = generation;
	return worldReadable;
}
}
namespace D2RAccess::NativeEntities {
bool Initialize(const D2RL::PluginContext*) { return nativeReady; }
bool Read(uint32_t player, uint64_t generation, std::vector<EntityRadar::Entity>& output, bool distantExits, bool levelBearings, bool) {
	++entityReads;
	Check(player == 42 && generation == session && !distantExits && !levelBearings,
		"entity cues read live nearby units for the current player");
	output = entities; return entitiesReadable;
}
}
namespace D2RAccess::EntityRadar {
bool LineOfSight(const Radar::World&, Radar::Position, Radar::Position) { return true; }
}
namespace D2RAccess::RadarConfig { EntityCueSettings ReadEntityCues(const D2RL::PluginContext*) { return {}; } }
namespace D2RAccess::RadarUi { bool InPlay() { return playing; } }
namespace D2RAccess::EntityCueAudio {
bool Start(const D2RL::PluginContext*, double) { ++starts; return audioStartSucceeds; }
void Play(Sound sound, Placement place) { plays.push_back({sound, place}); }
void Silence() { ++silences; }
void Stop() { ++stops; }
bool Available() { return audioReady; }
}
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool) { speech.emplace_back(text); } }

int main() {
	using Kind = D2RL::Lifecycle::GameplayEventKind;
	Check(EntityCueService::Register(&context) && listeners.size() == 6 && starts == 1,
		"service registers lifecycle and starts its audio worker once");
	EntityCueService::Tick(&context);
	Check(worldReads == 0 && plays.empty(), "front end never reads native game state");
	Event(Kind::LocalPlayerReady);
	EntityCueService::Tick(&context);
	Check(plays.size() == 1 && plays.back().sound == EntityCueAudio::Sound::Npc,
		"nearby NPC produces a cue after local player ready");
	Check(plays.back().place.right > 0 && plays.back().place.front == 0,
		"NPC cue uses the fixed screen direction");
	const int sampled = worldReads;
	EntityCueService::Tick(&context);
	EntityCueService::Tick(&context);
	Check(worldReads == sampled, "repeated UI ticks do not rescan the world before the cadence");

	playing = false;
	const int beforePause = silences;
	EntityCueService::Tick(&context);
	Check(silences == beforePause + 1 && worldReads == sampled, "pause silences the active cue before another native read");
	EntityCueService::Tick(&context);
	Check(silences == beforePause + 1, "paused UI ticks do not flood the audio worker");
	playing = true;
	Poll();
	Check(plays.size() == 2, "resuming gameplay rearms the NPC cue");

	worldReadable = false;
	const int beforeFailure = silences;
	Poll();
	Check(silences == beforeFailure + 1 && entityReads < worldReads,
		"failed world read silences immediately and skips unit read");
	worldReadable = true;
	entitiesReadable = false;
	Poll();
	Check(plays.size() == 2, "failed unit read cannot play a stale NPC cue");
	entitiesReadable = true;
	Poll();
	Check(plays.size() == 3, "recovered native reads rearm the cue");

	const int beforeOff = silences;
	Command("off");
	Check(silences == beforeOff + 1, "off cancels active cue immediately");
	const int beforeDisabled = worldReads;
	Poll();
	Check(worldReads == beforeDisabled && plays.size() == 3, "disabled feature does not read or play");
	Command("on");
	Poll();
	Check(plays.size() == 4, "on resumes an available NPC cue");
	Command("npc-off");
	Poll();
	Check(plays.size() == 4, "NPC category switch suppresses only NPC cues");
	Check(!speech.empty() && speech.back().find("NPC cues off") != std::string::npos,
		"NPC category state remains readable through the shared speech sink");
	Command("npc-on");
	Poll();
	Check(plays.size() == 5, "NPC cues can be restored without changing the independent announcer");

	expectedLevel = 12;
	const int beforeLevel = silences;
	Event(Kind::LevelChanged, expectedLevel);
	Check(silences == beforeLevel + 1, "level transition cancels any queued cue at the event boundary");
	EntityCueService::Tick(&context);
	Check(plays.size() == 6, "new level resets cadence and reads the new level identity");
	Event(Kind::GameLeft);
	const int beforeLeft = worldReads;
	Poll();
	Check(worldReads == beforeLeft, "leaving the game blocks world reads");
	++session; expectedLevel = 0;
	Event(Kind::GameJoined); Event(Kind::LocalPlayerReady);
	EntityCueService::Tick(&context);
	Check(plays.size() == 7, "new game session starts a fresh cue cadence");

	audioReady = false;
	const int beforeNoAudio = worldReads;
	Poll(); Command("status");
	Check(worldReads == beforeNoAudio && speech.back().find("audio unavailable") != std::string::npos,
		"unavailable audio blocks reads and is reported in status");
	EntityCueService::Unregister();
	Check(stops == 1, "unregister stops the audio worker");

	listeners.clear(); audioReady = true; audioStartSucceeds = false;
	Check(EntityCueService::Register(&context), "service remains controllable when audio startup fails");
	Event(Kind::LocalPlayerReady);
	const int beforeFailedStart = worldReads;
	Command("status");
	Check(worldReads == beforeFailedStart && speech.back().find("audio unavailable") != std::string::npos,
		"failed audio startup is reported and avoids native scans");
	EntityCueService::Unregister();

	listeners.clear(); audioStartSucceeds = true; nativeReady = false;
	Check(EntityCueService::Register(&context), "service exposes status when native reader cannot initialize");
	Command("status");
	Check(speech.back().find("reader unavailable") != std::string::npos,
		"native reader startup failure is reported to the player");
	EntityCueService::Unregister();
	Passed();
}
