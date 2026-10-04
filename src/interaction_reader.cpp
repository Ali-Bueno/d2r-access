#include "advanced_stats_reader.h"
#include "async_log.h"
#include "auto_detail.h"
#include "click_words.h"
#include "control_hints.h"
#include "character_stats.h"
#include "character_stats_reader.h"
#include "entity_radar_service.h"
#include "hire_stats.h"
#include "generated/item_sizes.h"
#include "equip_attempt.h"
#include "interaction_reader.h"
#include "inventory_census.h"
#include "inventory_cursor.h"
#include "item_hand.h"
#include "native_entities.h"
#include "native_belt.h"
#include "ground_item_service.h"
#include "ground_item_text.h"
#include "input_direction.h"
#include "localization.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_ground_items.h"
#include "native_progression.h"
#include "progression_text.h"
#include "radar_ui.h"
#include "native_widgets.h"
#include "pad_filter.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tab_reader.h"
#include "tooltip_delay.h"
#include "tooltip_text.h"
#include "native_skill_icons.h"
#include "mercenary_stats.h"
#include "ui_text.h"

#include <D2RLPlugin/item.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace D2RAccess::InteractionReader {
namespace {
UiText::Reader g_reader;
bool g_inventory = false;
constexpr uint64_t InventorySettleMs = 20; // at most one subsequent 30 Hz UI poll
struct InventoryContext {
	uintptr_t panel = 0;
	std::string name, title;
};
InventoryContext g_inventoryContext;
struct StatValue { uintptr_t widget = 0; std::string value, points; };
StatValue g_stat;
std::string g_help, g_spokenHelp, g_helpContext;
// The focus the last real readout described, so a hint can tell "nothing new
// to say here" from "nothing said about here yet".
std::string g_describedFocus;
std::string g_skillHeadline, g_skillMismatch;
// The world label last dropped for being out of sight, so the log records each
// one once instead of on every UI poll.
std::string g_hiddenLabel;
// Whether an item was in hand at the last inventory tick, and the red
// requirement lines of the item last described and of the one in hand (taken
// as it was picked up): why the game would refuse to equip it.
std::optional<bool> g_wasHolding;
std::string g_itemUnmet, g_heldUnmet;
EquipAttempt::Tracker g_equipAttempt;
uint32_t g_heldId = 0;
bool g_heldFiller = false;
struct Hovered { ItemHand::Under item; std::string name; bool read = false; };
Hovered g_hovered;
struct SocketRead { uint32_t id = 0; std::string state; };
SocketRead g_socketRead;
ItemHand::Names g_itemNames;
// An equipment slot's location never changes, but the native read misses it
// while the game rebuilds the panel, and the label would then simply vanish.
// Widget names are stable across those rebuilds, so the last value seen under
// a name stands in for a failed read.
struct KnownSlot { std::string name; int location = 0; bool reported = false; };
std::vector<KnownSlot> g_slotLocations;

// The part of a description the player asked not to hear on every move: an
// item's requirements, properties, flavour text and price. Held for whatever
// the highlight is on now, and read on demand with a push up on the right
// stick while a panel is open.
std::string g_detail, g_detailFor;

// What a push right on the right stick reads for the highlighted thing, apart
// from its detail like the button hints: an item's size (the user, 2026-09-21:
// "put size to be read separately on right stick right. it should not read at
// all on right stick up") and a skill's choices left, the way the character
// panel gives its stat points there. Offered only while an item or a skill has
// focus; g_asideWidget is 0 otherwise, and right stick right is left alone.
std::string g_aside;
uintptr_t g_asideWidget = 0;

// When a tab last changed. The first readout after it queues behind the title
// the window announcer is still speaking instead of cutting it off -- the skill
// tree's first skill arrived about 350 ms into "Skill Tree" and interrupted it
// (MEASURED 2026-09-21 13:03:30). Later readouts interrupt as usual.
uint64_t g_tabChangedAt = 0;
constexpr uint64_t TabTitleMs = 1500;
// The title that tab change spoke, so the readout queued behind it does not
// begin by saying it again ("Inventory", then "Inventory. Belt...").
std::string g_tabTitle;
// The panel the last readout was about, to tell a tab change that moved focus
// to another panel from one that left it where it was.
uintptr_t g_readPanel = 0;
// The tab bar highlight last logged, so [tabs] bar is written once per move.
std::string g_tabBarLogged;

// Right stick left on a skill opens its description one line at a time (the
// user, 2026-09-21): up and down move through the lines, and right stick left
// again or B closes it, after which the skill is read again. B is kept from the
// game meanwhile (PadFilter::HoldB), so closing does not close the skill tree;
// moving the cursor to another skill closes it too. No line is given a number.
struct LineReader { uintptr_t widget = 0; std::vector<std::string> lines; size_t at = 0; };
LineReader g_lineReader;
// The focused skill's lines, offered as its tooltip renders.
std::vector<std::string> g_skillLines;
uintptr_t g_skillLinesWidget = 0;

// Ctrl with an arrow key on an item or a skill (the user, 2026-09-27): its
// description a line at a time, up and down a line, left and right to the
// first and the last. The lines are the tooltip's own, in the order the screen
// draws them, the name first; an item's size comes last. Held for whatever the
// cursor is on now, by its widget and its place in it. No line is given a number.
// Both keys read the tooltip as the game draws it at the moment of the key:
// with Ctrl held that is its ranges, "+(1-3) to Sacrifice" (MEASURED
// 2026-09-29), with Shift held the regular tooltip, "+2 to Sacrifice" (the
// user, 2026-09-29 and 2026-09-30: the game shows ranges only with Ctrl). A
// copy kept from earlier once read a jewel's ranges under Shift (2026-09-30),
// so none is kept. Shift's follow the compare readout: after the equipped item
// is read they are its lines (compare), after the highlighted one again they
// are the highlighted item's once more.
struct KeyLines { std::string owner; std::vector<std::string> lines; size_t at = 0; std::vector<std::string> compare; };
KeyLines g_keyLines;

// Item tooltips in the log: off unless asked for, and each only once.
std::atomic<bool> g_logItems{false};
std::string g_itemLogged;

// The equipped-item records last logged, so [compare] is written once per
// change. Diagnosis only: what the game draws beside a held compare is not
// yet measured.
std::string g_compareLogged;

// The compare record read last (0 the highlighted item, then each equipped
// record the game draws), and the item it belongs to.
std::string g_compareOwner;
size_t g_compareAt = 0;

// The belt slot last logged, so [belt] is written once per move, not per poll.
int g_beltSlot = -2;

// The game's own labels for a set of string keys, as the part of each printf
// format before its first placeholder -- the only part that can begin a
// rendered line. Resolved again whenever the text language changes: the main
// menu switches it without restarting the game.
class Labels {
public:
	template <typename Keys> const std::vector<std::string>& For(const Keys& keys) {
		if (language_ != Strings::Selected()) {
			language_ = Strings::Selected();
			prefixes_.clear();
			for (const char* key : keys)
				if (auto text = TooltipText::LabelPrefix(Localization::Text(key)); !text.empty()) prefixes_.push_back(std::move(text));
		}
		return prefixes_;
	}

private:
	std::optional<Strings::Language> language_;
	std::vector<std::string> prefixes_;
};

// A weapon's damage and an armour's defense, so the split can tell them from the name.
const std::vector<std::string>& StatLabels() {
	static Labels cache;
	return cache.For(TooltipText::BaseStatKeys);
}

// Requirement labels, whose red lines say what the character cannot meet.
const std::vector<std::string>& RequirementLabels() {
	static Labels cache;
	return cache.For(TooltipText::RequirementKeys);
}

// A tooltip body with any magic name the game composed with a broken grammar
// header rebuilt, the format resolved again when the text language changes.
TooltipText::Body ItemBody(std::string_view raw) {
	static std::optional<Strings::Language> language;
	static std::string format;
	if (language != Strings::Selected() || format.empty()) {
		language = Strings::Selected();
		format = Localization::Text(GroundItemText::SuffixOnlyFormatKey);
	}
	auto body = TooltipText::Read(raw);
	// A line that names a click names the keyboard's key for it (click_words.h).
	for (auto& line : body.lines) line = ClickWords::Spoken(GroundItemText::RepairSuffixOnlyName(line, format));
	return body;
}

std::string Refusal(const std::string& reasons) {
	if (reasons.empty()) return Strings::Current().equipRefused;
	char phrase[300]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().equipRefusedBecause, reasons.c_str());
	return phrase;
}

// What a vendor charges, and what it would pay for the player's own item: in a
// shop the name and this are the headline.
const std::vector<std::string>& PriceLabels() {
	static constexpr const char* Keys[] = {TooltipText::PriceKey, TooltipText::SellKey};
	static Labels cache;
	return cache.For(Keys);
}

// A push up on the right stick, claimed only while a panel is open so the
// level scanner keeps the stick during play.
bool ReadDetail(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	if (!focus.panel || !InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Up, "detail")) return false;
	const std::string& text = g_detail.empty() ? Strings::Current().detailNone : g_detail;
	D2RAccess::AsyncLog::InfoF(context, "[detail] %s -> \"%s\"", g_detailFor.c_str(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

// A push down on the right stick while a menu is up: the controls the
// highlighted thing offers, which are only spoken unasked with autodetail and
// control tooltips both on (control_hints.h). In play the stick belongs to the
// scan, the pathfinder and the exit finder.
bool ReadHints(const D2RL::PluginContext* context) {
	if (InputDirection::PeekRightStickPush() != InputDirection::StickPush::Down || RadarUi::InPlay()) return false;
	(void)InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Down, "hints");
	const std::string& text = ControlHints::Current().empty() ? Strings::Current().detailNone : ControlHints::Current();
	D2RAccess::AsyncLog::InfoF(context, "[hints] asked -> \"%s\"", text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

// A push right on the right stick while an item or a skill has focus. Only
// then: in play the stick belongs to the scan, the pathfinder and the exit
// finder, and the character panel's own reader has claimed it before this.
bool ReadAside(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	if (!focus.panel || !g_asideWidget || focus.widget != g_asideWidget
		|| !InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Right, "aside")) return false;
	const std::string& text = g_aside.empty() ? Strings::Current().detailNone : g_aside;
	D2RAccess::AsyncLog::InfoF(context, "[aside] -> \"%s\"", text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

void OfferAside(uintptr_t widget, std::string text) {
	g_asideWidget = widget;
	g_aside = std::move(text);
}

std::string LinesOwner(uintptr_t widget, uint64_t selection) {
	return std::to_string(widget) + ":" + std::to_string(selection);
}

void OfferLines(const std::string& owner, std::vector<std::string> lines) {
	if (owner != g_keyLines.owner) g_keyLines = {owner, {}, 0};
	if (lines.empty() || lines == g_keyLines.lines) return;
	g_keyLines.lines = std::move(lines);
	if (g_keyLines.at >= g_keyLines.lines.size()) g_keyLines.at = 0;
}

bool ReadLines(const D2RL::PluginContext* context, NativeUi::Focus focus) {
	using InputDirection::StickPush;
	if (!InputDirection::LineStepPending() || !focus.panel || g_keyLines.lines.empty()
		|| g_keyLines.owner != LinesOwner(focus.widget, NativeUi::FocusSelection(focus.widget))) return false;
	const auto push = InputDirection::PeekRightStickPush();
	if (!InputDirection::TakeRightStickPushIf(push, "line")) return false;
	// Shift's lines: the equipped item's after the compare readout read it, else
	// the tooltip as drawn now, as Ctrl's are.
	const bool shift = InputDirection::LineStepPlain();
	const bool compare = shift && !g_keyLines.compare.empty();
	const auto& lines = compare ? g_keyLines.compare : g_keyLines.lines;
	const size_t last = lines.size() - 1;
	if (g_keyLines.at > last) g_keyLines.at = last;
	// At either end the step reads the same line again rather than nothing.
	if (push == StickPush::Up && g_keyLines.at > 0) --g_keyLines.at;
	else if (push == StickPush::Down && g_keyLines.at < last) ++g_keyLines.at;
	else if (push == StickPush::Left) g_keyLines.at = 0;
	else if (push == StickPush::Right) g_keyLines.at = last;
	const auto& line = lines[g_keyLines.at];
	AsyncLog::InfoF(context, "[lines] key %zu%s -> \"%s\"", g_keyLines.at,
		compare ? " equipped" : shift ? " shift" : "", line.c_str());
	ScreenReader::Speak(line, true);
	return true;
}

bool ReadSkillLines(const D2RL::PluginContext* context, NativeUi::Focus focus, uint64_t now) {
	using InputDirection::StickPush;
	if (g_lineReader.widget) {
		if (focus.widget != g_lineReader.widget) {
			g_lineReader = {};
			PadFilter::ReleaseB();
			AsyncLog::Info(context, "[lines] closed: the cursor moved");
			return false;
		}
		PadFilter::HoldB(now);
		if (InputDirection::TakeFacePress(InputDirection::FaceButton::B)
			|| InputDirection::TakeRightStickPushIf(StickPush::Left, "lines")) {
			g_lineReader = {};
			PadFilter::ReleaseB();
			AsyncLog::Info(context, "[lines] closed");
			if (const auto again = Repeat(); !again.empty()) ScreenReader::Speak(again, true);
			return true;
		}
		const bool up = InputDirection::TakeRightStickPushIf(StickPush::Up, "lines");
		const bool down = !up && InputDirection::TakeRightStickPushIf(StickPush::Down, "lines");
		// Right has nothing to do here, and is not left pending for after.
		(void)InputDirection::TakeRightStickPushIf(StickPush::Right, "lines");
		// At either end the push reads the same line again rather than nothing.
		if (up && g_lineReader.at > 0) --g_lineReader.at;
		if (down && g_lineReader.at + 1 < g_lineReader.lines.size()) ++g_lineReader.at;
		if (up || down) {
			const auto& line = g_lineReader.lines[g_lineReader.at];
			AsyncLog::InfoF(context, "[lines] %zu -> \"%s\"", g_lineReader.at, line.c_str());
			ScreenReader::Speak(line, true);
		}
		return true;
	}
	if (!focus.panel || !g_skillLinesWidget || focus.widget != g_skillLinesWidget
		|| !InputDirection::TakeRightStickPushIf(StickPush::Left, "lines")) return false;
	if (g_skillLines.empty()) { ScreenReader::Speak(Strings::Current().detailNone, true); return true; }
	g_lineReader = {focus.widget, g_skillLines, 0};
	(void)InputDirection::TakeFacePress(InputDirection::FaceButton::B); // an old press is not a new one
	PadFilter::HoldB(now);
	AsyncLog::InfoF(context, "[lines] open, %zu lines -> \"%s\"", g_skillLines.size(), g_skillLines.front().c_str());
	ScreenReader::Speak(g_skillLines.front(), true);
	return true;
}

bool IsInventory(UiText::Kind kind);

// Tooltip records' lines joined as TooltipText::Comparison joins them, with a
// suffix-only magic name repaired the way the item readout repairs it.
std::string CompareText(std::span<const std::string> records) {
	std::string result;
	for (const auto& record : records)
		for (const auto& line : ItemBody(record).lines) {
			if (line.empty()) continue;
			if (!result.empty()) result += result.ends_with('.') ? " " : ". ";
			result += line;
		}
	return result;
}

// The same records' lines, one entry each, for the line keys.
std::vector<std::string> CompareLines(std::span<const std::string> records) {
	std::vector<std::string> lines;
	for (const auto& record : records)
		for (auto& line : ItemBody(record).lines)
			if (!line.empty()) lines.push_back(std::move(line));
	return lines;
}

// Compare mode, while Y is held on an inventory item (the user, 2026-09-22):
// the left stick reads, since Y and the right stick share the right thumb.
// Left reads the highlighted item's tooltip in full, right the equipped one the
// game draws beside it ("Currently Equipped"), up the detail and down the
// controls, as the right stick would. The left stick is kept from the game
// meanwhile (PadFilter::ArmLeftStickWithY) so it does not move the cursor off
// the item. Holding Y reads nothing by itself; the right stick is unchanged.
bool ReadCompare(const D2RL::PluginContext* context, NativeUi::Focus focus, uint64_t now) {
	using InputDirection::StickPush;
	if (!focus.panel || !IsInventory(NativeUi::Classify(focus.widget).kind)) return false;
	PadFilter::ArmLeftStickWithY(now);
	const auto push = InputDirection::PeekLeftStickPush();
	if (push == StickPush::None || !InputDirection::TakeLeftStickPushIf(push, "compare")) return false;
	const auto tooltip = NativeTooltips::Latest(now);
	const bool current = tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& tooltip.selection == NativeUi::FocusSelection(focus.widget);
	std::string text;
	const char* what = "detail";
	if (push == StickPush::Up) text = g_detail;
	else if (push == StickPush::Down) { text = ControlHints::Current(); what = "controls"; }
	else if (current) {
		// Each record the game draws is read on its own, the highlighted item
		// first: right the next, left the previous, stopping at either end (the
		// user, 2026-09-29). Joined, a shield compared read the weapon first.
		std::vector<const std::string*> records{&tooltip.body};
		for (const auto& extra : tooltip.extras)
			if (!CompareText(std::span(&extra, 1)).empty()) records.push_back(&extra);
		const auto owner = LinesOwner(focus.widget, NativeUi::FocusSelection(focus.widget));
		if (g_compareOwner != owner) { g_compareOwner = owner; g_compareAt = 0; }
		g_compareAt = (std::min)(g_compareAt, records.size() - 1);
		if (push == StickPush::Right && g_compareAt + 1 < records.size()) ++g_compareAt;
		else if (push == StickPush::Left && g_compareAt > 0) --g_compareAt;
		D2RAccess::AsyncLog::InfoF(context, "[compare] record %zu of %zu", g_compareAt, records.size());
		if (push == StickPush::Right && records.size() == 1) what = "equipped"; // nothing to compare against
		else {
			text = CompareText(std::span(records[g_compareAt], 1));
			what = g_compareAt == 0 ? "highlighted" : "equipped";
		}
		// Shift's lines become the item just read (the user, 2026-09-29).
		if (g_keyLines.owner == owner && (push == StickPush::Left || records.size() > 1)) {
			g_keyLines.compare = g_compareAt == 0 ? std::vector<std::string>{} : CompareLines(std::span(records[g_compareAt], 1));
			g_keyLines.at = 0;
		}
	}
	if (text.empty()) text = Strings::Current().detailNone;
	D2RAccess::AsyncLog::InfoF(context, "[compare] %s -> \"%s\"", what, text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

int SlotLocation(const D2RL::PluginContext* context, uintptr_t widget) {
	const int live = NativeUi::EquipmentLocation(widget);
	const auto name = NativeWidgets::ReadName(widget);
	if (name.empty()) return live;
	for (auto& known : g_slotLocations) {
		if (known.name != name) continue;
		if (live > 0) { known.location = live; known.reported = false; return live; }
		if (!known.reported) {
			known.reported = true;
			AsyncLog::InfoF(context, "[interaction] equipment location unreadable for %s; keeping %d",
				name.c_str(), known.location);
		}
		return known.location;
	}
	if (live > 0) g_slotLocations.push_back({name, live, false});
	return live;
}

// The label of the equipment slot a location belongs to, null when unknown.
const char* SlotName(int location) {
	const auto& labels = Strings::Current().equipmentSlots;
	return location > 0 && static_cast<size_t>(location) < labels.size() ? labels[location] : nullptr;
}

void Append(std::string& text, const std::string& part) {
	if (part.empty() || text == part) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

// The cells an item takes up -- "1 column by 3 rows" -- which decide where it
// can go: the game will not take an item no free block of its shape can hold,
// however many single cells are free (the user, 2026-09-21, on hearing a short
// sword named on three cells: "that is important information to have").
// Empty for an item the tables do not know.
std::string SizePhrase(uintptr_t item) {
	if (!item) return {};
	const auto size = Generated::ItemSizeFor(NativeGroundItems::ItemTableCode(item));
	if (!size.width || !size.height) return {};
	const char* format = Strings::Current().itemSize[(size.width > 1 ? 2 : 0) + (size.height > 1 ? 1 : 0)];
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), format, size.width, size.height);
	return phrase;
}

// The inventory's gold row, named by the game's own widget name.
constexpr std::string_view GoldButton = "gold_button";
// The stash's withdraw button and the text box beside it that shows the open tab's gold.
constexpr std::string_view StashGoldButton = "gold_withdraw";
constexpr std::string_view StashGoldBox = "gold_amount", StashGoldContainer = "basicstash_container";

// The digits of the stash's gold box, "" when it cannot be read.
std::string StashGold(const D2RL::PluginContext* context, uintptr_t panel) {
	uintptr_t box = NativeWidgets::FindChild(panel, StashGoldBox);
	if (!box) box = NativeWidgets::FindChild(NativeWidgets::FindChild(panel, StashGoldContainer), StashGoldBox);
	const std::string text = UiText::Clean(NativeWidgets::ReadText(box));
	std::string digits;
	for (const char c : text)
		if (c >= '0' && c <= '9') digits += c;
	static std::string logged;
	if (text != logged) {
		logged = text;
		AsyncLog::InfoF(context, "[stash] gold box=%d text=\"%s\" stat=%d", box ? 1 : 0, text.c_str(), NativeGroundItems::PlayerStashGold());
	}
	return digits;
}

bool IsInventory(UiText::Kind kind) {
	return kind == UiText::Kind::InventoryGrid || kind == UiText::Kind::InventorySlot || kind == UiText::Kind::InventoryItem;
}

// The trade window's two grids and the partner's name box (TradeLayoutHD.json).
constexpr std::string_view TradeGridUs = "grid_us", TradeGridThem = "grid_them", TradeNameThem = "name_them";

std::string TradePartner(uintptr_t panel) {
	return UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(panel, TradeNameThem)));
}

std::string Formatted(const char* format, const std::string& value) {
	char phrase[160]{};
	std::snprintf(phrase, sizeof(phrase), format, value.c_str());
	return phrase;
}

std::string Title(uintptr_t panel) {
	if (NativeUi::Classify(panel).name == "TradePanel") {
		const auto partner = TradePartner(panel);
		return partner.empty() ? Localization::Text("NPCMenuTrade") : Formatted(Strings::Current().tradeWith, partner);
	}
	const bool stash = NativeUi::Classify(panel).name == "BankPanel";
	const bool hireling = NativeWidgets::ReadName(panel) == "HirelingInventoryPanel";
	const auto localized = Localization::Text(stash ? "stash" : hireling ? "MiniPanelHireinv" : "minipanelinv");
	return localized.empty() ? (stash ? Strings::Current().stash : Strings::Current().inventory) : localized;
}

UiText::Readout Inventory(const D2RL::PluginContext* context, NativeUi::Focus focus, const NativeUi::Type& type) {
	if (g_inventoryContext.panel != focus.panel) {
		g_inventoryContext = {focus.panel, NativeWidgets::ReadName(focus.panel), Title(focus.panel)};
	}
	UiText::Readout result;
	result.context = g_inventoryContext.name;
	result.body = g_inventoryContext.title;
	Append(result.body, TabReader::Selected(focus.panel));
	if (type.kind == UiText::Kind::InventorySlot) Append(result.body, Strings::Current().equipment);
	else if (type.name == "BeltWidget") Append(result.body, Strings::Current().belt);
	else if (const auto grid = NativeWidgets::ReadName(focus.widget); grid == TradeGridUs) Append(result.body, Strings::Current().tradeYourOffer);
	else if (grid == TradeGridThem) {
		const auto partner = TradePartner(focus.panel);
		Append(result.body, Formatted(Strings::Current().tradeTheirOffer, partner.empty() ? Strings::Current().tradePartnerUnknown : partner));
	}
	result.focusKey = std::to_string(focus.widget) + ":" + std::to_string(NativeUi::FocusSelection(focus.widget));
	const auto cell = NativeUi::GridCell(focus.widget);
	if (cell.x >= 0) {
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
		result.coordinates = phrase;
	}
	// The section in the body already says "equipment"; only a real slot name
	// is worth adding here. A slot with nothing else to say is covered in Tick.
	if (type.kind == UiText::Kind::InventorySlot)
		if (const char* slot = SlotName(SlotLocation(context, focus.widget))) Append(result.option, slot);
	return result;
}

// The game goes on drawing the tooltip of the place the pointer left for a
// while after it moves (MEASURED 2026-09-28: the helmet's, 200 ms after the
// cursor came to the torso, read as "Torso. Skull Cap"). An item's tooltip is
// taken for the item under the cursor once it names that item; one that never
// does is taken after NamesItemMs all the same, so a name this misjudges can
// delay a readout but never silence it.
// A skill's tooltip is taken only when its own first lines name the skill
// under the cursor, with no time limit: the game went on drawing Prayer's,
// the skill last clicked, and every skill after was read as Prayer (MEASURED
// 2026-09-28 03:25:57 on). The first lines, because a tooltip names other
// skills lower down among what it gains from.
uintptr_t g_skillHeld = 0;
bool NamesSkill(const D2RL::PluginContext* context, const std::string& body, uintptr_t widget) {
	const std::string name = NativeSkillIcons::Name(widget);
	if (name.empty()) return true;
	const auto lines = TooltipText::Read(body).lines;
	for (size_t i = 0; i < lines.size() && i < 2; ++i)
		if (lines[i].find(name) != std::string::npos) return true;
	if (g_skillHeld != widget) {
		g_skillHeld = widget;
		D2RAccess::AsyncLog::InfoF(context, "[interaction] held back a skill tooltip that is not \"%s\"'s: \"%s\"", name.c_str(),
			lines.empty() ? "" : lines.front().c_str());
	}
	return false;
}

// The mercenary's panel. A skill icon has no text of its own: it is named from
// the tooltip text the panel keeps on it, and the rest is its detail, for J or
// autodetail. Ctrl with the arrows reads the mercenary's statistics there,
// which nothing else reaches from the keyboard; on the controller they are
// Advanced Stats' (the user, 2026-09-28: "skill0, skill1, skill2").
bool MercenaryPanel(uintptr_t panel) { return panel && NativeWidgets::ReadName(panel) == MercenaryStats::Panel; }
UiText::Readout MercenarySkill(NativeUi::Focus focus) {
	UiText::Readout readout;
	readout.context = NativeWidgets::ReadName(focus.panel);
	readout.focusKey = std::to_string(focus.widget);
	const std::string name = NativeWidgets::ReadName(focus.widget);
	size_t index = 0;
	if (name.size() <= 5 || !name.starts_with("Skill")) return readout;
	for (size_t i = 5; i < name.size(); ++i) {
		if (name[i] < '0' || name[i] > '9') return readout;
		index = index * 10 + static_cast<size_t>(name[i] - '0');
	}
	const auto skills = MercenaryStats::Skills(focus.panel);
	if (index >= skills.size()) return readout;
	const auto lines = TooltipText::Read(skills[index].text).lines;
	if (lines.empty()) return readout;
	readout.option = lines.front();
	std::string detail;
	for (size_t i = 1; i < lines.size(); ++i) Append(detail, lines[i]);
	if (AutoDetail::Enabled()) Append(readout.option, detail);
	OfferDetail(readout.focusKey, detail);
	return readout;
}
std::vector<std::string> MercenaryLines(uintptr_t panel) {
	std::vector<std::string> lines;
	for (auto line : {MercenaryStats::Name(panel), MercenaryStats::LevelAndType(panel), MercenaryStats::Experience(panel)})
		if (!line.empty()) lines.push_back(std::move(line));
	for (auto& line : MercenaryStats::Lines(panel))
		if (!line.empty()) lines.push_back(std::move(line));
	return lines;
}

constexpr uint64_t NamesItemMs = 400;
uintptr_t g_namingWidget = 0;
uint64_t g_namingSelection = 0, g_namingSince = 0;
bool g_namingLogged = false;
bool NamesItem(const D2RL::PluginContext* context, const std::string& body, const std::string& name, uintptr_t widget, uint64_t selection, uint64_t now, bool fixedName) {
	if (widget != g_namingWidget || selection != g_namingSelection) {
		g_namingWidget = widget;
		g_namingSelection = selection;
		g_namingSince = now;
		g_namingLogged = false;
	}
	if (name.empty() || UiText::Clean(body).find(name) != std::string::npos) return true;
	if (!fixedName && now - g_namingSince >= NamesItemMs) {
		D2RAccess::AsyncLog::InfoF(context, "[interaction] a tooltip without \"%s\" taken after %llu ms", name.c_str(),
			static_cast<unsigned long long>(NamesItemMs));
		return true;
	}
	if (!g_namingLogged) {
		g_namingLogged = true;
		D2RAccess::AsyncLog::InfoF(context, "[interaction] held back a tooltip that does not name \"%s\": the last place's", name.c_str());
	}
	return false;
}
// A table name with its quality: what an item is called when no tooltip names it.
std::string TableName(const std::string& base, uint32_t quality) {
	const auto& qualities = Strings::Current().itemQualities;
	std::string name = base;
	if (quality < qualities.size()) Append(name, qualities[quality]);
	return name;
}

// The item the game handed back: the one just under the focus, or one of its own.
std::string HeldName(const InventoryCursor::ItemState& held) {
	if (held.id == g_hovered.item.id && !g_hovered.name.empty()) return g_hovered.name;
	return ItemName(held.id, held.classId, held.quality);
}

ItemHand::Under UnderOf(const InventoryCursor::ItemState& item) { return {item.id, item.sockets, item.filled, item.quantity}; }

// An item's sockets: the total from the SDK, the filled ones from the item's own
// inventory, because the SDK read 0 for a filled socket. docs/inventory-npc-followup.md#socket-contents
std::optional<ItemHand::Under> ReadUnder(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit) {
	const auto state = id ? InventoryCursor::Find(context, id) : std::nullopt;
	if (!state) return std::nullopt;
	auto item = UnderOf(*state);
	item.filled = NativeEntities::FilledSockets(unit).value_or(ItemHand::UnknownFilled);
	return item;
}

// What the focus rests on while the hand is full, read once per item.
void Hover(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit, const std::string& name) {
	if (id == g_hovered.item.id) {
		if (!name.empty()) g_hovered.name = name;
		return;
	}
	g_hovered = {{id}, name};
	if (const auto item = ReadUnder(context, id, unit)) {
		g_hovered.item = *item;
		g_hovered.read = true;
	}
}

// Whether an item has room for a gem, rune or jewel; only the total when the
// filled ones cannot be read.
std::string SocketState(const ItemHand::Under& item) {
	const auto& strings = Strings::Current();
	if (!item.sockets) return strings.socketsNone;
	char text[80]{};
	if (item.filled == ItemHand::UnknownFilled)
		std::snprintf(text, sizeof(text), strings.socketsCount, static_cast<int>(item.sockets));
	else if (item.filled >= item.sockets) return strings.socketsFull;
	else std::snprintf(text, sizeof(text), strings.socketsFree, static_cast<int>(item.sockets - item.filled));
	return text;
}

// What the hand did: picked up, put down, socketed, stacked, swapped or thrown.
std::string HandNotice(const D2RL::PluginContext* context,
	const InventoryCursor::ItemState& held, uint32_t heldBefore, bool wasHolding, bool filler, uint32_t focusedId, uintptr_t focusedUnit) {
	const auto& strings = Strings::Current();
	if (held.id && !wasHolding) {
		g_heldUnmet = g_itemUnmet;
		return {}; // local preference: picking an item up is silent
	}
	if (!wasHolding) return {};
	ItemHand::Under after{focusedId};
	if (focusedId && focusedId == g_hovered.item.id)
		if (const auto item = ReadUnder(context, focusedId, focusedUnit)) after = *item;
	const bool dropPressed = InputDirection::LastFaceButton(EquipAttempt::AnswerMs) == InputDirection::FaceButton::X;
	const auto outcome = ItemHand::Resolve(heldBefore, held.id, g_hovered.item, after, filler, dropPressed);
	if (outcome != ItemHand::Outcome::Placed && outcome != ItemHand::Outcome::None)
		AsyncLog::InfoF(context, "[item] hand outcome=%d held=%u->%u filler=%u under=%u sockets=%u/%u->%u/%u quantity=%d->%d name=\"%s\"",
			static_cast<int>(outcome), heldBefore, held.id, filler ? 1u : 0u, g_hovered.item.id, g_hovered.item.filled,
			g_hovered.item.sockets, after.filled, after.sockets, g_hovered.item.quantity, after.quantity, g_hovered.name.c_str());
	switch (outcome) {
	case ItemHand::Outcome::Swapped: {
		// A gem handed back for the item it was put on: the game would not socket it.
		std::string text = filler && held.id == g_hovered.item.id ? strings.socketRefused : "";
		Append(text, Formatted(strings.itemSwapped, HeldName(held)));
		return text;
	}
	case ItemHand::Outcome::Merged: return Formatted(strings.itemAddedTo, g_hovered.name);
	case ItemHand::Outcome::Thrown: return strings.itemDropped;
	case ItemHand::Outcome::Socketed: {
		auto text = Formatted(strings.itemSocketedIn, g_hovered.name);
		Append(text, SocketState(after));
		return text;
	}
	case ItemHand::Outcome::Placed: case ItemHand::Outcome::None: break;
	}
	if (held.id) return {};
	// A plain release stays silent on this branch. Only the specific outcomes
	// above add feedback; the existing focus readout already names a placement.
	return {};
}

// The sockets of the item under a gem in hand, read once per item and hand.
std::string SocketsUnder(const D2RL::PluginContext* context, uint32_t id, uintptr_t unit) {
	if (id != g_socketRead.id) {
		g_socketRead = {id, {}};
		if (const auto item = ReadUnder(context, id, unit)) g_socketRead.state = SocketState(*item);
	}
	return g_socketRead.state;
}

std::string CursorNotice(const D2RL::PluginContext* context, bool inventory,
	uint32_t focusedId, uintptr_t focusedUnit, const std::string& focusedName) {
	// Focus may pass through another control while the same item remains held.
	if (!inventory && !g_wasHolding.value_or(false)) return {};
	const auto held = InventoryCursor::Held(context);
	if (!held) return {};
	const bool wasHolding = g_wasHolding.value_or(false), wasFiller = g_heldFiller;
	const uint32_t heldBefore = std::exchange(g_heldId, held->id);
	g_wasHolding = held->id != 0;
	if (held->id != heldBefore) {
		g_heldFiller = held->id && NativeGroundItems::SocketFiller(held->classId);
		g_socketRead = {};
	}
	auto notice = inventory ? HandNotice(context, *held, heldBefore, wasHolding, wasFiller, focusedId, focusedUnit) : std::string{};
	// Recorded after the notice, which judges the hand against what lay under it before.
	if (held->id) Hover(context, focusedId, focusedUnit, focusedName);
	else g_hovered = {};
	return notice;
}
}

std::string ItemName(uint32_t id, uint32_t classId, uint32_t quality) {
	const auto label = NativeGroundItems::ClassLabel(classId, quality);
	auto name = g_itemNames.Recall(id, label.baseName, quality);
	return name.empty() ? TableName(label.baseName, quality) : name;
}

void OfferDetail(std::string_view owner, std::string detail) {
	if (owner != g_detailFor) {
		g_detailFor = owner;
		g_detail.clear();
	}
	if (!detail.empty()) g_detail = std::move(detail);
}

bool HandlesFocus(NativeUi::Focus focus) {
	const auto type = NativeUi::Classify(focus.widget);
	const auto kind = type.kind;
	return IsInventory(kind) || kind == UiText::Kind::Button || kind == UiText::Kind::StatButton
		|| kind == UiText::Kind::TabButton || kind == UiText::Kind::Tabs || CharacterStats::HandlesFocus(focus, type)
		|| type.name == "HirelingSkillIconWidget";
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange) {
	// The Advanced Stats list has nothing the controller can focus, and a menu
	// reader may or may not own its scroll bar, so its stick reader runs either
	// way -- and before the character panel's, whose panel stays open behind it.
	if (AdvancedStatsReader::Tick(context, focus)) return !conversation;
	// The hire list's stats exist only in the game's tooltip, and a menu
	// suspends this reader and its capture; HireStats keeps capture on there
	// and takes the right stick. Its skill icons and gold lines are the
	// keyboard cursor's places whether or not the menu reader has anything to
	// say of the panel, so it is asked either way: left to the readers below, an
	// icon was read as the first row's skill of that number, or by its layout name.
	const bool hire = HireStats::Tick(context, now, focus);
	if (conversation) {
		if (!hire) Reset();
		return false;
	}
	if (hire) return true;
	// Focus that has been anywhere else since, even back in the same panel
	// after another tab, counts as moved when the next tab change comes.
	if (focus.panel != g_readPanel) g_readPanel = 0;
	// The character panel's own stick reader goes first: it claims all four
	// directions, and ReadDetail below would otherwise take up from it. Nothing
	// is lost by that order -- the character panel holds neither items nor
	// skills, so there is never a detail for up to read there.
	if (CharacterStatsReader::Tick(context, focus)) return true;
	if (ReadLines(context, focus)) return true;
	if (ReadSkillLines(context, focus, now)) return true;
	if (ReadDetail(context, focus)) return true;
	if (ReadHints(context)) return true;
	if (ReadCompare(context, focus, now)) return true;
	if (ReadAside(context, focus)) return true;
	const uintptr_t tooltips = NativeWidgets::FindPanel("TooltipsPanel");
	NativeTooltips::Track(tooltips);
	TooltipDelay::Apply(context, tooltips);
	const auto type = NativeUi::Classify(focus.widget);
	const bool inventory = IsInventory(type.kind);
	const bool stashSlot = type.name == "AdvancedStashSlotWidget";
	// These fixed slots carry the game's item-code string keys even when
	// empty. Read the visible identity now; tooltip properties remain detail.
	const std::string stashName = stashSlot ? UiText::Clean(Localization::Text(NativeWidgets::ReadName(focus.widget))) : std::string{};
	const auto itemSlot = stashSlot ? NativeUi::ItemWidgetSlot(focus.widget) : NativeUi::GridSlot{};
	if (type.kind == UiText::Kind::InventoryGrid) InventoryCensus::Log(context, focus, now);
	const auto selection = NativeUi::FocusSelection(focus.widget);
	const auto equipmentPresence = type.kind == UiText::Kind::InventorySlot
		? NativeUi::EquipmentItemPresence(focus.widget) : NativeUi::ItemPresence::Unknown;
	const bool tabBar = type.kind == UiText::Kind::Tabs;
	const bool button = type.kind == UiText::Kind::Button || type.kind == UiText::Kind::StatButton || type.kind == UiText::Kind::TabButton
		|| tabBar;
	// The character panel's non-button rows: a label the controller stops on
	// through its focusable Tooltip child, with its value in a sibling widget.
	const bool stats = !inventory && !button && CharacterStats::HandlesFocus(focus, type);
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(context, focus, type)
		: stats ? CharacterStats::Read(focus, type)
		: type.name == "HirelingSkillIconWidget" ? MercenarySkill(focus) : UiText::Readout{};
	// A belt slot's item is named by the game's tooltip like any other, drawn
	// at once now the controller layout's delay is gone (TooltipDelay; the
	// user, 2026-09-22, on naming it early from the slot: "the belt fix is now
	// unnecessary"). An empty slot has no tooltip at all, so it says so itself.
	uintptr_t beltItem = 0;
	if (type.name == "BeltWidget") {
		const auto belt = NativeUi::BeltItem(focus.widget);
		if (belt.slot != g_beltSlot) {
			g_beltSlot = belt.slot;
			AsyncLog::InfoF(context, "[belt] slot=%d known=%d item=%p", belt.slot, belt.known ? 1 : 0,
				reinterpret_cast<void*>(belt.item));
		}
		if (belt.known && !belt.item) {
			readout.option = Strings::Current().slotEmpty;
			// An empty slot says where it is, as an empty cell of a grid does. The
			// belt's rows are counted from the bottom one, which is the row that is
			// drunk from; its columns are the keys 1 to 4.
			if (belt.slot >= 0) {
				char phrase[64]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, belt.slot / 4 + 1, belt.slot % 4 + 1);
				readout.coordinates = phrase;
			}
		}
		// An empty bottom cell shows, see-through, the inventory potion its
		// D-pad button would drink (docs/belt.md): name it instead of "Empty".
		if (belt.known && !belt.item && belt.slot >= 0 && belt.slot < 4) {
			if (const auto automatic = NativeBelt::AutomaticItem(context, focus.widget, belt.slot); automatic && *automatic) {
				readout.option = NativeGroundItems::ItemBaseName(*automatic);
				Append(readout.option, Strings::Current().beltAutomatic);
			}
		}
		if (belt.known) beltItem = belt.item;
	} else g_beltSlot = -2;
	// Moving to another cell drops the previous item's detail even before its
	// own tooltip renders, so a push up can never answer about the last item.
	if (inventory) OfferDetail(readout.focusKey, {});
	else if (progression.skill) OfferDetail(std::to_string(focus.widget), {});
	// A stat's explanation ("Increases attack damage...") is its detail: right
	// stick up, or read with it under autodetail.
	else if (type.kind == UiText::Kind::StatButton) OfferDetail(std::to_string(focus.widget), progression.description);
	bool goldAmount = false; // the gold row is saying the amount in place of its label
	if (button) {
		const auto snapshot = NativeUi::Snapshot(focus.widget, focus.widget);
		readout = UiText::Describe(snapshot);
		readout.context = NativeWidgets::ReadName(focus.panel);
		readout.focusKey = std::to_string(focus.widget);
		if (!progression.readout.Empty() || progression.skill) {
			readout = progression.readout;
			if (type.kind == UiText::Kind::StatButton && AutoDetail::Enabled()) Append(readout.option, progression.description);
			if (!progression.skill && !snapshot.nodes.empty() && !snapshot.nodes.front().enabled)
				Append(readout.option, Strings::Current().unavailable);
		}
		// The inventory's gold button carries no text of its own -- MEASURED
		// 2026-09-20: [field] focus name="gold_button" ... text="" -- so landing
		// on it said only what the A button would do. The amount is a player
		// stat, read the same way a gold pile on the ground already is. It
		// REPLACES the label rather than joining it: "Drop Gold" is already the
		// A hint below, and a hint never leads the thing it acts on.
		// The tab bar across the top of the menus: moving along it highlights a
		// tab without opening it, so the highlighted label is read, not the open
		// tab the bar's own text gives (the user, 2026-09-21: the list of tabs at
		// the top "is still not reading"). INFERRED, not measured: the highlight
		// is the hover index, which a live read found at 2 while the open tab was
		// 3 (docs/navigation-latency.md). The log line settles it.
		if (tabBar) {
			readout.option = UiText::Clean(NativeUi::FocusedTabText(focus.widget));
			readout.focusKey = std::to_string(focus.widget) + ":" + std::to_string(selection);
			if (readout.focusKey != g_tabBarLogged) {
				g_tabBarLogged = readout.focusKey;
				AsyncLog::InfoF(context, "[tabs] bar highlight x=%d -> \"%s\" (open: \"%s\")", static_cast<int32_t>(selection),
					readout.option.c_str(), UiText::Clean(NativeUi::SelectedTabText(focus.widget)).c_str());
			}
		} else g_tabBarLogged.clear();
		const std::string focusName = NativeWidgets::ReadName(focus.widget);
		if (focusName == GoldButton) {
			if (const int gold = NativeGroundItems::PlayerGold(); gold >= 0) {
				char phrase[64]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldCarried,
					std::to_string(gold).c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
		// Its gold box has no focusable part on the controller layouts, so the
		// withdraw button says the amount it acts on, the same way.
		else if (focusName == StashGoldButton) {
			std::string gold = StashGold(context, focus.panel);
			if (const int stat = NativeGroundItems::PlayerStashGold(); gold.empty() && stat >= 0) gold = std::to_string(stat);
			if (!gold.empty()) {
				char phrase[96]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldStashed, gold.c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
	}
	// The item under the cursor, for its size and, while an item is held, its name.
	const uintptr_t focusItem = type.kind == UiText::Kind::InventoryGrid ? NativeUi::GridItem(focus.widget)
		: type.kind == UiText::Kind::InventorySlot ? NativeUi::EquipmentItem(focus.widget) : stashSlot ? itemSlot.item : beltItem;
	const std::string itemName = !stashName.empty() ? stashName : stashSlot ? NativeGroundItems::ItemBaseName(focusItem) : std::string{};
	const auto itemLabel = inventory ? NativeGroundItems::ItemUnitLabel(focusItem) : NativeGroundItems::UnitLabel{};
	const auto size = inventory ? SizePhrase(focusItem) : std::string{};
	if (inventory) OfferAside(focus.widget, size);
	else if (progression.skill) OfferAside(focus.widget, progression.points);
	else OfferAside(0, {});
	const auto tooltip = NativeTooltips::Latest(now);
	bool renderedDescription = false;
	std::string skillAddition;
	const std::string tableHeadline = progression.skill && !progression.name.empty()
		? ProgressionText::SkillReadout(progression.name, AutoDetail::Enabled() ? progression.points : std::string{}, progression.help).headline
		: std::string{};
	// Moving on drops the lines of what the cursor was on, as it drops its detail.
	const auto linesOwner = LinesOwner(focus.widget, selection);
	OfferLines(linesOwner, {});
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!inventory || tooltip.selection == selection)
		&& equipmentPresence != NativeUi::ItemPresence::Empty
		&& (!inventory || NamesItem(context, tooltip.body, stashSlot ? itemName : NativeGroundItems::ItemBaseName(focusItem), focus.widget, selection, now, stashSlot))
		&& (!progression.skill || NamesSkill(context, tooltip.body, focus.widget))) {
		std::string description;
		if (progression.skill) {
			description = ProgressionText::SkillDescription(tooltip.body);
			g_skillLines = ProgressionText::SkillLines(tooltip.body);
			g_skillLinesWidget = focus.widget;
			OfferLines(linesOwner, g_skillLines);
			Append(description, UiText::Clean(tooltip.footer));
		} else if (inventory) {
			// An item's own tooltip, in the order the screen draws it: the
			// name is spoken, with its price in a shop, and the rest is held
			// for the player to ask for.
			const auto body = ItemBody(tooltip.body);
			{
				std::vector<std::string> lines;
				for (const auto& line : body.lines) if (!line.empty()) lines.push_back(line);
				if (auto footer = UiText::Clean(tooltip.footer); !footer.empty()) lines.push_back(std::move(footer));
				if (!size.empty()) lines.push_back(size);
				OfferLines(linesOwner, std::move(lines));
			}
			const auto unidentified = UiText::Clean(Localization::Text(TooltipText::UnidentifiedKey));
			auto       split = TooltipText::Split(body, StatLabels(), PriceLabels(), unidentified, RequirementLabels());
			// Magic and better, the quality the screen shows only by colour
			// ("Crimson Spear. magic"); an unidentified item says so instead
			// (Split). A lesser item's own name already says it ("Superior Short
			// Sword"), and a runeword's name is its own, so the name is kept.
			if (!split.unidentified && !split.name.empty()) {
				const uint32_t quality = NativeGroundItems::ItemUnitLabel(focusItem).quality;
				const auto& qualities = Strings::Current().itemQualities;
				if (quality >= static_cast<uint32_t>(D2RL::Items::Quality::Magic) && quality < qualities.size())
					Append(split.name, qualities[quality]);
			}
			if (itemLabel.id) g_itemNames.Remember(itemLabel.id, itemLabel.baseName, itemLabel.quality, split.name);
			// What a sighted player sees in red, said right after that.
			g_itemUnmet = split.unmet;
			if (!split.unmet.empty()) Append(split.name, Refusal(split.unmet));
			split.headline = split.name;
			Append(split.headline, split.price);
			Append(split.detail, UiText::Clean(tooltip.footer));
			// The engine stores a body bottom-up, so Read gives the lines back
			// in screen order and the name leads. The log still names the line
			// the headline came from, and whether the renderer styled it.
			if (g_logItems.load(std::memory_order_relaxed)) {
				const std::string line = std::to_string(body.lines.size()) + " lines, title line "
					+ (body.Marked() ? std::to_string(body.heading) : std::string("unstyled"))
					+ " -> \"" + split.headline + "\" | " + split.detail;
				if (line != g_itemLogged) {
					g_itemLogged = line;
					D2RAccess::AsyncLog::InfoF(context, "[item] %s", line.c_str());
					// How the game styled each line (colour of unmet requirements): docs/native-tooltips.md.
					if (const auto markup = TooltipText::Markup(tooltip.body); !markup.empty())
						D2RAccess::AsyncLog::InfoF(context, "[item-markup] %s", markup.c_str());
				}
			}
			if (const auto comparison = TooltipText::Comparison(tooltip.extras); comparison != g_compareLogged) {
				g_compareLogged = comparison;
				if (!comparison.empty()) D2RAccess::AsyncLog::InfoF(context, "[compare] drawn -> \"%s\"", comparison.c_str());
			}
			// With autodetail the cells the item takes up come right after the
			// name, ahead of any price; otherwise they wait for right stick right.
			if (AutoDetail::Enabled()) {
				description = std::move(split.name);
				Append(description, size);
				Append(description, split.price);
				Append(description, split.detail);
			} else description = std::move(split.headline);
			OfferDetail(readout.focusKey, std::move(split.detail));
		} else {
			description = UiText::Clean(tooltip.body);
			Append(description, UiText::Clean(tooltip.footer));
		}
		renderedDescription = !description.empty();
		if (type.kind == UiText::Kind::InventoryGrid && renderedDescription) {
			readout.option = description;
			readout.coordinates.clear(); // a described cell keeps its existing coordinate-free readout
		}
		else if (progression.skill) {
			// The choices left are right stick right's, and join the readout only
			// with autodetail (the user, 2026-09-21: heard on every highlight).
			auto split = ProgressionText::SkillReadout(description,
				AutoDetail::Enabled() ? progression.points : std::string{}, progression.help);
			if (!tableHeadline.empty() && split.headline == tableHeadline) {
				if (AutoDetail::Enabled()) skillAddition = split.detail;
			} else if (!tableHeadline.empty() && g_skillMismatch != progression.name + "|" + split.headline) {
				g_skillMismatch = progression.name + "|" + split.headline;
				AsyncLog::InfoF(context, "[skill-tree] table name %s differs from tooltip %s", progression.name.c_str(), split.headline.c_str());
			}
			readout.option = std::move(split.headline);
			if (AutoDetail::Enabled()) Append(readout.option, split.detail);
			OfferDetail(std::to_string(focus.widget), std::move(split.detail));
		}
		// Not onto the gold amount: that row's tooltip is "Drop Gold", the A hint
		// again, and it renders ~400 ms after the row was read, so joining it
		// re-read the whole row (MEASURED 2026-09-21: "34 gold. Drop Gold").
		else if ((inventory || button) && !goldAmount && (type.kind != UiText::Kind::StatButton || progression.readout.Empty())) {
			Append(readout.option, description);
		}
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	if (progression.skill && !renderedDescription && !tableHeadline.empty()) readout.option = tableHeadline;
	if (!progression.skill) { g_skillLines.clear(); g_skillLinesWidget = 0; }
	if (stashSlot && !renderedDescription) {
		readout.option = itemName;
	}
	if (stashSlot && !readout.option.empty() && itemSlot.known && !itemSlot.item) Append(readout.option, Strings::Current().slotEmpty);
	// An item there keeps its own lines; anywhere else on the panel, the mercenary's.
	if (MercenaryPanel(focus.panel) && !renderedDescription) OfferLines(linesOwner, MercenaryLines(focus.panel));
	// While an item is held the game draws no tooltip for what the cursor is
	// over -- MEASURED 2026-09-21, none in five seconds of moving a held item
	// across others -- so an occupied cell said nothing and a slot only
	// "Occupied". The item there, the one a press would swap into the hand, is
	// named from the grid or the slot. Whether an item is held is asked of the
	// game each time, never remembered.
	std::string heldOver;
	if (!renderedDescription && focusItem
		&& (type.kind == UiText::Kind::InventoryGrid || equipmentPresence == NativeUi::ItemPresence::Occupied || beltItem)
		&& InventoryCursor::HoldingItem(context).value_or(false)) {
		heldOver = g_itemNames.Recall(itemLabel.id, itemLabel.baseName, itemLabel.quality);
		if (heldOver.empty()) heldOver = NativeGroundItems::ItemBaseName(focusItem);
		if (g_heldFiller && itemLabel.id) Append(heldOver, SocketsUnder(context, itemLabel.id, focusItem));
		if (heldOver.empty()) heldOver = Strings::Current().slotOccupied;
		if (AutoDetail::Enabled()) Append(heldOver, size);
	}
	if ((type.kind == UiText::Kind::InventoryGrid || beltItem) && !heldOver.empty()) {
		readout.option = heldOver;
		readout.coordinates.clear();
		renderedDescription = true;
	}
	if (type.kind == UiText::Kind::InventorySlot && !renderedDescription) {
		if (equipmentPresence == NativeUi::ItemPresence::Empty) Append(readout.option, Strings::Current().slotEmpty);
		// An occupied slot is read with its item or not at all. "Occupied" first
		// meant hearing the slot twice, the second time with the item (the user,
		// 2026-09-21), so it waits for the game's tooltip -- or, while an item is
		// held and there is none, is named from the slot.
		else if (equipmentPresence == NativeUi::ItemPresence::Occupied) Append(readout.option, heldOver);
		// Neither a known slot nor readable contents: say what the focus is on
		// rather than let the move pass in silence.
		else if (readout.option.empty()) Append(readout.option, Strings::Current().equipment);
	}
	// The controls the focus offers. Never part of a readout: held for right
	// stick down, and spoken unasked only last and only with autodetail and
	// control tooltips on -- MEASURED 2026-09-21, a change of hints spoken on
	// its own interrupted the gold amount it followed.
	const auto help = !(inventory || button || stats) ? std::string{}
		: progression.skill ? progression.controls : NativeControllerHelp::ForFocus(focus);
	if (inventory || button || stats) ControlHints::Offer(help);
	const auto helpContext = readout.context + ":" + readout.body;
	// Button hints belong after the thing they act on. An occupied cell or slot
	// is still waiting for the game's own tooltip (~350 ms), so the hints wait
	// with it instead of arriving first. A grid cell that could not be read
	// waits too: its position alone says "empty", and only a cell known to be
	// empty may say that (the user, 2026-09-21).
	const bool occupied = type.kind == UiText::Kind::InventorySlot
		? equipmentPresence == NativeUi::ItemPresence::Occupied
		: type.kind == UiText::Kind::InventoryGrid
			? NativeUi::GridItemPresence(focus.widget) != NativeUi::ItemPresence::Empty
			: stashSlot ? !itemSlot.known || itemSlot.item != 0 : beltItem != 0;
	const bool awaitingDescription = inventory && !renderedDescription && occupied && !(stashSlot && !itemName.empty());
	// The game refuses an item it will not equip by leaving it in hand. An A
	// press on an equipment slot with the same item still held once the game
	// has had time to answer was refused (equip_attempt.h).
	std::string cursorNotice;
	if (inventory) {
		cursorNotice = CursorNotice(context, inventory, itemLabel.id, focusItem, heldOver.empty() ? readout.option : heldOver);
		if (!cursorNotice.empty()) AsyncLog::InfoF(context, "[item] hand -> \"%s\"", cursorNotice.c_str());
		const bool held = g_wasHolding.value_or(false);
		const bool pressed = held && type.kind == UiText::Kind::InventorySlot
			&& InputDirection::LastFaceButton(EquipAttempt::PressWindowMs) == InputDirection::FaceButton::A;
		const uint32_t item = held ? InventoryCursor::HeldItemId(context).value_or(0) : 0;
		if (g_equipAttempt.Update(pressed, item, now)) {
			const auto refusal = g_heldFiller && equipmentPresence == NativeUi::ItemPresence::Occupied
				? std::string(Strings::Current().socketRefused) : Refusal(g_heldUnmet);
			D2RAccess::AsyncLog::InfoF(context, "[equip] refused -> \"%s\"", refusal.c_str());
			ScreenReader::Speak(refusal, true);
		}
	}
	const bool helpChanged = help != g_spokenHelp || helpContext != g_helpContext;
	g_help = help;
	if (!focus.panel && readout.Empty()) {
		const auto target = NativeUi::Snapshot(NativeWidgets::FindPanel("HUDMonsterHealth"), 0);
		if (target.surface == UiText::Surface::Target) readout = UiText::Describe(target);
	}
	if (!focus.panel && GroundItemService::OwnsWorldLabel(readout.body)) {
		readout = {};
		renderedDescription = false;
	}
	// The game targets through walls, so either world label — the monster health
	// plate and the world target tooltip — can name something in the next room,
	// which the right-stick scan would never list. Those are dropped and the
	// reader is left with nothing to say, so the same entity announces itself as
	// soon as it is really in sight.
	if (!focus.panel && !readout.Empty()
		&& EntityRadarService::LabelSight(readout.body) == TargetSight::Sight::Hidden) {
		if (g_hiddenLabel != readout.body) {
			g_hiddenLabel = readout.body;
			AsyncLog::InfoF(context, "[interaction] behind a wall, not spoken -> \"%s\"", readout.body.c_str());
		}
		readout = {};
		renderedDescription = false;
	} else if (!readout.Empty()) g_hiddenLabel.clear();
	if (inventory != g_inventory) {
		// Leaving the inventory for play silences what it was saying; leaving it
		// for another tab does not, because that tab's title is already being
		// spoken by the time focus reaches it. INFERRED from the 13:03:28 log of
		// 2026-09-21: "Character" began at .350 and the character's name was
		// queued at .397, in the same poll as the first focus off the inventory,
		// and the user heard both cut to silence. The title interrupts an item
		// readout on its own.
		if (g_inventory && RadarUi::InPlay()) ScreenReader::Stop();
		g_reader.Reset();
	}
	g_inventory = inventory;
	if (!inventory) g_inventoryContext = {};
	if (!tabChange.empty()) {
		// A tab switch announces its name, and the new panel's first highlight
		// follows it, queued (the user, 2026-09-21: "tab header spoken in full,
		// then the initially highlighted choice"). Focus that stayed where it
		// was -- a stash tab changed with LT/RT while the cursor is in the
		// player's inventory -- is taken as the baseline and not read again.
		if (readout.Empty() || focus.panel != g_readPanel) g_reader.Reset();
		else (void)g_reader.Update(readout, now, 0);
		g_tabTitle = UiText::Clean(tabChange);
		// The menu switcher's tab is the panel the window announcer has just
		// named. MEASURED 2026-09-21: "Character" was said twice, the second
		// time with interrupt, which cut off what was queued behind the first.
		g_tabChangedAt = now;
		const bool named = PanelState::TakeAnnounced(tabChange);
		if (named) D2RAccess::AsyncLog::InfoF(context, "[tabs] %s already named by the window, not repeated", std::string(tabChange).c_str());
		std::string text(named ? std::string_view{} : tabChange);
		Append(text, cursorNotice);
		if (!text.empty()) ScreenReader::Speak(text, !named);
		return true;
	}
	// An occupied grid cell or equipment slot waits for the game's tooltip to
	// name its item, without speaking its position or its slot alone first;
	// other cells keep their quick coordinate feedback.
	if (occupied && !renderedDescription && heldOver.empty() && !(stashSlot && !itemName.empty())) {
		if (!cursorNotice.empty()) ScreenReader::Speak(cursorNotice, false);
		return true;
	} // the hints stay pending until the description arrives
	if (progression.skill && !renderedDescription && tableHeadline.empty()) return true;
	if (readout.Empty() && help.empty()) { g_reader.Reset(); if (!cursorNotice.empty()) ScreenReader::Speak(cursorNotice, false); return inventory || button || stats; }
	// A rendered, focus-matching item description is already complete. Cells
	// without one wait a single poll for capture, rather than the dialogue delay.
	auto announcement = progression.skill && !renderedDescription ? g_reader.Update(readout, now, InventorySettleMs)
		: button ? g_reader.Update(readout, now, 0)
		: inventory ? g_reader.Update(readout, now, renderedDescription || type.kind == UiText::Kind::InventorySlot || (stashSlot && !itemName.empty()) ? 0 : InventorySettleMs)
		: g_reader.Update(readout, now);
	if (type.kind == UiText::Kind::StatButton && !progression.readout.Empty()) {
		std::string changed;
		if (g_stat.widget == focus.widget) {
			// The new value alone: right stick right says the points left.
			if (g_stat.value != progression.value) Append(changed, progression.value);
		}
		if (!changed.empty() && !announcement.text.empty()) announcement.text = std::move(changed);
		g_stat = {focus.widget, progression.value, progression.points};
	}
	// A control hint follows the thing it acts on and never leads. While the
	// focus has not been described yet the hint waits for the readout that
	// will describe it -- that is how "B: Close Menu" arrived before "Belt.
	// Empty". Once the player has heard where they are, a hint that changes
	// under them is news too (the merchant's action list), so it is offered
	// again -- queued, never interrupting.
	const bool describedNow = !announcement.text.empty();
	const bool describedHere = readout.focusKey.empty() || readout.focusKey == g_describedFocus;
	const bool helpFollows = helpChanged && !awaitingDescription && (describedNow || describedHere);
	bool skillDetailFollows = !skillAddition.empty() && describedNow && g_skillHeadline == readout.focusKey + "|" + tableHeadline;
	if (skillDetailFollows) announcement.text = skillAddition;
	if (describedNow) {
		g_describedFocus = readout.focusKey;
		g_skillHeadline = progression.skill && !renderedDescription ? readout.focusKey + "|" + readout.option : std::string{};
	}
	// Only what was actually spoken counts as spoken. An empty hint is
	// recorded either way, so leaving a panel does not leave the last hint
	// looking unchanged when the player comes back to it.
	if (!awaitingDescription && (helpFollows || help.empty())) {
		g_spokenHelp = help;
		g_helpContext = helpContext;
	}
	if (!announcement.text.empty()) {
		D2RAccess::AsyncLog::InfoF(context, "[interaction] %s -> \"%s\"", readout.context.c_str(), announcement.text.c_str());
		const bool behindTitle = g_tabChangedAt && now - g_tabChangedAt < TabTitleMs;
		if (behindTitle && announcement.text.starts_with(g_tabTitle + ". "))
			announcement.text.erase(0, g_tabTitle.size() + 2);
		g_tabChangedAt = 0;
		g_readPanel = focus.panel;
		ScreenReader::Speak(announcement.text, !behindTitle && !skillDetailFollows);
	}
	if (helpFollows) ControlHints::SpeakAfter(context, help);
	if (!cursorNotice.empty()) ScreenReader::Speak(cursorNotice, false);
	return inventory || button || stats;
}

void Reset() {
	g_reader.Reset();
	g_inventory = false;
	g_inventoryContext = {};
	g_stat = {};
	g_help.clear();
	g_spokenHelp.clear();
	g_describedFocus.clear();
	g_skillHeadline.clear();
	g_skillMismatch.clear();
	g_helpContext.clear();
	g_hiddenLabel.clear();
	g_tabChangedAt = 0;
	g_tabTitle.clear();
	if (g_lineReader.widget) PadFilter::ReleaseB();
	g_lineReader = {};
	g_keyLines = {};
	g_skillLines.clear();
	g_skillLinesWidget = 0;
	g_readPanel = 0;
	g_wasHolding.reset();
	g_heldId = 0; g_heldFiller = false; g_hovered = {}; g_socketRead = {}; g_itemNames.Clear();
	g_itemUnmet.clear();
	g_heldUnmet.clear();
	g_equipAttempt.Reset();
	g_slotLocations.clear();
	g_detail.clear();
	g_detailFor.clear();
	g_aside.clear();
	g_asideWidget = 0;
	g_itemLogged.clear();
	g_compareLogged.clear();
	CharacterStats::Reset();
	CharacterStatsReader::Reset();
	NativeControllerHelp::Reset();
	NativeTooltips::Reset();
	ControlHints::Offer({});
}

void SetItemLogging(bool enabled) { g_logItems.store(enabled, std::memory_order_relaxed); }
bool ItemLogging() { return g_logItems.load(std::memory_order_relaxed); }

std::string Repeat() {
	std::string text = g_reader.Repeat();
	if (ControlHints::Automatic()) Append(text, g_help);
	return text;
}
}
