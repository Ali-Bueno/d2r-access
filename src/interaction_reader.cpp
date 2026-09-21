#include "async_log.h"
#include "auto_detail.h"
#include "control_hints.h"
#include "character_stats.h"
#include "character_stats_reader.h"
#include "entity_radar_service.h"
#include "generated/item_sizes.h"
#include "interaction_reader.h"
#include "inventory_census.h"
#include "inventory_cursor.h"
#include "ground_item_service.h"
#include "input_direction.h"
#include "localization.h"
#include "native_tooltips.h"
#include "native_controller_help.h"
#include "native_ground_items.h"
#include "native_progression.h"
#include "progression_text.h"
#include "radar_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "tab_reader.h"
#include "tooltip_delay.h"
#include "tooltip_text.h"
#include "ui_text.h"

#include <cstdio>

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
// The world label last dropped for being out of sight, so the log records each
// one once instead of on every UI poll.
std::string g_hiddenLabel;
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

// The cells the highlighted item takes up, read on its own with a push right
// on the right stick, apart from the detail like the button hints (the user,
// 2026-09-21: "put size to be read separately on right stick right. it should
// not read at all on right stick up").
std::string g_size, g_sizeFor;

// When a tab last changed. The first readout after it queues behind the title
// the window announcer is still speaking instead of cutting it off -- the skill
// tree's first skill arrived about 350 ms into "Skill Tree" and interrupted it
// (MEASURED 2026-09-21 13:03:30). Later readouts interrupt as usual.
uint64_t g_tabChangedAt = 0;
constexpr uint64_t TabTitleMs = 1500;

// The belt slot last logged, so [belt] is written once per move, not per poll.
int g_beltSlot = -2;

// The game's own labels for a weapon's damage and an armour's defense, so the
// split can tell them from the item's name in any language. Each table entry is a printf format, so only the part before its
// first placeholder can begin a rendered line -- resolving "Defense: %d" and
// then looking for that whole string never matched anything. Resolved once per
// language: the string tables do not change under a running game.
const std::vector<std::string>& StatLabels() {
	static std::vector<std::string> cached = [] {
		std::vector<std::string> labels;
		for (const char* key : TooltipText::BaseStatKeys) {
			auto text = TooltipText::LabelPrefix(Localization::Text(key));
			if (!text.empty()) labels.push_back(std::move(text));
		}
		return labels;
	}();
	return cached;
}

// What a vendor charges, and what it would pay for the player's own item: in a
// shop the name and this are the headline.
const std::vector<std::string>& PriceLabels() {
	static std::vector<std::string> cached = [] {
		std::vector<std::string> labels;
		for (const char* key : {TooltipText::PriceKey, TooltipText::SellKey}) {
			auto text = TooltipText::LabelPrefix(Localization::Text(key));
			if (!text.empty()) labels.push_back(std::move(text));
		}
		return labels;
	}();
	return cached;
}

// A push up on the right stick, claimed only while a panel is open so the
// entity scan and the pathfinder keep the stick during play.
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

// A push right on the right stick while an inventory item has focus. Only
// there: in play the stick belongs to the scan, the pathfinder and the exit
// finder, and the character panel's own reader has claimed it before this.
bool ReadSize(const D2RL::PluginContext* context, NativeUi::Focus focus, bool inventory) {
	if (!focus.panel || !inventory || !InputDirection::TakeRightStickPushIf(InputDirection::StickPush::Right, "size")) return false;
	const std::string& text = g_size.empty() ? Strings::Current().detailNone : g_size;
	D2RAccess::AsyncLog::InfoF(context, "[size] %s -> \"%s\"", g_sizeFor.c_str(), text.c_str());
	ScreenReader::Speak(text, true);
	return true;
}

void OfferSize(std::string_view owner, std::string size) {
	g_sizeFor = owner;
	g_size = std::move(size);
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

bool IsInventory(UiText::Kind kind) {
	return kind == UiText::Kind::InventoryGrid || kind == UiText::Kind::InventorySlot || kind == UiText::Kind::InventoryItem;
}

std::string Title(uintptr_t panel) {
	const bool stash = NativeUi::Classify(panel).name == "BankPanel";
	const auto localized = Localization::Text(stash ? "stash" : "minipanelinv");
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
		|| kind == UiText::Kind::TabButton || CharacterStats::HandlesFocus(focus, type);
}

bool Tick(const D2RL::PluginContext* context, uint64_t now, NativeUi::Focus focus, bool conversation, std::string_view tabChange) {
	if (conversation) { Reset(); return false; }
	// The character panel's own stick reader goes first: it claims all four
	// directions, and ReadDetail below would otherwise take up from it. Nothing
	// is lost by that order -- the character panel holds neither items nor
	// skills, so there is never a detail for up to read there.
	if (CharacterStatsReader::Tick(context, focus)) return true;
	if (ReadDetail(context, focus)) return true;
	if (ReadHints(context)) return true;
	const uintptr_t tooltips = NativeWidgets::FindPanel("TooltipsPanel");
	NativeTooltips::Track(tooltips);
	TooltipDelay::Apply(context, tooltips);
	const auto type = NativeUi::Classify(focus.widget);
	const bool inventory = IsInventory(type.kind);
	if (ReadSize(context, focus, inventory)) return true;
	if (type.kind == UiText::Kind::InventoryGrid) InventoryCensus::Log(context, focus, now);
	const auto selection = NativeUi::FocusSelection(focus.widget);
	const auto equipmentPresence = type.kind == UiText::Kind::InventorySlot
		? NativeUi::EquipmentItemPresence(focus.widget) : NativeUi::ItemPresence::Unknown;
	const bool button = type.kind == UiText::Kind::Button || type.kind == UiText::Kind::StatButton || type.kind == UiText::Kind::TabButton;
	// The character panel's non-button rows: a label the controller stops on
	// through its focusable Tooltip child, with its value in a sibling widget.
	const bool stats = !inventory && !button && CharacterStats::HandlesFocus(focus, type);
	const auto progression = button ? NativeProgression::Read(focus, type) : NativeProgression::Details{};
	if (type.kind != UiText::Kind::StatButton) g_stat = {};
	UiText::Readout readout = inventory ? Inventory(context, focus, type)
		: stats ? CharacterStats::Read(focus, type) : UiText::Readout{};
	// The belt names its slot's item straight away instead of waiting for the
	// game's tooltip, which the controller layout shows 400 ms late -- the lag
	// the user heard walking the belt on 2026-09-21. A slot that cannot be read
	// falls back to the tooltip, as before.
	bool beltNamed = false;
	if (type.name == "BeltWidget") {
		const auto belt = NativeUi::BeltItem(focus.widget);
		std::string name = belt.known && belt.item ? NativeGroundItems::ItemBaseName(belt.item) : std::string{};
		if (belt.known && !belt.item) name = Strings::Current().slotEmpty;
		if (belt.slot != g_beltSlot) {
			g_beltSlot = belt.slot;
			AsyncLog::InfoF(context, "[belt] slot=%d known=%d item=%p -> \"%s\"", belt.slot, belt.known ? 1 : 0,
				reinterpret_cast<void*>(belt.item), name.c_str());
		}
		if (!name.empty()) { readout.option = std::move(name); beltNamed = true; }
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
		if (NativeWidgets::ReadName(focus.widget) == GoldButton) {
			if (const int gold = NativeGroundItems::PlayerGold(); gold >= 0) {
				char phrase[64]{};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().goldCarried,
					std::to_string(gold).c_str());
				readout.option = phrase;
				goldAmount = true;
			}
		}
	}
	// The item under the cursor, for its size and, while an item is held, its name.
	const uintptr_t focusItem = type.kind == UiText::Kind::InventoryGrid ? NativeUi::GridItem(focus.widget)
		: type.kind == UiText::Kind::InventorySlot ? NativeUi::EquipmentItem(focus.widget) : 0;
	const auto size = inventory ? SizePhrase(focusItem) : std::string{};
	if (inventory) OfferSize(readout.focusKey, size);
	const auto tooltip = NativeTooltips::Latest(now);
	bool renderedDescription = false;
	// Discard the previous focus/cell's last rendered description while the new
	// one is being built. Empty cells still expose the actual grid coordinates.
	if (tooltip.panel == focus.panel && tooltip.widget == focus.widget
		&& (!inventory || tooltip.selection == selection)
		&& equipmentPresence != NativeUi::ItemPresence::Empty) {
		std::string description;
		if (progression.skill) {
			description = ProgressionText::SkillDescription(tooltip.body);
			Append(description, UiText::Clean(tooltip.footer));
		} else if (inventory) {
			// An item's own tooltip, in the order the screen draws it: the
			// name is spoken, with its price in a shop, and the rest is held
			// for the player to ask for.
			const auto body = TooltipText::Read(tooltip.body);
			auto       split = TooltipText::Split(body, StatLabels(), PriceLabels());
			Append(split.detail, UiText::Clean(tooltip.footer));
			// The engine stores a body bottom-up, so Read gives the lines back
			// in screen order and the name leads. The log still names the line
			// the headline came from, and whether the renderer styled it.
			D2RAccess::AsyncLog::InfoF(context, "[item] %zu lines, title line %s -> \"%s\" | %s",
				body.lines.size(), body.Marked() ? std::to_string(body.heading).c_str() : "unstyled",
				split.headline.c_str(), split.detail.c_str());
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
			auto split = ProgressionText::SkillReadout(description, progression.points, progression.help);
			readout.option = std::move(split.headline);
			if (AutoDetail::Enabled()) Append(readout.option, split.detail);
			OfferDetail(std::to_string(focus.widget), std::move(split.detail));
		}
		// Not onto the gold amount: that row's tooltip is "Drop Gold", the A hint
		// again, and it renders ~400 ms after the row was read, so joining it
		// re-read the whole row (MEASURED 2026-09-21: "34 gold. Drop Gold").
		else if ((inventory || button) && !goldAmount && (type.kind != UiText::Kind::StatButton || progression.readout.Empty())) {
			// The belt said the name from the slot already; its tooltip adds
			// only what follows the name.
			if (beltNamed && description.starts_with(readout.option)) {
				description.erase(0, readout.option.size());
				description.erase(0, description.find_first_not_of(". "));
			}
			Append(readout.option, description);
		}
		else if (!focus.panel) { readout.context = "World target"; readout.body = description; }
	}
	// While an item is held the game draws no tooltip for what the cursor is
	// over -- MEASURED 2026-09-21, none in five seconds of moving a held item
	// across others -- so an occupied cell said nothing and a slot only
	// "Occupied". The item there, the one a press would swap into the hand, is
	// named from the grid or the slot. Whether an item is held is asked of the
	// game each time, never remembered.
	std::string heldOver;
	if (!renderedDescription && focusItem
		&& (type.kind == UiText::Kind::InventoryGrid || equipmentPresence == NativeUi::ItemPresence::Occupied)
		&& InventoryCursor::HoldingItem(context).value_or(false)) {
		heldOver = NativeGroundItems::ItemBaseName(focusItem);
		if (heldOver.empty()) heldOver = Strings::Current().slotOccupied;
		if (AutoDetail::Enabled()) Append(heldOver, size);
	}
	if (type.kind == UiText::Kind::InventoryGrid && !heldOver.empty()) {
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
			&& NativeUi::GridItemPresence(focus.widget) != NativeUi::ItemPresence::Empty;
	const bool awaitingDescription = inventory && !renderedDescription && occupied;
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
		// A tab switch announces only its name. Accept the current cell as the
		// baseline so its delayed readout cannot immediately interrupt that name.
		if (readout.Empty()) g_reader.Reset();
		else (void)g_reader.Update(readout, now, 0);
		// The menu switcher's tab is the panel the window announcer has just
		// named. MEASURED 2026-09-21: "Character" was said twice, the second
		// time with interrupt, which cut off what was queued behind the first.
		g_tabChangedAt = now;
		const bool named = PanelState::TakeAnnounced(tabChange);
		if (named) D2RAccess::AsyncLog::InfoF(context, "[tabs] %s already named by the window, not repeated", std::string(tabChange).c_str());
		std::string text(named ? std::string_view{} : tabChange);
		if (!text.empty()) ScreenReader::Speak(text, !named);
		return true;
	}
	// An occupied grid cell or equipment slot waits for the game's tooltip to
	// name its item, without speaking its position or its slot alone first;
	// other cells keep their quick coordinate feedback.
	if ((type.kind == UiText::Kind::InventoryGrid || type.kind == UiText::Kind::InventorySlot)
		&& occupied && !renderedDescription && heldOver.empty())
		return true; // the hints stay pending until the description arrives
	if (progression.skill && !renderedDescription) return true;
	if (readout.Empty() && help.empty()) { g_reader.Reset(); return inventory || button || stats; }
	// A rendered, focus-matching item description is already complete. Cells
	// without one wait a single poll for capture, rather than the dialogue delay.
	auto announcement = button ? g_reader.Update(readout, now, 0)
		: inventory ? g_reader.Update(readout, now, renderedDescription || type.kind == UiText::Kind::InventorySlot ? 0 : InventorySettleMs)
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
	if (describedNow) g_describedFocus = readout.focusKey;
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
		g_tabChangedAt = 0;
		ScreenReader::Speak(announcement.text, !behindTitle);
	}
	if (helpFollows) ControlHints::SpeakAfter(context, help);
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
	g_helpContext.clear();
	g_hiddenLabel.clear();
	g_tabChangedAt = 0;
	g_slotLocations.clear();
	g_detail.clear();
	g_detailFor.clear();
	g_size.clear();
	g_sizeFor.clear();
	CharacterStats::Reset();
	CharacterStatsReader::Reset();
	NativeControllerHelp::Reset();
	NativeTooltips::Reset();
	ControlHints::Offer({});
}

std::string Repeat() {
	std::string text = g_reader.Repeat();
	if (ControlHints::Automatic()) Append(text, g_help);
	return text;
}
}
