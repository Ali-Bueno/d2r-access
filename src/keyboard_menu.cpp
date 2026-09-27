#include "keyboard_menu.h"

#include "async_log.h"
#include "grid_jump.h"
#include "inventory_cursor.h"
#include "native_skill_navigation.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "skill_grid.h"
#include "skill_navigation.h"

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace D2RAccess::KeyboardMenu {
namespace {
using GridJump::Direction;

// A move of the cursor is the key's when the key is down, or went down this
// recently: the game may act on a tap after the key is already up again.
constexpr uint64_t RecentMs = 250;
constexpr int Keys[] = {VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN}; // GridJump::Direction order

bool g_down[4]{};
int g_last = -1;       // the arrow that went down last, as an index into Keys
uint64_t g_lastMs = 0;

uintptr_t g_grid = 0;  // the grid the cursor was in at the last update
GridJump::Cell g_cell; // the cell the game's cursor was on
GridJump::Cell g_cursor; // the cell travelled through, inside what stands on g_cell
uintptr_t g_skill = 0; // the skill the cursor was on

// The arrow a move now would belong to, or -1: the one held, the latest first.
int Arrow(uint64_t now) {
	int held = -1;
	for (int i = 0; i < 4; ++i) {
		const bool down = (GetAsyncKeyState(Keys[i]) & 0x8000) != 0;
		if (down && !g_down[i]) { g_last = i; g_lastMs = now; }
		g_down[i] = down;
		if (down) held = i;
	}
	if (g_last >= 0 && g_down[g_last]) return g_last;
	if (held >= 0) return held;
	return g_last >= 0 && now - g_lastMs <= RecentMs ? g_last : -1;
}

GridJump::Grid ReadGrid(uintptr_t widget) {
	GridJump::Grid grid;
	const auto size = NativeUi::GridSize(widget);
	if (size.x < 1 || size.y < 1) return grid;
	grid.width = size.x;
	grid.height = size.y;
	std::vector<uintptr_t> units;
	for (int y = 0; y < size.y; ++y) {
		for (int x = 0; x < size.x; ++x) {
			const auto slot = NativeUi::GridItemAt(widget, {x, y});
			if (!slot.known || !slot.item) continue;
			const auto known = std::find(units.begin(), units.end(), slot.item);
			if (known == units.end()) {
				// Rows are read top first and cells left first, so an item is met at its top-left cell.
				units.push_back(slot.item);
				grid.items.push_back({x, y, 1, 1});
				continue;
			}
			auto& item = grid.items[static_cast<size_t>(known - units.begin())];
			item.width = std::max(item.width, x - item.x + 1);
			item.height = std::max(item.height, y - item.y + 1);
		}
	}
	return grid;
}

void Grid(const D2RL::PluginContext* context, NativeUi::Focus focus, int arrow) {
	const auto at = NativeUi::GridCell(focus.widget);
	if (at.x < 0) { g_grid = 0; return; }
	const GridJump::Cell cell{at.x, at.y};
	if (focus.widget != g_grid) {
		g_grid = focus.widget;
		g_cell = g_cursor = cell;
		return;
	}
	if (cell == g_cell) return;
	const auto accept = [&] { g_cell = g_cursor = cell; };
	// Not the keyboard's move, or an item is in hand: the cursor is where the game put it.
	if (arrow < 0 || InventoryCursor::HoldingItem(context).value_or(true)) { accept(); return; }
	const auto grid = ReadGrid(focus.widget);
	if (grid.width < 1 || g_cursor.x >= grid.width || g_cursor.y >= grid.height) { accept(); return; }
	const auto step = GridJump::Move(grid, g_cursor, static_cast<Direction>(arrow));
	if (!step.moved) { accept(); return; }
	if (step.anchor != cell) {
		const uint64_t selection = static_cast<uint32_t>(step.anchor.x) | (static_cast<uint64_t>(static_cast<uint32_t>(step.anchor.y)) << 32);
		if (!NativeSkillNavigation::SelectAt(focus, selection)) {
			AsyncLog::Warn(context, "[keys] The cursor could not be placed; the game's own move stands.");
			accept();
			return;
		}
		AsyncLog::InfoF(context, "[keys] grid %s: game %d,%d -> %d,%d", NativeWidgets::ReadName(focus.widget).c_str(),
			cell.x, cell.y, step.anchor.x, step.anchor.y);
	}
	g_cell = step.anchor;
	g_cursor = step.cursor;
}

void Skills(const D2RL::PluginContext* context, NativeUi::Focus focus, int arrow) {
	if (focus.widget == g_skill) return;
	const uintptr_t from = g_skill;
	g_skill = focus.widget;
	if (arrow < 0 || !from) return;
	const auto nodes = SkillNavigation::Nodes(focus.panel);
	const auto listed = [&nodes](uintptr_t widget) {
		return std::any_of(nodes.begin(), nodes.end(), [widget](const auto& node) { return node.widget == widget; });
	};
	if (!listed(from)) return;
	constexpr int Steps[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	const auto next = SkillGrid::Jump(nodes, from, Steps[arrow][0], Steps[arrow][1]);
	// No skill that way: the key leaves the skills, for the tabs or the buttons, as the game moved it.
	if (!next || next == from || next == focus.widget) return;
	if (!NativeSkillNavigation::Select({focus.panel, next})) {
		AsyncLog::Warn(context, "[keys] The skill could not be selected; the game's own move stands.");
		return;
	}
	AsyncLog::InfoF(context, "[keys] skill: game %s -> %s", NativeWidgets::ReadName(focus.widget).c_str(),
		NativeWidgets::ReadName(next).c_str());
	g_skill = next;
}
}

void Reset() {
	g_grid = g_skill = 0;
	g_last = -1;
}

void Tick(const D2RL::PluginContext* context) {
	const int arrow = Arrow(GetTickCount64());
	if (!NativeSkillNavigation::Available()) return;
	const auto focus = NativeUi::CurrentFocus();
	if (!focus.panel || !focus.widget) { g_grid = g_skill = 0; return; }
	if (NativeUi::Classify(focus.widget).kind == UiText::Kind::InventoryGrid) {
		g_skill = 0;
		Grid(context, focus, arrow);
		return;
	}
	g_grid = 0;
	if (NativeWidgets::ReadName(focus.panel) == "SkillsTreePanel") Skills(context, focus, arrow);
	else g_skill = 0;
}
}
