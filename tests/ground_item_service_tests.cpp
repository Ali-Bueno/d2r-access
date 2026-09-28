#include "ground_item_service.h"
#include "ground_items.h"
#include "entity_layout.h"
#include "item_audio.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_world.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "test_support.h"
#include "zone_transitions.h"
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
std::vector<EntityRadar::Entity> entities{{7, EntityRadar::Kind::Npc, {2, 2}, "Kashya"},
	{20, EntityRadar::Kind::Exit, {3, 3}, "Portal", 0, false, EntityRadar::Category::Portal}};
bool includeBoundaries = false;
const std::vector<Radar::LevelExit> boundaries{{{4, 4}, 2}, {{80, 80}, 3}};
// A casket the predicate flagged, and a shrine it did not.
const EntityRadar::Entity casket{30, EntityRadar::Kind::Object, {2, 3}, "Ataúd", 0, true};
const EntityRadar::Entity shrine{31, EntityRadar::Kind::Object, {2, 4}, "Santuario", 0, false};
size_t Portals() { size_t n = 0; for (const auto& v : voices) n += v.sound == GroundItems::Sound::Portal; return n; }
size_t Zones() { size_t n = 0; for (const auto& v : voices) n += v.sound == GroundItems::Sound::ZoneTransition; return n; }
size_t Chests() { size_t n = 0; for (const auto& v : voices) n += v.sound == GroundItems::Sound::Chest; return n; }
size_t Count(GroundItems::Sound sound) { size_t n = 0; for (const auto& v : voices) n += v.sound == sound; return n; }
size_t Quests() { size_t n = 0; for (const auto& v : voices) n += v.sound == GroundItems::Sound::Quest; return n; }
auto RegisterListener(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEventListener* listener, D2RL::Lifecycle::ListenerHandle* handle) noexcept -> D2RL::Lifecycle::Result {
	listeners.push_back(*listener); *handle = listeners.size(); return D2RL::Lifecycle::Result::Success;
}
auto UnregisterListener(const D2RL::PluginContext*, D2RL::Lifecycle::ListenerHandle) noexcept -> D2RL::Lifecycle::Result {
	++removed; return D2RL::Lifecycle::Result::Success;
}
const D2RL::LifecycleService lifecycle{.serviceSize = D2RL::LifecycleServiceSize, .serviceVersion = D2RL::LifecycleService::AbiVersion,
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
namespace D2RAccess::NativeEntities {
bool Initialize(const D2RL::PluginContext*) { return supported; }
bool Read(uint32_t player, uint64_t session, std::vector<EntityRadar::Entity>& result, bool distantExits, bool levelBearings, bool wholeLevel) {
	Check(!distantExits, "proximity loops never request unloaded exit presets");
	Check(!wholeLevel, "proximity loops keep to the rooms around the player");
	Check(!levelBearings, "proximity loops do not use approximate area bearings");
	Check(player == 42 && session == snapshot.session, "exit reader uses active lifecycle identity");
	result = entities;
	return readable;
}
bool ReadLayout(uint32_t player, bool neighbours, NativeLevelExits::Layout& layout) {
	Check(player == 42 && neighbours, "boundary reader requests neighbouring level rooms");
	layout.level = 1;
	return readable;
}
bool CountContainerRows(size_t& rows, size_t& total) {
	rows = EntityLayout::ContainerRowCount;
	total = EntityLayout::ObjectRowCount;
	return readable;
}
}
namespace D2RAccess::NativeWorld {
bool Initialize(const D2RL::PluginContext*) { return true; }
bool Read(uint32_t player, uint64_t session, int level, Radar::World& result, Diagnostics*) {
	Check(player == 42 && session == snapshot.session && level == 0,
		"boundary reader uses active player and bootstraps the native level");
	result.level = 1;
	result.player = {snapshot.player.x, snapshot.player.y};
	return readable;
}
}
namespace D2RAccess::ZoneTransitions {
std::vector<Radar::LevelExit> Find(const Radar::World& world, const NativeLevelExits::Layout& layout, double) {
	Check(world.level == static_cast<int>(layout.level), "boundary finder receives matching world and layout");
	return includeBoundaries ? boundaries : std::vector<Radar::LevelExit>{};
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
std::vector<std::string> openPanels;
namespace D2RAccess::PanelState { const std::vector<std::string>& OpenPanels() { return openPanels; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupted = interrupt; } }
int main() {
	using K = D2RL::Lifecycle::GameplayEventKind;
	Check(GroundItemService::Register(&context) && listeners.size() == 6, "loot service subscribes to lifecycle independently of radar");
	Tick(); Check(reads == 0 && voices.empty(), "front end never reads world pointers");
	Event(K::GameJoined); Tick(); Check(reads == 0, "loading waits for local player ready");
	Event(K::LocalPlayerReady); Tick();
	Check(voices.size() == 2 && Portals() == 1 && speech.size() == 1 && !interrupted, "nearby item and exit create spatial sources and noninterrupting speech");
	Check(speech[0] == "Poción", "exits are never spoken by the proximity reader");
	Check(GroundItemService::OwnsWorldLabel("Poción"), "nearby world tooltip defers to the proximity announcement");
	Check(!GroundItemService::OwnsWorldLabel("Charsi"), "NPC labels are not suppressed");
	speech.clear(); Tick(); Check(speech.empty(), "continuous updates do not repeat the item");
	EntityRadar::Entity tree{70, EntityRadar::Kind::Object, {3, 4}, "Árbol de Inifuss"};
	tree.questObject = true;
	EntityRadar::Entity questChest{71, EntityRadar::Kind::Object, {4, 4}, "Cofre", 0, true};
	questChest.questObject = true;
	entities.push_back(tree); entities.push_back(questChest);
	Command("sound-on"); speech.clear(); Tick();
	Check(Quests() == 2 && Chests() == 0 && speech.empty(), "tree and quest chest use quest audio without duplicate chest sound or speech");
	Command("chests-off"); Check(Quests() == 2, "quest interactables remain audible with container sounds disabled");
	Command("exits-off"); Check(Quests() == 2 && Portals() == 0, "quest interactables remain audible with exit sounds disabled");
	Command("sound-off"); Check(Quests() == 2, "quest objects keep sounding with item sounds disabled");
	Command("quests-off"); Check(voices.empty(), "quest object switch silences both quest objects");
	Command("quests-on"); Check(Quests() == 2, "enabling quest object sounds refreshes them without items, exits or containers");
	Command("sound-on");
	for (auto state : {RadarUi::State::Dialogue, RadarUi::State::Paused, RadarUi::State::Inactive, RadarUi::State::Unavailable}) {
		ui = state; Tick(); Check(voices.empty(), "quest sounds obey dialogue, pause, focus and unavailable UI gates");
	}
	ui = RadarUi::State::Playing; Tick(true);
	Check(Quests() == 2, "nonpausing menus retain quest sounds");
	Event(K::LevelChanged); Check(voices.empty(), "level transition immediately silences quest audio");
	entities.pop_back(); entities.pop_back(); Tick();
	Check(Quests() == 0, "used or unloaded quest objects leave no cached voices after a level transition");
	entities.push_back(tree); Command("sound-on"); Check(Quests() == 1, "unused tree can be found again");
	entities.pop_back(); Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Quests() == 0, "using a tree removes its sound on the native refresh");
	Command("exits-on"); Command("chests-on"); speech.clear();
	for (auto state : {RadarUi::State::Dialogue, RadarUi::State::Paused, RadarUi::State::Inactive, RadarUi::State::Unavailable}) {
		ui = state; Tick(); Check(voices.empty() && !GroundItemService::OwnsWorldLabel("Poción"), "dialogue, pause, lost focus and missing UI silence loot cues");
	}
	ui = RadarUi::State::Playing; Tick(); Check(voices.size() == 2 && speech.empty(), "resume restores audio without repeating stationary loot");
	snapshot.items[0].id = 10; Tick(true);
	Check(voices.size() == 2 && speech.empty(), "nonpausing inventory retains cues but does not compete with menu speech");
	Sleep(static_cast<DWORD>(GroundItems::SpeechGapMs)); Tick(); Check(speech.size() == 1, "closing nonpausing menu permits newly nearby speech");
	Command("sound-off"); Check(voices.size() == 1 && Portals() == 1, "sound toggle immediately cancels item sources but keeps the exit loop");
	snapshot.items[0].id = 11; Sleep(static_cast<DWORD>(GroundItems::SpeechGapMs)); speech.clear(); Tick();
	Check(voices.size() == 1 && speech.size() == 1, "speech works with item loops disabled");
	Command("sound-on"); Check(voices.size() == 2, "loops can be reenabled");
	Command("exits-off"); Check(voices.size() == 1 && Portals() == 0, "exit loops toggle independently of item loops");
	Command("exits-on"); Check(voices.size() == 2 && Portals() == 1, "exit loops can be reenabled");
	// Containers: the same unit read feeds them, they never speak, and they have
	// their own switch.
	entities.push_back(casket); entities.push_back(shrine);
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); speech.clear(); Tick();
	Check(voices.size() == 3 && Chests() == 1, "a container loops, and the shrine beside it does not");
	Check(speech.empty(), "containers are never spoken");
	const int beforeContainers = reads;
	Command("chests-off");
	Check(Chests() == 0 && Portals() == 1 && voices.size() == 2, "the container switch leaves items and exits alone");
	Command("chests-on"); Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Chests() == 1, "container loops can be reenabled");
	Check(reads > beforeContainers, "and the world is still read");
	Command("sound-off"); Command("exits-off"); Command("off");
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); speech.clear(); Tick();
	Check(voices.size() == 1 && Chests() == 1 && speech.empty(), "container loops survive on their own with everything else off");
	Command("chests-off"); Tick();
	Check(voices.empty(), "with every cue off the service falls silent");
	Command("on"); Command("sound-on"); Command("exits-on"); Command("chests-on");
	entities.pop_back(); entities.pop_back();
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Chests() == 0, "an opened container disappears from the unit read and its loop stops");
	// Shut doors loop by the sound the game gives them, under their own switch;
	// a dead body the player can loot uses the corpse loop, not the chest one.
	EntityRadar::Entity gate{34, EntityRadar::Kind::Object, {3, 2}, "Puerta"};
	gate.shutDoor = Generated::DoorSound::Metal;
	EntityRadar::Entity rogue{35, EntityRadar::Kind::Object, {3, 4}, "Arpía muerta", 0, true};
	rogue.corpse = true;
	entities.push_back(gate); entities.push_back(rogue);
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); speech.clear(); Tick();
	Check(Count(GroundItems::Sound::MetalDoor) == 1 && Count(GroundItems::Sound::Corpse) == 1 && Chests() == 0 && speech.empty(),
		"a shut iron gate loops the metal door sound and a dead rogue the corpse sound, neither spoken");
	EntityRadar::Entity rack{36, EntityRadar::Kind::Object, {4, 3}, "Estante de armas", 0, true};
	rack.weaponRack = true;
	entities.push_back(rack);
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::WeaponRack) == 1 && Chests() == 0, "a weapon rack loops its own sound, not the chest one");
	Command("chests-off");
	Check(Count(GroundItems::Sound::WeaponRack) == 0, "and follows the container switch");
	Command("chests-on");
	entities.pop_back();
	// The stash is a fixed place: it loops its own sound with the exits, under a switch of its own.
	entities.push_back({37, EntityRadar::Kind::Object, {4, 4}, "Alijo", 703, false, EntityRadar::Category::Stash});
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); speech.clear(); Tick();
	Check(Count(GroundItems::Sound::Stash) == 1 && Chests() == 0 && speech.empty(), "the stash loops its own sound, unspoken");
	const size_t beforeStash = voices.size();
	openPanels = {"BankExpansionLayout"}; Tick();
	Check(Count(GroundItems::Sound::Stash) == 0 && voices.size() == beforeStash - 1, "an open stash silences only its own loop");
	openPanels = {"BankOriginalLayout"}; Tick();
	Check(Count(GroundItems::Sound::Stash) == 0, "the classic stash layout counts too");
	openPanels.clear(); Tick();
	Check(Count(GroundItems::Sound::Stash) == 1, "closing the stash brings its loop back");
	Command("exits-off");
	Check(Count(GroundItems::Sound::Stash) == 1 && Portals() == 0, "the exit switch leaves the stash alone");
	Command("stash-off");
	Check(Count(GroundItems::Sound::Stash) == 0, "the stash switch silences the stash");
	Command("exits-on");
	Check(Count(GroundItems::Sound::Stash) == 0 && Portals() == 1, "and only the stash");
	Command("stash-on");
	Check(Count(GroundItems::Sound::Stash) == 1, "the stash loop can be reenabled");
	entities.pop_back();
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Command("doors-off");
	Check(Count(GroundItems::Sound::MetalDoor) == 0 && Count(GroundItems::Sound::Corpse) == 1, "the door switch leaves containers alone");
	Command("doors-on"); Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::MetalDoor) == 1, "door loops can be reenabled");
	auto& shut = entities[entities.size() - 2];
	shut.shutDoor = Generated::DoorSound::Gate; Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::WoodenDoor) == 1, "the game's great-door gate sound maps to the wooden loop");
	shut.shutDoor = Generated::DoorSound::Slime; Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::SlimeDoor) == 1, "a slime door has its own loop");
	entities[entities.size() - 2].shutDoor = Generated::DoorSound::None;
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::MetalDoor) == 0, "an opened door falls silent");
	entities.pop_back(); entities.pop_back();
	// An unread quest tome loops the scroll cue with the quest objects; reading it silences it.
	EntityRadar::Entity tome{36, EntityRadar::Kind::Object, {3, 3}, "El tomo mohoso"};
	tome.unreadTome = true;
	entities.push_back(tome);
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); speech.clear(); Tick();
	Check(Count(GroundItems::Sound::Scroll) == 1 && Quests() == 0 && speech.empty(), "an unread tome loops the scroll sound, unspoken");
	Command("quests-off"); Check(Count(GroundItems::Sound::Scroll) == 0, "the quest object switch covers tomes like other quest objects");
	Command("quests-on"); Check(Count(GroundItems::Sound::Scroll) == 1, "and brings them back");
	entities.back().unreadTome = false;
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(Count(GroundItems::Sound::Scroll) == 0, "a read tome falls silent");
	entities.pop_back();
	entities.push_back({21, EntityRadar::Kind::Exit, {4, 3}, "Waypoint", 0, false, EntityRadar::Category::Waypoint});
	entities.push_back({22, EntityRadar::Kind::Exit, {5, 4}, "Stairs", 701, false, EntityRadar::Category::Exit});
	includeBoundaries = true;
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(voices.size() == 5 && Portals() == 3 && Zones() == 1,
		"interactive dungeon stairs, portals and waypoints use portal audio; only the walkable crossing uses zone audio");
	entities.push_back({26, EntityRadar::Kind::Exit, {4, 4}, "Blood Moor warp", 702,
		false, EntityRadar::Category::Exit, 2});
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(voices.size() == 5 && Portals() == 4 && Zones() == 0,
		"an interactive warp replaces the coincident synthetic boundary with one portal cue");
	entities.pop_back();
	Command("exits-off");
	Check(voices.size() == 1 && Portals() == 0 && Zones() == 0, "the exit switch silences zone and portal loops together");
	Command("exits-on");
	Check(voices.size() == 5 && Portals() == 3 && Zones() == 1, "the exit switch restores portal and walking-transition loops");
	snapshot.player = {79, 79}; Tick();
	Check(voices.size() == 1 && Zones() == 1, "a distant border is silent until the player comes within the loop radius");
	snapshot.player = {0, 0};
	includeBoundaries = false;
	entities.pop_back(); entities.pop_back();
	Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick();
	Check(voices.size() == 2 && Zones() == 0 && Portals() == 1, "leaving the boundary removes its zone loop on refresh");
	speech.clear();
	Command("off"); snapshot.items[0].id = 12; speech.clear(); Tick();
	Check(voices.size() == 2 && speech.empty(), "loops work with proximity speech disabled");
	readable = false; Tick(); Check(voices.empty(), "failed native snapshot clears audio"); readable = true;
	Event(K::GameLeft); const int before = reads; Tick(); Check(voices.empty() && reads == before, "leaving a game stops all native reads and loops");
	Command("on"); snapshot.session++; Event(K::GameJoined); Event(K::LocalPlayerReady); speech.clear(); Tick();
	Check(voices.size() == 2 && speech.size() == 1, "new session does not inherit speech suppression or stale voices");
	snapshot.items.clear(); Tick(); Check(voices.size() == 1 && Portals() == 1, "pickup removes its live source on the next snapshot");
	entities.clear(); Sleep(static_cast<DWORD>(GroundItems::ExitRefreshMs)); Tick(); Check(voices.empty(), "a closed portal stops its loop on the next exit refresh");
	GroundItemService::Unregister(); Check(removed == 6 && voices.empty(), "unload detaches all listeners and audio");
	Passed();
}
