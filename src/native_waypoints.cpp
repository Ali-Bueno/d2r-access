#include "native_waypoints.h"

#include "async_log.h"
#include "entity_layout.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_world.h"
#include "speech_strings.h"

#include <charconv>

namespace D2RAccess::NativeWaypoints {
namespace {
// D2R 3.3.93787, read in the image, not yet measured live: the shown tab's rows as
// nine {int level, byte open} entries, open = activated and not the player's level.
// Evidence and the functions below: docs/waypoints.md.
constexpr uintptr_t Count = 0x2A969D4, Table = 0x2A969E0;
constexpr uintptr_t EntrySize = 8, OpenOffset = 4, TabOffset = 0x168;
constexpr int32_t MaxRows = 9;
constexpr std::string_view PanelPrefix = "WaypointsPanel";
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// The first two hold the table's and the count's addresses unmasked, so a moved table fails them.
constexpr Evidence Checks[] = {
	{0x1EA9B0, 1077, 0x0E1C8C0518116BADULL}, // FillRows
	{0x1EC430, 1006, 0xC4727BD29B8D6CFFULL}, // StyleRows
	{0x1EC820, 723, 0x3F09072B1AB33F92ULL},  // SelectTab: the tab at +0x168
};
uintptr_t g_base = 0;

// "Act1 Button3" -> act 1, row 3.
bool Parse(std::string_view name, int& act, int& index) {
	constexpr std::string_view Act = "Act", Button = " Button";
	if (!name.starts_with(Act)) return false;
	name.remove_prefix(Act.size());
	const auto actEnd = std::from_chars(name.data(), name.data() + name.size(), act);
	if (actEnd.ec != std::errc{}) return false;
	name.remove_prefix(static_cast<size_t>(actEnd.ptr - name.data()));
	if (!name.starts_with(Button)) return false;
	name.remove_prefix(Button.size());
	const auto rowEnd = std::from_chars(name.data(), name.data() + name.size(), index);
	return rowEnd.ec == std::errc{} && rowEnd.ptr == name.data() + name.size() && act >= 1 && index >= 0;
}

int32_t PlayerLevel() {
	using namespace EntityLayout;
	uintptr_t extended = 0, level = 0;
	int32_t id = 0;
	const uintptr_t room = NativeWorld::PlayerRoom();
	if (!room || !NativeMemory::Peek(room + RoomExtended, extended) || !extended
		|| !NativeMemory::Peek(extended + ExtendedLevel, level) || !level || !NativeMemory::Peek(level + LevelId, id)) return 0;
	return id;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "waypoint", "waypoint rows are not told apart"))
		return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[waypoint] Verified.");
	return true;
}

Row Read(uintptr_t panel, std::string_view name) noexcept {
	Row row;
	int act = 0, index = 0;
	if (!g_base || !panel || !Parse(name, act, index)) return row;
	const NativeMemory::ReadScope scope;
	if (!NativeMemory::Peek(panel + TabOffset, row.tab) || row.tab != act - 1
		|| !NativeMemory::Peek(g_base + Count, row.count) || row.count > MaxRows || index >= row.count
		|| !NativeMemory::Peek(g_base + Table + EntrySize * static_cast<uintptr_t>(index), row.level)
		|| !NativeMemory::Peek(g_base + Table + EntrySize * static_cast<uintptr_t>(index) + OpenOffset, row.open)
		|| row.level <= 0 || row.open > 1) return row;
	row.player = PlayerLevel();
	row.state = row.level == row.player ? State::Here : row.open ? State::Open : State::Undiscovered;
	return row;
}

std::string FocusWords(std::string_view panelName, uintptr_t panel, std::string_view control) {
	if (!panelName.starts_with(PanelPrefix)) return {};
	const auto& strings = Strings::Current();
	switch (Read(panel, control).state) {
	case State::Here:         return strings.waypointHere;
	case State::Undiscovered: return strings.waypointUndiscovered;
	default:                  return {};
	}
}

const char* Name(State state) {
	switch (state) {
	case State::Open:         return "open";
	case State::Here:         return "here";
	case State::Undiscovered: return "undiscovered";
	default:                  return "unknown";
	}
}
}
