#include "menu_navigation_policy.h"
#include "test_support.h"

#include <array>
#include <vector>

int main() {
	using namespace D2RAccess::MenuNavigationPolicy;
	const std::vector<Node> host{
		{1, 20, 20, 280, 32}, // name
		{2, 20, 70, 280, 32}, // password
		{3, 20, 120, 280, 32}, // description
		{4, 20, 170, 280, 32}, // difficulty
		{5, 20, 220, 120, 32}, // unlisted
		{6, 160, 224, 120, 32}, // LAN only, slightly staggered
		{7, 160, 275, 120, 32}, // another right-column control
	};
	Check(Beside(host, 5, 1) == 6 && Beside(host, 6, -1) == 5,
		"checkbox row navigates horizontally despite staggered rectangles");
	Check(Beside(host, 1, 1) == 1 && Beside(host, 6, 1) == 6 && Beside(host, 5, -1) == 5,
		"nothing beside on the row: the selection stays, no other row is taken");
	Check(Beside(host, 0, 1) == 1 && Beside(host, 0, -1) == 7, "from no control, the first or the last");
	Check(Beside(host, 1, 0) == 0, "no direction, no target");
	const std::vector<Node> sparse{{8, 20, 20, 40, 30}, {9, 180, 25, 40, 30},
		{10, 20, 60, 40, 30}, {11, 180, 65, 40, 30}, {14, 100, 95, 40, 10}};
	Check(Beside(sparse, 8, 1) == 9 && Beside(sparse, 10, 1) == 11, "the row's nearest control");
	Check(Beside(sparse, 14, 1) == 14, "a row only touching another is not part of it");
	const std::array<Node, 2> partlyInvalid{{{12, 0, 0, 0, 20}, {13, 10, 10, 20, 20}}};
	Check(Beside(partlyInvalid, 13, -1) == 13 && Beside(partlyInvalid, 0, 1) == 13,
		"invalid rectangles cannot become navigation targets");
	Check(Beside({}, 0, 1) == 0, "empty control set has no target");

	// Up and down follow the form's own order, not the nearest rectangle (the user, 2026-10-17).
	Check(Step(host, 1, 1) == 2 && Step(host, 5, 1) == 6 && Step(host, 6, -1) == 5, "the next and previous control in order");
	Check(Step(host, 7, 1) == 1 && Step(host, 1, -1) == 7, "the ends wrap");
	Check(Step(host, 0, 1) == 1 && Step(host, 99, -1) == 7, "from no control, the first or the last");
	Check(Step(partlyInvalid, 13, 1) == 13 && Step(partlyInvalid, 0, -1) == 13, "invalid rectangles are skipped");
	Check(Step({}, 0, 1) == 0 && Step(host, 1, 0) == 0, "no controls or no direction, no target");
}
