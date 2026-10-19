#include "place_service.h"
#include "async_log.h"
#include "descriptions.h"
#include "entity_radar.h"
#include "level_exit_layout.h"
#include "native_entities.h"
#include "native_level_exits.h"
#include "native_preset_map.h"
#include "native_world.h"
#include "places.h"
#include "radar_ui.h"
#include "room_service.h"
#include "rooms.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::PlaceService {
namespace {
using Kind = D2RL::Lifecycle::GameplayEventKind;
using Generated::PlaceFeature;
using Generated::PlaceInterior;
using Generated::PresetPlace;
using Passages::Shape;
enum class Command { None, On, Off, Toggle, Status, Where, Dump };

static_assert(static_cast<size_t>(PresetPlace::Count) == Strings::PresetPlaceWords);
static_assert(static_cast<size_t>(PlaceInterior::Count) == Strings::PlaceInteriorWords);
static_assert(static_cast<size_t>(PlaceFeature::Count) == Strings::PlaceFeatureWords);

// Walking a few steps is slow; the room service polls at the same rate.
constexpr uint64_t PollMs = 250;
// Being at a landmark: on its footprint or within one map tile of it, walked.
// The user asked for the forge when passing through the forge, not near a point of it.
constexpr int FeatureRadius = LevelExitLayout::SubtilesPerTile;
// Corridor yardsticks scale with the maze's own doorway width
// (Rooms::TypicalOpeningWidth), so the Catacombs and the sewers calibrate
// themselves: docs/places.md#corridors. Irregular walls widen a corridor by a
// cell or two; it is followed this many widths each way. Check by listening.
constexpr int CorridorWidthSlack = 2, CorridorFollowWidths = 6;
// Half a screen each way of the live grid, written by `access-places dump`.
constexpr int DumpRadius = 40;

struct Lifecycle { bool ready = false; uint32_t player = 0; uint64_t session = 0; int level = 0; };
std::mutex g_mutex;
Lifecycle g_state;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Lifecycle::ListenerHandle> g_listeners;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{true};
bool g_supported = false;
Rooms::BasicHysteresis<Places::Spot> g_spots;
uint64_t g_landingAt = 0; // when the player last stood by a cliff stairway's landing
uint64_t g_nextPoll = 0;
std::string g_lastPiece; // the last piece line logged, so it is written on change only
// The last named piece walked into. A river or a row of cottages is several
// pieces with plain ground between them; near the last one, the name holds.
struct Named { PresetPlace preset = PresetPlace::None; int level = 0, minX = 0, minY = 0, maxX = 0, maxY = 0; uint64_t map = 0, seenAt = 0; };
Named g_named;
// A named piece worth spoken from further out (Places::Padding), read off the loaded rooms now and then.
struct Padded { Named piece; uint64_t key = 0; };
std::vector<Padded> g_padded;
int g_paddedLevel = -1;
uint64_t g_paddedAt = 0;
constexpr uint64_t PaddedRescanMs = 2000;

void __cdecl OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	std::lock_guard lock(g_mutex);
	g_state.session = event->sessionGeneration;
	if (event->kind == Kind::GameJoined || event->kind == Kind::GameLeft) { g_state.ready = false; g_state.level = 0; }
	else if (event->kind == Kind::LocalPlayerReady) { g_state.ready = true; g_state.player = event->playerId; g_state.level = 0; }
	else if (event->kind == Kind::ActChanged) g_state.level = 0;
	else if (event->kind == Kind::LevelChanged) g_state.level = event->currentValue;
	g_spots.Forget();
}
bool Playing() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return RadarUi::Inspect(process == GetCurrentProcessId()) == RadarUi::State::Playing;
}
// The terrain the player walks on, doors open: actors and closed doors are not walls of a room.
Passages::Ground Ground(const Radar::World& world, int x, int y) {
	const auto state = world.Cell(x, y, Radar::Layer::Terrain);
	if (state == Radar::State::Unknown) return Passages::Ground::Unknown;
	return Radar::Passable(state) || state == Radar::State::Door ? Passages::Ground::Open : Passages::Ground::Blocked;
}

struct Reading {
	Radar::World         world;
	Places::Spot         spot;
	NativePresetMap::Map map;
	Places::Piece        piece;
	int                  originX = 0, originY = 0;
	int                  seed = Places::None;
	Passages::Reading    passage;
	Passages::Settings   corridor;
	NativePresetMap::Trace trace;
	bool                 known = false; // the piece has generated places
};

std::optional<Passages::Settings> CorridorSettings(const Radar::World& world) {
	const int typical = Rooms::TypicalOpeningWidth(world);
	if (typical <= 0) return std::nullopt;
	return Passages::Settings{.minWidth = typical / 2, .maxWidth = typical + CorridorWidthSlack, .minLength = typical,
		.slack = typical / 2, .reach = typical * CorridorFollowWidths};
}

// The pieces of Places::Padding named in the loaded rooms, rescanned every couple of seconds.
void RefreshPadded(int level, uint64_t now) {
	if (level == g_paddedLevel && now - g_paddedAt < PaddedRescanMs) return;
	g_paddedLevel = level;
	g_paddedAt = now;
	g_padded.clear();
	const int s = LevelExitLayout::SubtilesPerTile;
	for (const uintptr_t room : NativeWorld::LoadedRooms()) {
		NativePresetMap::Map map;
		if (!NativePresetMap::Read(room, map)) continue;
		const auto preset = Places::NameOf(map.def, map.path);
		if (Places::Padding(preset) == 0) continue;
		const uint64_t key = Places::MapKey(map.def, map.tileX, map.tileY);
		if (std::any_of(g_padded.begin(), g_padded.end(), [key](const Padded& p) { return p.key == key; })) continue;
		g_padded.push_back({{preset, level, map.tileX * s, map.tileY * s, (map.tileX + map.tileWidth) * s,
			(map.tileY + map.tileHeight) * s, key, now}, key});
	}
}

// Keeps the name of the last named piece while the player stays within one piece's size of it,
// so walking along a river made of several pieces names it once; a building's name lapses with time.
// A piece worth a wider berth (Places::Padding) is named from there.
void Remember(int level, Reading& out, int x, int y) {
	const uint64_t now = GetTickCount64();
	const int s = LevelExitLayout::SubtilesPerTile;
	if (out.spot.preset != PresetPlace::None) {
		g_named = {out.spot.preset, level, out.map.tileX * s, out.map.tileY * s,
			(out.map.tileX + out.map.tileWidth) * s, (out.map.tileY + out.map.tileHeight) * s, out.spot.map, now};
		out.spot.namedMap = out.spot.map;
		return;
	}
	if (g_named.preset != PresetPlace::None && g_named.level == level) {
		const int marginX = g_named.maxX - g_named.minX, marginY = g_named.maxY - g_named.minY;
		if (Places::HoldsName(g_named.preset, static_cast<long long>(now - g_named.seenAt))
			&& x >= g_named.minX - marginX && x < g_named.maxX + marginX && y >= g_named.minY - marginY && y < g_named.maxY + marginY) {
			out.spot.preset = g_named.preset;
			out.spot.namedMap = g_named.map;
			return;
		}
		g_named = {};
	}
	RefreshPadded(level, now);
	for (const auto& padded : g_padded) {
		const int pad = Places::Padding(padded.piece.preset) * s;
		if (x < padded.piece.minX - pad || x >= padded.piece.maxX + pad || y < padded.piece.minY - pad || y >= padded.piece.maxY + pad) continue;
		g_named = padded.piece;
		g_named.seenAt = now;
		out.spot.preset = padded.piece.preset;
		out.spot.namedMap = padded.key;
		return;
	}
}

// Where the player stands; false when the world itself is unreadable.
bool Observe(const Lifecycle& state, Reading& out) {
	out = {};
	if (!NativeWorld::Read(state.player, state.session, state.level, out.world)) return false;
	const auto& world = out.world;
	out.spot.valid = true;
	const int x = static_cast<int>(std::floor(world.player.x)), y = static_cast<int>(std::floor(world.player.y));
	const auto probe = [&world](int cx, int cy) { return Ground(world, cx, cy); };
	// Corridors only where the room partition is the maze's own: elsewhere the
	// typical opening is an artefact of how open ground was cut up.
	if (RoomService::Maze(state.level))
		if (const auto settings = CorridorSettings(world)) {
			out.corridor = *settings;
			out.passage = Passages::Classify(probe, x, y, out.corridor);
			out.spot.passage = out.passage.shape;
			out.spot.deadEnd = out.passage.DeadEnd();
		}
	const bool mapped = NativePresetMap::Read(NativeWorld::PlayerRoom(), out.map, &out.trace);
	if (mapped) {
		out.spot.map = Places::MapKey(out.map.def, out.map.tileX, out.map.tileY);
		out.spot.preset = Places::NameOf(out.map.def, out.map.path);
	}
	Remember(state.level, out, x, y);
	if (!mapped) return true; // open ground no map owns
	const auto* places = Places::Find(out.map.path);
	if (!places) return true;
	out.known = true;
	out.piece = Places::PieceOf(*places);
	out.originX = out.map.tileX * LevelExitLayout::SubtilesPerTile;
	out.originY = out.map.tileY * LevelExitLayout::SubtilesPerTile;
	out.spot.interior = Places::InteriorAt(out.piece, x - out.originX, y - out.originY);
	const auto feature = Places::Nearest(out.piece, out.originX, out.originY, x, y, out.spot.interior,
		[&probe](int cx, int cy) { return probe(cx, cy) == Passages::Ground::Open; }, FeatureRadius);
	out.spot.feature = feature.kind;
	out.seed = feature.seed;
	return true;
}

// What a corridor leads to: a level exit near one of its open ends, or else a
// landmark of this piece there. Read only when a corridor is about to be spoken.
std::vector<std::string> Towards(const Lifecycle& state, const Reading& reading) {
	std::vector<std::string> targets;
	if (reading.passage.shape != Shape::Corridor) return targets;
	std::vector<EntityRadar::Entity> entities;
	(void)NativeEntities::Read(state.player, state.session, entities);
	const double around = reading.corridor.maxWidth * 2.0;
	const auto& strings = Strings::Current();
	const auto open = [&world = reading.world](int cx, int cy) { return Ground(world, cx, cy) == Passages::Ground::Open; };
	for (const auto& end : reading.passage.ends) {
		if (end.end == Passages::End::Closed) continue;
		const EntityRadar::Entity* exit = nullptr;
		double best = around;
		for (const auto& entity : entities) {
			if (entity.kind != EntityRadar::Kind::Exit || entity.name.empty()) continue;
			const double distance = std::hypot(entity.position.x - end.x, entity.position.y - end.y);
			if (distance <= best) { best = distance; exit = &entity; }
		}
		std::string target = exit ? exit->name : std::string{};
		if (target.empty() && !reading.piece.seeds.empty()) {
			const int interior = Places::InteriorAt(reading.piece, end.x - reading.originX, end.y - reading.originY);
			const auto feature = Places::Nearest(reading.piece, reading.originX, reading.originY, end.x, end.y, interior, open, FeatureRadius);
			if (feature.seed != Places::None) target = strings.placeFeatures[static_cast<size_t>(feature.kind)];
		}
		if (target.empty()) continue;
		char text[256]{};
		std::snprintf(text, sizeof text, strings.placeTowards, target.c_str());
		if (std::find(targets.begin(), targets.end(), text) == targets.end()) targets.emplace_back(text);
	}
	return targets;
}

// The spoken line; one piece per change, never the same word twice in a row
// (a forge piece holding an anvil is one "forge").
std::string Phrase(const Reading& reading, const Places::Change& change, const std::vector<std::string>& towards) {
	const auto& spot = reading.spot;
	const auto& strings = Strings::Current();
	std::string told; // a landmark's words, said instead of its name the first time (a tester, 2026-10-16)
	std::vector<std::string_view> parts;
	const auto add = [&](std::string_view word) { if (!word.empty() && (parts.empty() || parts.back() != word)) parts.push_back(word); };
	if (change.preset) add(strings.placePresets[static_cast<size_t>(spot.preset)]);
	if (change.interior) add(strings.placeInteriors[static_cast<size_t>(reading.piece.volumes[static_cast<size_t>(spot.interior)].interior)]);
	else if (change.outside) add(strings.placeOutside);
	if (change.node) add(spot.passage == Shape::Junction ? strings.placeJunction : strings.placeBend);
	// Out of a junction, a corridor is news only when it is shut or leads somewhere named.
	if (change.corridor || (change.branch && (spot.deadEnd || !towards.empty()))) {
		add(spot.deadEnd ? strings.placeDeadEnd : strings.placeCorridor);
		for (const auto& target : towards) add(target);
	}
	if (change.feature) {
		const std::string_view name = strings.placeFeatures[static_cast<size_t>(spot.feature)];
		told = Descriptions::FirstTime(name, spot.feature == Places::PlaceFeature::HangmanTree ? strings.hangmanTreeDescription : "");
		add(told.empty() ? name : std::string_view(told));
	}
	std::string line;
	for (const auto part : parts) {
		if (!line.empty()) line += strings.listSeparator;
		line += part;
	}
	return line;
}

void Announce(const D2RL::PluginContext* context, const Lifecycle& state, const Reading& reading, const Places::Change& change, bool interrupt) {
	const auto towards = change.corridor || change.branch ? Towards(state, reading) : std::vector<std::string>{};
	const std::string line = Phrase(reading, change, towards);
	if (line.empty()) return;
	ScreenReader::Speak(line.c_str(), interrupt);
	const auto& ends = reading.passage.ends;
	AsyncLog::InfoF(context,
		"[places] map=%s def=%d origin=%d,%d interior=%d feature=%d seed=%d passage=%d dead=%u ends=%d,%d:%d %d,%d:%d width=%d..%d length=%d -> %s",
		reading.map.path.c_str(), reading.map.def, reading.map.tileX, reading.map.tileY, reading.spot.interior,
		static_cast<int>(reading.spot.feature), reading.seed, static_cast<int>(reading.passage.shape), reading.spot.deadEnd ? 1u : 0u,
		ends[0].x, ends[0].y, static_cast<int>(ends[0].end), ends[1].x, ends[1].y, static_cast<int>(ends[1].end),
		reading.corridor.minWidth, reading.corridor.maxWidth, reading.corridor.minLength, line.c_str());
}

// Which piece the player stands in and how far reading it got; logged when it changes.
void LogPiece(const D2RL::PluginContext* context, const Lifecycle& state, const Reading& reading) {
	const int x = static_cast<int>(std::floor(reading.world.player.x)), y = static_cast<int>(std::floor(reading.world.player.y));
	char text[512]{};
	std::snprintf(text, sizeof text,
		"[places] piece level=%d stage=%s type=%d path=%s%s def=%d file=%d origin=%d,%d size=%dx%d places=%u seeds=%zu volumes=%zu interior=%d",
		state.level, reading.trace.stage, reading.trace.type, reading.map.path.c_str(), reading.trace.path.c_str(), reading.map.def,
		reading.map.file, reading.map.tileX, reading.map.tileY, reading.map.tileWidth, reading.map.tileHeight, reading.known ? 1u : 0u,
		reading.piece.seeds.size(), reading.piece.volumes.size(), reading.spot.interior);
	if (g_lastPiece == text) return;
	g_lastPiece = text;
	AsyncLog::InfoF(context, "%s player=%d,%d local=%d,%d", text, x, y, x - reading.originX, y - reading.originY);
}

// The whole place on request, as if just walked into.
void Where(const D2RL::PluginContext* context, const Lifecycle& state) {
	Reading reading;
	if (!Observe(state, reading)) { ScreenReader::Speak(Strings::Current().placesUnavailable, true); return; }
	const auto change = Places::Compare({}, reading.spot);
	if (!change.Any()) {
		ScreenReader::Speak(Strings::Current().placeOutside, true);
		return;
	}
	Announce(context, state, reading, change, true);
}

// The live terrain around the player, one log line per row, to tune the
// corridor yardsticks on real ground: '#' wall, '.' floor, '?' not streamed, '@' the player.
void Dump(const D2RL::PluginContext* context, const Lifecycle& state) {
	Reading reading;
	if (!Observe(state, reading)) return;
	const int px = static_cast<int>(std::floor(reading.world.player.x)), py = static_cast<int>(std::floor(reading.world.player.y));
	AsyncLog::InfoF(context, "[places] dump level=%d at=%d,%d typical=%d passage=%d width=%d..%d length=%d",
		state.level, px, py, Rooms::TypicalOpeningWidth(reading.world), static_cast<int>(reading.passage.shape),
		reading.corridor.minWidth, reading.corridor.maxWidth, reading.corridor.minLength);
	for (int y = py - DumpRadius; y <= py + DumpRadius; ++y) {
		std::string row;
		for (int x = px - DumpRadius; x <= px + DumpRadius; ++x) {
			const auto ground = Ground(reading.world, x, y);
			row += x == px && y == py ? '@' : ground == Passages::Ground::Open ? '.' : ground == Passages::Ground::Blocked ? '#' : '?';
		}
		AsyncLog::InfoF(context, "[places] row %d %s", y, row.c_str());
	}
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "where") g_command = Command::Where;
	else if (arg == "dump") g_command = Command::Dump;
	else if (arg.empty() || arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Toggle(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = Command::Toggle;
	return D2RL::Input::ActionResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_supported = NativeWorld::Initialize(context) && NativePresetMap::Initialize(context);
	(void)context->RegisterConsoleCommand("access-places", Console,
		"Named places and corridors: on|off|status, where to hear the current one, dump to log the ground around you.");
	if (context->QueryService(&g_lifecycle) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasLifecycleServiceField(g_lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged}) {
		const D2RL::Lifecycle::GameplayEventListener listener{
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize, .kind = kind, .callback = OnGameplay};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &listener, &handle)
			!= D2RL::Lifecycle::Result::Success) { Unregister(); return false; }
		g_listeners.push_back(handle);
	}
	AsyncLog::InfoF(context, "[places] supported=%u pieces=%zu", g_supported ? 1u : 0u, std::size(Generated::PresetPlacesTable));
	return g_supported;
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	const auto command = g_command.exchange(Command::None);
	Lifecycle state;
	{ std::lock_guard lock(g_mutex); state = g_state; }
	if (command == Command::Where || command == Command::Dump) {
		if (g_supported && state.ready) (command == Command::Where ? Where : Dump)(context, state);
		return;
	}
	if (command != Command::None) {
		if (command != Command::Status) {
			g_enabled = command == Command::On || (command == Command::Toggle && !g_enabled);
			std::lock_guard lock(g_mutex);
			g_spots.Forget();
		}
		const auto& strings = Strings::Current();
		ScreenReader::Speak(!g_supported ? strings.placesUnavailable
			: g_enabled ? strings.placesEnabled : strings.placesDisabled, true);
		AsyncLog::InfoF(context, "[places] enabled=%u supported=%u", g_enabled ? 1u : 0u, g_supported ? 1u : 0u);
		return;
	}
	if (!g_supported || !g_enabled) return;
	const uint64_t now = GetTickCount64();
	if (uiOwnsSpeech || now - g_nextPoll < PollMs) return;
	g_nextPoll = now;
	if (!state.ready || !Playing()) return;
	Reading reading;
	if (!Observe(state, reading)) {
		if (g_lastPiece != "unreadable") AsyncLog::InfoF(context, "[places] world unreadable on level %d", state.level);
		g_lastPiece = "unreadable";
		return;
	}
	LogPiece(context, state, reading);
	std::lock_guard lock(g_mutex);
	if (Places::Stairway(reading.spot.feature)) g_landingAt = now;
	const Places::Spot before = Places::HeldStairway(g_spots.current, static_cast<long long>(now - g_landingAt));
	reading.spot = Places::Carry(before, reading.spot);
	if (!g_spots.Observe(reading.spot, static_cast<long long>(now))) return;
	Announce(context, state, reading, Places::Compare(before, reading.spot), false);
}

bool Enabled() { return g_enabled.load(); }
void SetEnabled(bool on) { g_command = on ? Command::On : Command::Off; }

void Unregister() {
	if (g_lifecycle) for (auto handle : g_listeners) (void)g_lifecycle->unregisterGameplayEventListener(g_context, handle);
	g_listeners.clear();
	if (g_input) for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	std::lock_guard lock(g_mutex);
	g_spots.Forget();
	g_state = {};
	g_enabled = true; g_supported = false; g_nextPoll = 0;
	g_lastPiece.clear();
	g_named = {};
	g_padded.clear();
	g_paddedLevel = -1;
	g_lifecycle = nullptr; g_input = nullptr; g_context = nullptr;
}

std::vector<EntityRadar::Entity> MapLandmarks() {
	std::vector<EntityRadar::Entity> found;
	if (!g_supported) return found;
	std::vector<uint64_t> maps;
	for (const uintptr_t room : NativeWorld::LoadedRooms()) {
		NativePresetMap::Map map;
		if (!NativePresetMap::Read(room, map)) continue;
		// The rooms of one piece share its map.
		const uint64_t key = Places::MapKey(map.def, map.tileX, map.tileY);
		if (std::find(maps.begin(), maps.end(), key) != maps.end()) continue;
		maps.push_back(key);
		const auto* places = Places::Find(map.path);
		if (!places) continue;
		const int originX = map.tileX * LevelExitLayout::SubtilesPerTile, originY = map.tileY * LevelExitLayout::SubtilesPerTile;
		for (const auto& seed : Places::PieceOf(*places).seeds) {
			if (!Places::Tracked(seed.feature)) continue;
			EntityRadar::Entity entity;
			entity.kind = EntityRadar::Kind::Object;
			entity.category = EntityRadar::Category::Interactable;
			entity.position = {originX + (seed.minX + seed.maxX) / 2.0, originY + (seed.minY + seed.maxY) / 2.0};
			entity.placeId = NativeLevelExits::Identity(entity.position);
			entity.name = Strings::Current().placeFeatures[static_cast<size_t>(seed.feature)];
			entity.mapLandmark = true;
			found.push_back(std::move(entity));
		}
	}
	return found;
}
}
