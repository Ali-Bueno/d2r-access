#include "beacon_service.h"
#include "beacon.h"
#include "beacon_audio.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_automap.h"
#include "native_level_exits.h"
#include "native_rooms.h"
#include "native_world.h"
#include "navigation_policy.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "terrain_service.h"
#include "zone_transitions.h"
#include "test_support.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
std::vector<D2RL::Lifecycle::GameplayEventListener> listeners;
D2RL::ConsoleCommandCallback console = nullptr;
bool playing = true;
bool worldReadable = false;
Radar::Position playerAt{100, 100};
// Fixture rooms are measured in subtiles: two side by side over one open collision grid.
const std::vector<NativeRooms::Room> levelRooms{{0, 0, 50, 50, 1}, {50, 0, 50, 50, 2}};
std::vector<NativeRooms::Room> visitedRooms;
std::optional<std::vector<NativeRooms::Room>> chartedRooms; // what the automap has drawn; none when unreadable
struct Box { int minX = 0, minY = 0, maxX = -1, maxY = -1; } blocked; // walled cells, inclusive
int roomReads = 0;
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
std::string Format(const char* format, const char* name) {
	char text[256]{};
	std::snprintf(text, sizeof text, format, name);
	return text;
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
namespace D2RAccess::InputDirection { bool TakeLeftStickClick() { return false; } }
namespace D2RAccess::PadFilter { void Claim(const D2RL::PluginContext*, bool) {} }
namespace D2RAccess::RadarConfig {
Result Read(const D2RL::PluginContext*) { return {}; }
Navigation ReadNavigation(const D2RL::PluginContext*) { return {.fullAssistance = true}; }
}
namespace D2RAccess::NativeAutomap {
std::optional<std::vector<NativeRooms::Room>> RevealedRooms(const std::vector<NativeRooms::Room>&) { return chartedRooms; }
}
namespace D2RAccess::TerrainService {
std::vector<NativeRooms::Room> VisitedRooms(int, const std::vector<NativeRooms::Room>&) { return visitedRooms; }
std::vector<NativeRooms::Room> ExploredRooms(int level, const std::vector<NativeRooms::Room>& rooms, Radar::Position player) {
	const auto charted = NativeAutomap::RevealedRooms(rooms);
	return NavigationPolicy::Explored(VisitedRooms(level, rooms), charted ? &*charted : nullptr, rooms, player);
}
}
namespace D2RAccess::NativeWorld {
bool Read(uint32_t, uint64_t, int, Radar::World& world, Diagnostics*) {
	if (!worldReadable) return false;
	world.player = playerAt;
	world.walkingMask = Radar::Collision::Wall;
	world.rooms = {{0, 0, 100, 50, std::vector<uint16_t>(100 * 50, 0)}};
	for (int y = blocked.minY; y <= blocked.maxY; ++y)
		for (int x = blocked.minX; x <= blocked.maxX; ++x) world.rooms[0].cells[static_cast<size_t>(y) * 100 + x] = Radar::Collision::Wall;
	return true;
}
uintptr_t PlayerRoom() noexcept { return 0; }
}
namespace D2RAccess::NativeRooms {
Bounds Subtiles(const Room& room) { return {room.tileX, room.tileY, room.tileX + room.tileWidth, room.tileY + room.tileHeight}; }
bool Contains(const Bounds& bounds, int x, int y) { return x >= bounds.minX && x < bounds.maxX && y >= bounds.minY && y < bounds.maxY; }
bool Read(uintptr_t, std::vector<Room>& rooms) {
	if (++roomReads == 1) return false;
	rooms = levelRooms;
	return true;
}
size_t Containing(const std::vector<Room>& rooms, Radar::Position subtile) {
	for (size_t i = 0; i < rooms.size(); ++i)
		if (Contains(Subtiles(rooms[i]), static_cast<int>(subtile.x), static_cast<int>(subtile.y))) return i;
	return None;
}
}
namespace D2RAccess::NativeGroundItems {
bool Read(uint32_t, uint64_t, GroundItems::Snapshot&) { return false; }
bool ReadLevel(uint32_t, uint64_t, GroundItems::Snapshot&) { return false; }
}
namespace D2RAccess::NativeEntities {
bool Read(uint32_t, uint64_t, std::vector<EntityRadar::Entity>&, bool, bool, bool) { return false; }
bool ReadLayout(uint32_t, bool, NativeLevelExits::Layout&) { return false; }
}
namespace D2RAccess::ZoneTransitions {
std::vector<Radar::LevelExit> Find(const Radar::World&, const NativeLevelExits::Layout&, double) { return {}; }
bool Crosses(const EntityRadar::Entity&) { return false; }
bool MoveToCrossing(EntityRadar::Entity&, const std::vector<Radar::LevelExit>&) { return false; }
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
	EntityRadar::Entity exit{.id = 7, .kind = EntityRadar::Kind::Exit, .position = {10, 10}, .name = "exit"};
	BeaconService::Offer(exit, 0, GetTickCount64());
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
	Command("clear");
	BeaconService::Tick(&context);
	worldReadable = true;
	EntityRadar::Entity farExit{.id = 8, .kind = EntityRadar::Kind::Exit, .position = {120, 100}, .name = "far exit"};
	BeaconService::Offer(farExit, 0, GetTickCount64());
	int before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1, "full assistance locks while the first room-graph read fails");
	Check(roomReads == 2, "a failed room-graph read is retried by the next use in the same level instance");
	Sleep(160); // the service's 150 ms target refresh cadence
	BeaconService::Tick(&context);
	Check(roomReads == 2, "a successfully built room graph is cached");

	const auto& strings = Strings::Current();
	Command("exploration");
	BeaconService::Tick(&context);
	Check(speech.back() == strings.navigationExploration, "the exploration command releases the lock and names the mode");
	playerAt = {10, 10};
	visitedRooms = {levelRooms[0]};
	EntityRadar::Entity gate{.id = 9, .kind = EntityRadar::Kind::Exit, .position = {70, 10}, .name = "gate"};
	BeaconService::Offer(gate, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before && speech.back() == Format(strings.beaconKnownRouteOnly, "gate"),
		"exploration refuses a target in a room never entered");
	visitedRooms = {};
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before && speech.back() == Format(strings.beaconKnownRouteOnly, "gate"),
		"without history only the room underfoot counts as visited");
	visitedRooms = levelRooms;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1, "exploration locks a target reached through visited rooms");

	worldReadable = false;
	const int silenced = silences;
	Sleep(160);
	BeaconService::Tick(&context);
	Command("status");
	BeaconService::Tick(&context);
	Check(silences == silenced && speech.back().find(strings.navigationExploration) == 0
		&& speech.back().find("gate") != std::string::npos, "an unreadable world keeps the exploration lock");
	worldReadable = true;
	visitedRooms = {levelRooms[0]};
	Sleep(160);
	BeaconService::Tick(&context);
	Check(silences == silenced, "a target briefly outside the visited rooms keeps the lock in silence");
	Sleep(static_cast<DWORD>(Beacon::Settings{}.routeLossMs) + 160);
	BeaconService::Tick(&context);
	Check(silences == silenced + 1 && speech.back() == Format(strings.beaconKnownRouteEnded, "gate"),
		"a target outside the visited rooms for the whole grace ends the known route");
	chartedRooms = std::vector<NativeRooms::Room>{levelRooms[1]};
	BeaconService::Offer(gate, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1, "a room the automap has drawn counts as explored");
	EntityRadar::Entity lever{901, EntityRadar::Kind::Object, {70, 10}, "lever"};
	BeaconService::Offer(lever, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1 && speech.back().find("lever") != std::string::npos,
		"one press on a newly scanned target moves the beacon there");
	Command("lock");
	BeaconService::Tick(&context);
	Check(speech.back() == strings.beaconCleared, "a second press on the same target releases it");
	Command("clear");
	BeaconService::Tick(&context);
	chartedRooms.reset();

	blocked = {28, 8, 32, 12}; // a 5x5 object's own collision around its centre
	EntityRadar::Entity statue{900, EntityRadar::Kind::Object, {30, 10}, "statue"};
	statue.footprint = {5, 5};
	BeaconService::Offer(statue, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1, "a large object is reached from beside its footprint");
	Command("clear");
	BeaconService::Tick(&context);

	blocked = {70, 20, 79, 29}; // the unexplored room's centre is walled in
	visitedRooms = {levelRooms[0]};
	EntityRadar::Entity unexplored{2, EntityRadar::Kind::Room, {74.5, 24.5}, "unexplored room 2"};
	unexplored.category = EntityRadar::Category::UnexploredRoom;
	unexplored.placeId = levelRooms[1].id;
	BeaconService::Offer(unexplored, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1, "exploration may lead into an unexplored room beside known ground");
	playerAt = {55, 10};
	Sleep(160);
	BeaconService::Tick(&context);
	Check(speech.back() == Format(strings.beaconArrived, "unexplored room 2"),
		"stepping into the unexplored room arrives, wherever its centre lies");
	playerAt = {10, 10};
	blocked = {};

	EntityRadar::Entity ally{60, EntityRadar::Kind::Player, {70, 10}, "Amiga"};
	ally.category = EntityRadar::Category::Player;
	BeaconService::Offer(ally, 0, GetTickCount64());
	before = plays;
	Command("lock");
	BeaconService::Tick(&context);
	Check(plays == before + 1 && speech.back().find("Amiga") != std::string::npos,
		"exploration follows another player into a room never entered");
	Sleep(static_cast<DWORD>(Beacon::Settings{}.routeLossMs) + 160);
	BeaconService::Tick(&context);
	Command("status");
	BeaconService::Tick(&context);
	Check(speech.back().find("Amiga") != std::string::npos, "the known-route limit never ends following a player");
	Command("clear");
	BeaconService::Tick(&context);
	Command("full");
	BeaconService::Tick(&context);
	Check(speech.back() == strings.navigationFullAssistance, "the full command names full assistance");

	BeaconService::Unregister();
	Passed();
}
