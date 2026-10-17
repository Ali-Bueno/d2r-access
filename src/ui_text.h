#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::UiText {

enum class Kind { Other, Text, LinkedText, NotificationList, Button, Toggle, ListItem, List, Focusable, ScrollingText,
	InventoryGrid, InventorySlot, InventoryItem, Tabs, StatButton, TabButton, ValueControl, Legend, QuestButton, TextInput };
enum class Surface { Other, Menu, Narrative, Tip, Area, Notice, NotificationFeed, Target };

// A value-only snapshot. Native pointers are identities, never dereferenced here.
struct Node {
	uintptr_t id = 0;
	int parent = -1;
	Kind kind = Kind::Other;
	std::string name, type, text;
	bool enabled = true;
	uintptr_t contentId = 0; // optional native notification delivery, not a widget address
	std::optional<bool> toggled;
	std::string hint; // optional native tooltip, separate from a value control's caption
	std::string state; // what the native layer knows of the control beyond the tree: a waypoint row's "undiscovered"
};
struct Snapshot {
	uintptr_t panel = 0, focus = 0;
	std::string name;
	Surface surface = Surface::Other;
	std::vector<Node> nodes; // visible nodes, in tree order; hidden subtrees omitted
};
struct Readout {
	std::string context, body, option, focusKey;
	// Grid position (row/column, branch position). Spoken after everything
	// else: it is the least relevant part of a cell's announcement.
	std::string coordinates;
	Surface surface = Surface::Other;
	uint64_t contentSettleMs = 0; // dependent detail panes populate after focus moves
	std::string help; // the game's action footer: a control hint, never part of the spoken phrase
	bool operator==(const Readout&) const = default;
	bool Empty() const { return body.empty() && option.empty() && coordinates.empty(); }
};
struct Announcement {
	std::string text;
	bool interrupt = true;
	bool sameFocus = false; // only the content under an unchanged focus changed
};

// U+E07E opens the game's inline font command; one index character, '0'..'V', follows it.
inline constexpr unsigned FontCommand = 0xE07E;
constexpr bool FontIndex(char c) { return static_cast<unsigned>(static_cast<unsigned char>(c) - '0') < 0x27u; }
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
