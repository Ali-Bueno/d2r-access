#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::UiText {

enum class Kind { Other, Text, LinkedText, NotificationList, Button, Toggle, ListItem, List, Focusable, ScrollingText,
	InventoryGrid, InventorySlot, InventoryItem, Tabs, StatButton, TabButton, ValueControl, Legend, QuestButton };
enum class Surface { Other, Menu, Narrative, Tip, Area, Notice, NotificationFeed, Target };

// A value-only snapshot. Native pointers are identities, never dereferenced here.
struct Node {
	uintptr_t id = 0;
	int parent = -1;
	Kind kind = Kind::Other;
	std::string name, type, text;
	bool enabled = true;
	uintptr_t contentId = 0; // optional native notification delivery, not a widget address
};
struct Snapshot {
	uintptr_t panel = 0, focus = 0;
	std::string name;
	Surface surface = Surface::Other;
	std::vector<Node> nodes; // visible nodes, in tree order; hidden subtrees omitted
};
struct Readout {
	std::string context, body, option, focusKey;
	Surface surface = Surface::Other;
	bool operator==(const Readout&) const = default;
	bool Empty() const { return body.empty() && option.empty(); }
};
struct Announcement {
	std::string text;
	bool interrupt = true;
};

std::string Clean(std::string_view text);
Readout Describe(const Snapshot& snapshot);

// Wait for a coherent snapshot after repopulation, then announce actual changes.
// New text settles for 80 ms by default. Existing menu-row navigation and
// descriptions already captured while rendering can be delivered immediately.
class Reader {
public:
	Announcement Update(const Readout& readout, uint64_t now, uint64_t settleMs = 80);
	void Reset();
	std::string Repeat() const;
private:
	Readout candidate_, spoken_;
	uint64_t candidateSince_ = 0;
	bool pending_ = false;
};
}
