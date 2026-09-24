#include "automap_service.h"
#include "async_log.h"
#include "native_automap.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <D2RLPlugin/input.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <set>
#include <string_view>
#include <tuple>
#include <vector>

namespace D2RAccess::AutomapService {
namespace {
using AutomapCells::Cell, AutomapCells::Origin, AutomapCells::Tree;
using Clock = std::chrono::steady_clock;
enum class Command { None, Status, Dump, Watch };
std::atomic<Command> g_command{Command::None};
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService* g_input = nullptr;
std::vector<D2RL::Input::ActionHandle> g_actions;
// A shared layer can hold every object of an act; the log stays readable.
constexpr size_t MaxLoggedCells = 2048;
// The watch reads only the four tree headers on this cadence.
constexpr auto WatchInterval = std::chrono::milliseconds(500);
// Floors grow with nearly every step, so their count is summarised at most this often.
constexpr auto FloorLogInterval = std::chrono::seconds(5);

// What the watch last logged, so it writes only changes.
struct Seen {
	NativeAutomap::Status status = NativeAutomap::Status::Unavailable;
	uint32_t level = 0;
	int32_t layer = -1;
	std::array<uint64_t, AutomapCells::TreeCount> counts{};
};
using CellKey = std::tuple<int, int16_t, int32_t, int32_t>; // tree, cell number, pixel x, pixel y
bool g_watch = false;
Seen g_seen;
std::set<CellKey> g_known;
Clock::time_point g_nextWatch{}, g_nextFloorLog{};

const char* OriginName(Origin origin) {
	return origin == Origin::Session ? "session" : origin == Origin::Restored ? "restored" : "other";
}

bool Landmark(const Cell& cell) { return cell.tree == Tree::Object || cell.tree == Tree::Extra; }
CellKey Key(const Cell& cell) { return {static_cast<int>(cell.tree), cell.number, cell.pixelX, cell.pixelY}; }

struct Bounds {
	int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
	void Add(const Cell& cell) {
		minX = std::min(minX, cell.x); minY = std::min(minY, cell.y);
		maxX = std::max(maxX, cell.x); maxY = std::max(maxY, cell.y);
	}
	bool Empty() const { return minX > maxX; }
};

void LogHeader(const D2RL::PluginContext* context, const NativeAutomap::Snapshot& s) {
	const auto& c = s.layer.counts;
	AsyncLog::InfoF(context, "[automap] status=%s level=%u expected_layer=%d layer=%d sheet=%d reveal_all=%u floors=%llu walls=%llu objects=%llu extras=%llu",
		NativeAutomap::StatusName(s.status), s.level, s.expectedLayer, s.layer.number, s.layer.sheet, s.revealAll,
		static_cast<unsigned long long>(c[0]), static_cast<unsigned long long>(c[1]),
		static_cast<unsigned long long>(c[2]), static_cast<unsigned long long>(c[3]));
}

// One line per tree: origins, the level filter and the decoded extent. Floors
// and walls are tiles; objects and extras are subtiles.
void LogTree(const D2RL::PluginContext* context, const NativeAutomap::Snapshot& s, Tree tree) {
	size_t cells = 0, session = 0, restored = 0, other = 0, inLevel = 0, offLattice = 0;
	Bounds all, level;
	const bool tiles = tree == Tree::Floor || tree == Tree::Wall;
	for (const auto& cell : s.cells) {
		if (cell.tree != tree) continue;
		++cells;
		session += cell.origin == Origin::Session;
		restored += cell.origin == Origin::Restored;
		other += cell.origin == Origin::Other;
		if (tiles && !cell.lattice) { ++offLattice; continue; }
		all.Add(cell);
		if (AutomapCells::InRooms(cell, s.rooms)) { ++inLevel; level.Add(cell); }
	}
	AsyncLog::InfoF(context, "[automap] tree=%s status=%s cells=%zu session=%zu restored=%zu other=%zu in_level=%zu off_lattice=%zu %s=%d,%d..%d,%d level_extent=%d,%d..%d,%d",
		AutomapCells::TreeName(tree), AutomapCells::StatusName(s.layer.trees[static_cast<size_t>(tree)]), cells, session, restored, other,
		inLevel, offLattice, tiles ? "tiles" : "subtiles",
		all.Empty() ? 0 : all.minX, all.Empty() ? 0 : all.minY, all.Empty() ? 0 : all.maxX, all.Empty() ? 0 : all.maxY,
		level.Empty() ? 0 : level.minX, level.Empty() ? 0 : level.minY, level.Empty() ? 0 : level.maxX, level.Empty() ? 0 : level.maxY);
}

// Object and extra cells; with `onlyNew`, just those the watch has not logged yet.
void LogCells(const D2RL::PluginContext* context, const NativeAutomap::Snapshot& s, bool onlyNew) {
	size_t logged = 0, skipped = 0;
	for (const auto& cell : s.cells) {
		if (!Landmark(cell)) continue;
		if (!g_known.insert(Key(cell)).second && onlyNew) continue;
		if (logged == MaxLoggedCells) { ++skipped; continue; }
		++logged;
		const auto rows = s.objectRows.find(cell.number);
		AsyncLog::InfoF(context, "[automap] %s%s cell=%d origin=%s pixel=%d,%d subtile=%d,%d in_level=%u rows=%zu name=%s",
			onlyNew ? "new " : "", cell.tree == Tree::Object ? "object" : "extra", cell.number, OriginName(cell.origin),
			cell.pixelX, cell.pixelY, cell.x, cell.y, AutomapCells::InRooms(cell, s.rooms) ? 1u : 0u,
			rows == s.objectRows.end() ? size_t{0} : rows->second.rows,
			rows == s.objectRows.end() || rows->second.name.empty() ? "-" : rows->second.name.c_str());
	}
	if (skipped) AsyncLog::InfoF(context, "[automap] %zu further object/extra cells not logged", skipped);
}

void Remember(const NativeAutomap::Snapshot& s) {
	g_seen = {s.status, s.level, s.layer.number, s.layer.counts};
}

void Dump(const D2RL::PluginContext* context) {
	const auto started = Clock::now();
	const auto snapshot = NativeAutomap::Read(true);
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
	LogHeader(context, snapshot);
	Remember(snapshot);
	if (snapshot.status != NativeAutomap::Status::Ok) return;
	for (const Tree tree : {Tree::Floor, Tree::Wall, Tree::Object, Tree::Extra}) LogTree(context, snapshot, tree);
	g_known.clear();
	LogCells(context, snapshot, false);
	AsyncLog::InfoF(context, "[automap] dump rooms=%zu read_ms=%lld", snapshot.rooms.size(), static_cast<long long>(elapsed));
}

// Logs what changed since the last line: a level or layer switch dumps in full,
// new object/extra cells are listed alone, and floor growth is summarised.
void Watch(const D2RL::PluginContext* context) {
	const auto now = Clock::now();
	if (now < g_nextWatch) return;
	g_nextWatch = now + WatchInterval;
	const auto head = NativeAutomap::Read(false);
	const auto& counts = head.layer.counts;
	const bool switched = head.status != g_seen.status || head.level != g_seen.level || head.layer.number != g_seen.layer;
	if (switched) { Dump(context); return; }
	const bool landmarks = counts[2] != g_seen.counts[2] || counts[3] != g_seen.counts[3];
	const bool floors = counts[0] != g_seen.counts[0] && now >= g_nextFloorLog;
	if (!landmarks && !floors) return;
	if (floors) g_nextFloorLog = now + FloorLogInterval;
	if (!landmarks) {
		LogHeader(context, head);
		Remember(head);
		return;
	}
	const auto snapshot = NativeAutomap::Read(true);
	LogHeader(context, snapshot);
	Remember(snapshot);
	if (snapshot.status == NativeAutomap::Status::Ok) LogCells(context, snapshot, true);
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "status") g_command = Command::Status;
	else if (arg == "dump") g_command = Command::Dump;
	else if (arg == "watch") g_command = Command::Watch;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_command = Command::Watch;
	return D2RL::Input::ActionResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	// The command stays registered when the check fails, so status can say why.
	(void)context->RegisterConsoleCommand("access-automap", Console,
		"Read-only automap diagnostics: status|dump|watch. Ctrl+J toggles the watch.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "automap-watch", .displayName = "Toggle the automap diagnostic watch", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::J, D2RL::Input::Modifier::Control}, .callback = Action};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) g_actions.push_back(handle);
	}
	return NativeAutomap::Initialize(context);
}

// On the UI tick like the other native readers; the tree walk fails closed if
// the game's layer switch ever ran concurrently (docs/automap-reader.md).
void Tick(const D2RL::PluginContext* context) {
	const Command command = g_command.exchange(Command::None);
	const auto& strings = Strings::Current();
	if (command == Command::Watch) {
		g_watch = !g_watch;
		AsyncLog::InfoF(context, "[automap] watch=%u", g_watch ? 1u : 0u);
		ScreenReader::Speak(g_watch ? strings.automapWatchOn : strings.automapWatchOff, true);
		if (g_watch) Dump(context);
		return;
	}
	if (command == Command::Dump) {
		Dump(context);
		ScreenReader::Speak(strings.automapLogged, true);
	} else if (command == Command::Status) {
		LogHeader(context, NativeAutomap::Read(false));
		ScreenReader::Speak(strings.automapLogged, true);
	} else if (g_watch) {
		Watch(context);
	}
}

void Unregister() {
	if (g_context && g_input)
		for (const auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	g_input = nullptr;
	g_context = nullptr;
	g_command = Command::None;
	g_watch = false;
	g_seen = {};
	g_known.clear();
}

}
