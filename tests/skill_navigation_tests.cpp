#include "skill_grid.h"
#include <set>
#include "skill_navigation.h"
#include "native_skill_navigation.h"
#include "native_widgets.h"
#include "input_direction.h"
#include "test_support.h"
#include <algorithm>
#include <map>

namespace {
using namespace D2RAccess;
struct Widget { uintptr_t parent; std::string name; bool visible = true; };
std::map<uintptr_t, Widget> widgets;
NativeUi::Focus focus;
InputDirection::DpadInput input;
bool available = true, selectWorks = true, held = false;
int selections = 0;
}
namespace D2RAccess::InputDirection { DpadInput Dpad() { return input; } }
namespace D2RAccess::NativeSkillNavigation {
bool Available() { return available; }
void HoldCursor(bool hold) { held = hold; }
bool Select(NativeUi::Focus next) { ++selections; if (selectWorks) focus = next; return selectWorks; }
NativeUi::Cell Position(uintptr_t widget) {
	if (widget == 16) return {100, 100};
	if (widget == 17) return {200, 100};
	if (widget == 18) return {200, 300};
	return {};
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) { return name == "SkillsTreePanel" ? 1 : 0; }
std::string ReadName(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).name : ""; }
}
namespace D2RAccess::NativeUi {
Focus CurrentFocus() { return focus; }
uintptr_t Parent(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).parent : 0; }
bool Visible(uintptr_t widget) {
	for (size_t depth = 0; widget && depth < widgets.size(); ++depth) {
		if (!widgets.contains(widget) || !widgets.at(widget).visible) return false;
		widget = widgets.at(widget).parent;
		if (!widget) return true;
	}
	return false;
}
std::vector<uintptr_t> FindControls(uintptr_t, UiText::Kind) { return {10, 11, 12, 13, 14, 15, 16, 17, 18}; }
}

int main() {
	using namespace D2RAccess;
	// All three Barbarian branches read from the user's live tree, 2026-09-16.
	// Learned state must not remove the lower-level-locked skills from navigation.
	const std::vector<std::vector<std::pair<int, int>>> branches{
		{{0,0},{2,0},{0,1},{1,1},{2,2},{0,3},{1,4},{2,4},{0,5},{1,5}},
		{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{2,3},{0,4},{2,5}},
		{{1,0},{0,1},{2,1},{1,2},{2,2},{0,3},{1,3},{2,4},{0,5},{1,5}}
	};
	constexpr std::pair<int, int> cardinal[]{{1,0},{-1,0},{0,1},{0,-1}};
	for (const auto& branch : branches) {
		std::vector<SkillGrid::Node> cells;
		for (const auto& [x,y] : branch) cells.push_back({cells.size() + 1, x, y});
		for (const auto& start : cells) {
			std::set<uintptr_t> visited{start.widget};
			std::vector<uintptr_t> pending{start.widget};
			while (!pending.empty()) {
				const auto from = pending.back(); pending.pop_back();
				for (const auto& [x,y] : cardinal) {
					const auto next = SkillGrid::Move(cells, from, x, y);
					if (visited.insert(next).second) pending.push_back(next);
				}
			}
			Check(visited.size() == cells.size(), "every skill in the recorded branch is reachable from every starting skill");
		}
	}
	const std::vector<SkillGrid::Node> nodes{{4, 2, 5}, {2, 2, 0}, {3, 0, 2}, {1, 0, 0}, {5, 1, 2}};
	Check(SkillGrid::Move(nodes, 0, 0, 1) == 1, "entry uses on-screen order, not discovery order");
	Check(SkillGrid::Move(nodes, 1, 1, 0) == 2, "horizontal navigation skips an empty cell");
	Check(SkillGrid::Move(nodes, 2, -1, 0) == 1, "horizontal movement is reversible in a row");
	Check(SkillGrid::Move(nodes, 1, 0, 1) == 3, "vertical navigation stays in its column and skips a row");
	Check(SkillGrid::Move(nodes, 3, 0, -1) == 1, "up returns to the same column");
	Check(SkillGrid::Move(nodes, 3, 1, 0) == 5, "closest skill in the same row comes first");
	Check(SkillGrid::Move(nodes, 2, 0, 1) == 4, "aligned destination is preferred over a nearer diagonal");
	Check(SkillGrid::Move(nodes, 5, 0, 1) == 4, "a sparse tree remains traversable when the column ends");
	Check(SkillGrid::Move(nodes, 1, 0, -1) == 1, "top edge does not wrap");
	Check(SkillGrid::Move(nodes, 4, 0, 1) == 4, "bottom edge does not wrap");
	Check(SkillGrid::Move(nodes, 2, 1, 0) == 2, "right edge does not wrap");
	Check(SkillGrid::Move(nodes, 1, -1, 0) == 1, "left edge does not wrap");
	Check(SkillGrid::Move({}, 1, 0, 1) == 0, "an empty branch has no selection");
	Check(SkillGrid::Move(nodes, 1, 1, 1) == 0, "diagonal presses do not choose an arbitrary skill");
	Check(SkillGrid::Move(nodes, 1, 0, 0) == 0, "no direction does not move focus");
	widgets = {
		{1, {0, "SkillsTreePanel"}}, {2, {1, "SkillButtonContainer"}},
		{3, {1, "SkillButtonContainer", false}}, {4, {1, "OtherButtons"}},
		{5, {1, "CommonSkillsContainer", false}}, {6, {1, "ItemSkillsContainer", false}},
		{10, {2, "SkillButton3 0,0"}}, {11, {2, "SkillButton3 2,0"}},
		{12, {2, "SkillButton3 0,2"}}, {13, {3, "SkillButton1 1,0"}},
		{14, {4, "SkillButton3 1,0"}}, {15, {2, "Unrecognized"}},
		{16, {5, "SkillButton3"}}, {17, {5, "SkillButton4"}}, {18, {6, "SkillButton5"}},
	};
	focus = {1, 10};
	SkillNavigation::Tick(nullptr);
	Check(selections == 0 && !held, "opening the tree leaves free cursor input available");
	input = {1, 0, true, true};
	SkillNavigation::Tick(nullptr);
	Check(focus.widget == 11 && selections == 1 && held, "one D-pad press selects the next real visible skill");
	Check(focus.widget != 13 && focus.widget != 14, "hidden branches and unrelated buttons are excluded");
	input.pressed = false;
	for (int i = 0; i < 30; ++i) SkillNavigation::Tick(nullptr);
	Check(selections == 1 && held, "holding the D-pad freezes analog cursor travel without repeating selection");
	input = {0, 0, true, true};
	SkillNavigation::Tick(nullptr);
	input = {0, 1, true, true};
	SkillNavigation::Tick(nullptr);
	Check(selections == 1 && focus.widget == 11, "rocking through a diagonal without releasing cannot cause a second selection");
	input = {-1, 0, true, true};
	SkillNavigation::Tick(nullptr);
	Check(selections == 1 && held, "a direction change in the same held gesture does not repeat navigation");
	input = {};
	SkillNavigation::Tick(nullptr);
	Check(!held && selections == 1, "release restores the stock cursor update");
	focus.widget = 12; // a subsequent analog cursor movement
	SkillNavigation::Tick(nullptr);
	input = {0, -1, true, true};
	SkillNavigation::Tick(nullptr);
	Check(focus.widget == 10, "D-pad resumes from the skill reached with the analog cursor");
	Check(selections == 2, "a deliberate new press after full release remains responsive without a debounce timer");
	input = {}; SkillNavigation::Tick(nullptr);
	focus.widget = 11; // stock input moved before the sampled edge
	input = {0, 1, true, true};
	SkillNavigation::Tick(nullptr);
	Check(focus.widget == 12, "an edge moves from the preceding selection, avoiding double movement");
	widgets[1].visible = false;
	SkillNavigation::Tick(nullptr);
	Check(!held, "closing the tree releases the cursor during a held press");
	widgets[1].visible = true; focus = {99, 0};
	const auto beforeOtherPanel = selections;
	SkillNavigation::Tick(nullptr);
	Check(selections == beforeOtherPanel && !held, "a covering menu retains all controller input");
	focus = {1, 0};
	SkillNavigation::Tick(nullptr);
	Check(focus.widget == 10 && held, "a cursor between skills can enter the grid");
	SkillNavigation::SetEnabled(false);
	SkillNavigation::Tick(nullptr);
	Check(!held, "turning off grid navigation immediately releases its gesture");
	SkillNavigation::SetEnabled(true); available = false;
	const auto beforeUnavailable = selections;
	SkillNavigation::Tick(nullptr);
	Check(selections == beforeUnavailable && !held, "failed native guards leave stock navigation available");
	available = true; selectWorks = false;
	SkillNavigation::Tick(nullptr);
	Check(!held, "failed native selection cannot trap the cursor");
	selectWorks = true; SkillNavigation::Reset();
	input = {0, 0, true, true};
	const auto beforeDiagonal = selections;
	SkillNavigation::Tick(nullptr);
	Check(selections == beforeDiagonal && !held, "an initial diagonal gesture does not capture input");
	widgets[2].visible = false; widgets[5].visible = widgets[6].visible = true;
	focus.widget = 0; input = {0, 1, true, true};
	SkillNavigation::Tick(nullptr);
	Check(focus.widget == 16 && held, "general skills enter in native screen order without encoded cells");
	input = {}; SkillNavigation::Tick(nullptr);
	input = {1, 0, true, true}; SkillNavigation::Tick(nullptr);
	Check(focus.widget == 17, "general skill buttons move by their native positions");
	input = {}; SkillNavigation::Tick(nullptr);
	input = {0, 1, true, true}; SkillNavigation::Tick(nullptr);
	Check(focus.widget == 18, "the grid reaches item-granted skills below common skills");
	SkillNavigation::Reset();
	Passed();
}
