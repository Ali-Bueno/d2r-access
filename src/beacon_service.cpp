#include "beacon_service.h"
#include "async_log.h"
#include "beacon.h"
#include "beacon_audio.h"
#include "input_direction.h"
#include "native_entities.h"
#include "native_ground_items.h"
#include "native_level_exits.h"
#include "native_world.h"
#include "radar_config.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <numbers>
#include <string>
#include <string_view>

namespace D2RAccess::BeaconService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
enum class Command { None, Toggle, Clear, Status };
// The locked target is re-read on this cadence: fast enough to follow a
// walking monster, far cheaper than the 50 ms UI poll.
constexpr uint64_t UpdateMs = 150;
// A scan result can be locked while it is still what the player heard last.
constexpr uint64_t OfferWindowMs = 15000;
constexpr uint64_t LogIntervalMs = 1000;

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0, revision = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
Beacon::Settings g_settings;
Beacon::Tracker g_tracker;
// `point` marks a way out that is not a unit (a passage between two streamed
// rooms): a fixed place, so the per-tick refresh must not look for it.
struct Target { EntityRadar::Entity entity; int level = 0; uint64_t offeredMs = 0, seenMs = 0; bool point = false; };
Target g_offer, g_target;
bool g_supported = false;
int g_revision = 0;
uint64_t g_nextUpdate = 0, g_lastLog = 0;

bool Playing() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return RadarUi::Inspect(process == GetCurrentProcessId()) == RadarUi::State::Playing;
}
// Screen placement of a world direction, the ground-item loops' convention:
// native projection 0x334DB0 turns the world diagonals into the screen axes.
BeaconAudio::Placement Place(Beacon::Position heading) {
	constexpr double Diagonal = std::numbers::sqrt2 / 2;
	return {(heading.x - heading.y) * Diagonal, -(heading.x + heading.y) * Diagonal};
}
void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	++g_state.revision;
}
void Release(const char* reason) {
	if (g_tracker.Locked()) AsyncLog::InfoF(g_context, "[beacon] released reason=%s id=%u name=%s", reason, g_target.entity.id, g_target.entity.name.c_str());
	g_tracker.Clear();
	g_target = {};
	BeaconAudio::Silence(reason);
}
auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "lock" || arg == "toggle") g_command = Command::Toggle;
	else if (arg == "clear" || arg == "off") g_command = Command::Clear;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* data) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = static_cast<Command>(reinterpret_cast<uintptr_t>(data));
	return D2RL::Input::ActionResult::Handled;
}
void Announce(const char* format, const std::string& name) {
	char text[256]{};
	std::snprintf(text, sizeof text, format, name.c_str());
	ScreenReader::Speak(text, true);
}
// The word for how the player gets there, on this snapshot of the collision.
const char* ReachWord(const Lifecycle& state, Radar::Position target) {
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return nullptr;
	return Beacon::ReachWord(world, target, g_settings, Strings::Current());
}
void Toggle(const Lifecycle& state, uint64_t now) {
	const auto& strings = Strings::Current();
	if (g_tracker.Locked()) {
		Release("cancelled");
		BeaconAudio::Play(BeaconAudio::Sound::Cleared, {});
		ScreenReader::Speak(strings.beaconCleared, true);
		return;
	}
	if (!BeaconAudio::Available()) { ScreenReader::Speak(strings.beaconUnavailable, true); return; }
	if ((!g_offer.entity.id && !g_offer.point) || g_offer.level != state.level || now - g_offer.offeredMs > OfferWindowMs) {
		ScreenReader::Speak(strings.beaconNothing, true);
		return;
	}
	g_target = g_offer;
	g_target.seenMs = now;
	g_tracker.Lock(now);
	g_nextUpdate = now;
	BeaconAudio::Play(BeaconAudio::Sound::Locked, {});
	const char* reach = ReachWord(state, g_target.entity.position);
	if (reach) {
		char text[256]{};
		std::snprintf(text, sizeof text, strings.beaconFollowingReach, g_target.entity.name.c_str(), reach);
		ScreenReader::Speak(text, true);
	} else Announce(strings.beaconFollowing, g_target.entity.name);
	AsyncLog::InfoF(g_context, "[beacon] locked id=%u kind=%u at=%.0f,%.0f level=%d reach=%s name=%s", g_target.entity.id,
		static_cast<unsigned>(g_target.entity.kind), g_target.entity.position.x, g_target.entity.position.y,
		g_target.level, reach ? reach : "unknown", g_target.entity.name.c_str());
}
// Re-reads the locked target's current position. A target that is out of the
// loaded rooms keeps its last known place: that is still where it was.
bool Refresh(const Lifecycle& state, uint64_t now) {
	if (g_target.point) { g_target.seenMs = now; return true; } // a passage does not move
	std::vector<EntityRadar::Entity> entities;
	if (g_target.entity.kind == EntityRadar::Kind::Item) {
		GroundItems::Snapshot snapshot;
		if (!NativeGroundItems::Read(state.player, state.session, snapshot)) return false;
		for (const auto& item : snapshot.items)
			if (item.id == g_target.entity.id) {
				g_target.entity.position = {item.position.x, item.position.y};
				g_target.seenMs = now;
				return true;
			}
		return false;
	}
	if (!NativeEntities::Read(state.player, state.session, entities, g_target.entity.levelExitId != 0)) return false;
	for (const auto& entity : entities)
		if (EntityRadar::SameTarget(entity, g_target.entity)) {
			g_target.entity.position = entity.position;
			g_target.seenMs = now;
			return true;
		}
	return false;
}
void Update(const D2RL::PluginContext* context, const Lifecycle& state, uint64_t now) {
	if (!Playing()) return; // dialogue, pause or another window: keep the lock, stay quiet
	Radar::World world;
	if (!NativeWorld::Read(state.player, state.session, state.level, world)) return;
	const bool seen = Refresh(state, now);
	const auto update = g_tracker.Tick(world, g_target.entity.position, now, g_settings);
	if (!update.valid) return;
	if (update.arrived) {
		const auto name = g_target.entity.name;
		const bool present = seen;
		Release("arrived");
		BeaconAudio::Play(BeaconAudio::Sound::Arrived, {});
		Announce(present ? Strings::Current().beaconArrived : Strings::Current().beaconLost, name);
		return;
	}
	if (update.ping) BeaconAudio::Play(BeaconAudio::Sound::Ping, Place(update.heading));
	if (!g_lastLog || now - g_lastLog >= LogIntervalMs) {
		g_lastLog = now;
		AsyncLog::InfoF(context, "[beacon] position=%.0f,%.0f target=%.0f,%.0f seen=%u distance=%.1f remaining=%.1f routed=%u heading=%.2f,%.2f route_cells=%zu name=%s",
			world.player.x, world.player.y, g_target.entity.position.x, g_target.entity.position.y, seen ? 1u : 0u,
			update.distance, update.remaining, update.routed ? 1u : 0u, update.heading.x, update.heading.y,
			g_tracker.CurrentRoute().path.size(), g_target.entity.name.c_str());
	}
}
}

void Offer(const EntityRadar::Entity& entity, int level, uint64_t now) {
	if (!entity.id) return;
	g_offer = {entity, level, now, now};
}

void OfferPoint(Radar::Position point, const std::string& label, int level, uint64_t now) {
	if (label.empty()) return;
	// Coordinate identity, the one the fixed level exits already use, instead of
	// inventing a unit id for something the game has no unit for.
	g_offer = {{0, EntityRadar::Kind::Exit, point, label, NativeLevelExits::Identity(point)}, level, now, now, true};
}

bool Register(const D2RL::PluginContext* context) {
	g_context = context;
	if (!context || context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) { Unregister(); return false; }
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		// Keyboard equivalent of the right-stick click, for testing without a pad.
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "beacon-toggle", .displayName = "Follow or release the scanned target", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::B, D2RL::Input::Modifier::Control}, .callback = Action,
			.userData = reinterpret_cast<void*>(Command::Toggle)};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	(void)context->RegisterConsoleCommand("access-beacon", Console,
		"Target beacon: lock|clear|status. Right stick click, or Ctrl+B, follows what the scan last named.");
	g_supported = BeaconAudio::Start(context, static_cast<double>(RadarConfig::Read(context).volume) / RadarConfig::MaxVolume);
	AsyncLog::InfoF(context, "[beacon] Registered; audio=%u.", g_supported ? 1u : 0u);
	return true;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	const bool clicked = InputDirection::TakeRightStickClick();
	const uint64_t now = GetTickCount64();
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (state.revision != g_revision) {
		g_revision = state.revision;
		Release("lifecycle");
		g_offer = {};
	}
	if (command == Command::Clear && g_tracker.Locked()) {
		Release("cancelled");
		BeaconAudio::Play(BeaconAudio::Sound::Cleared, {});
		ScreenReader::Speak(Strings::Current().beaconCleared, true);
	} else if (command == Command::Toggle || clicked) {
		Toggle(state, now);
	}
	if (command == Command::Status) {
		const auto& strings = Strings::Current();
		if (!g_tracker.Locked()) ScreenReader::Speak(strings.beaconNothing, true);
		else Announce(strings.beaconFollowing, g_target.entity.name);
	}
	if (!g_tracker.Locked() || !state.ready || now < g_nextUpdate) return;
	g_nextUpdate = now + UpdateMs;
	Update(context, state, now);
}

void Unregister() {
	Release("unregister");
	BeaconAudio::Stop();
	if (g_context && g_lifecycle) for (const auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	if (g_context && g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_listeners.clear();
	g_actions.clear();
	{ std::lock_guard lock(g_mutex); g_state = {}; }
	g_context = nullptr;
	g_lifecycle = nullptr;
	g_input = nullptr;
	g_offer = g_target = {};
	g_supported = false;
	g_revision = 0;
	g_nextUpdate = g_lastLog = 0;
	g_command = Command::None;
}
}
