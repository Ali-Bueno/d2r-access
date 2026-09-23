#include "vitals_service.h"
#include "async_log.h"
#include "input_direction.h"
#include "localization.h"
#include "native_memory.h"
#include "native_status.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "vitals.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/input.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace D2RAccess::VitalsService {
namespace {
enum class Command { None, On, Off, Status, Report };

// The game's own HUD widgets: HUDPanelHD.json declares HealthBall and ManaBall
// as AttributeBallWidgets, each carrying the ValueDisplay text box the game
// fills while the matching "show orb text" gameplay option is on.
constexpr const char* HudPanel = "HUDPanel";
const std::string LifePath[] = {"HealthBall", "ValueDisplay"};
const std::string ManaPath[] = {"ManaBall", "ValueDisplay"};
// Those two options, named by the keys the game's own widget catalogue uses
// for them (src/generated/panel_widgets.h), so the hint speaks their real
// on-screen labels in the player's language.
constexpr const char* LifeOrbTextOption = "OptionsMenuShowLifeOrbText";
constexpr const char* ManaOrbTextOption = "OptionsMenuShowManaOrbText";

constexpr uint64_t OrbSampleMs = 100;         // life can fall within a single blow
constexpr uint64_t DurabilitySampleMs = 1000; // durability only creeps down

const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};
bool g_enabled = true;
bool g_statusSupported = false;
unsigned g_hintedMask = 0;
uint64_t g_nextOrb = 0, g_nextDurability = 0;
Vitals::Alerts g_alerts;
Vitals::OrbAvailability g_orbAvailability;
Vitals::Condition g_poison;
Vitals::MercenaryMeter g_mercenary;
Vitals::Breakage g_breakage;
// The last census of what the character is wearing. See LogWorn.
std::string g_worn;

int ReadPercent(std::span<const std::string> path) {
	const auto orb = Vitals::ParseOrb(NativeWidgets::TextAt(HudPanel, path));
	return orb ? Vitals::Percent(orb->current, orb->maximum) : -1;
}

std::string FormattedPercent(const char* format, int percent) {
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), format, percent);
	return phrase;
}

void Announce(const Vitals::Alert& alert) {
	if (!alert.Any()) return;
	const auto& strings = Strings::Current();
	std::string text;
	if (alert.life >= 0) text = FormattedPercent(strings.vitalsLife, alert.life);
	if (alert.mana >= 0) {
		if (!text.empty()) text += ". ";
		text += FormattedPercent(strings.vitalsMana, alert.mana);
	}
	ScreenReader::Speak(text, alert.interrupt);
}

// Says which displayed orb value is unavailable, after persistent absence or
// an explicit report request. The labels come from the game's current locale.
void HintOrbText(unsigned missing, bool force = false) {
	if (!missing || (!force && missing == g_hintedMask)) return;
	const auto life = Localization::Text(LifeOrbTextOption);
	const auto mana = Localization::Text(ManaOrbTextOption);
	if (((missing & 1U) && life.empty()) || ((missing & 2U) && mana.empty())) return;
	g_hintedMask = missing;
	char phrase[300]{};
	if (missing == 3U)
		std::snprintf(phrase, sizeof(phrase), Strings::Current().vitalsOrbTextMissing, life.c_str(), mana.c_str());
	else
		std::snprintf(phrase, sizeof(phrase), Strings::Current().vitalsOrbTextMissingOne,
			(missing & 1U) ? life.c_str() : mana.c_str());
	AsyncLog::InfoF(g_context, "[vitals] orb values unreadable mask=%u", missing);
	ScreenReader::Speak(phrase, false);
}

// Life and mana on demand. The automatic announcements only speak when a
// step is crossed, so without a key there is no way to ask "where am I now";
// this reads the same two orbs at the moment the key is pressed.
void Report(const D2RL::PluginContext* context) {
	const auto& strings = Strings::Current();
	const NativeMemory::ReadScope scope;
	if (!NativeWidgets::IsAvailable() || !NativeWidgets::FindPanel(HudPanel)) {
		// Outside a game the orbs do not exist at all.
		AsyncLog::Info(context, "[vitals] report: no HUD");
		ScreenReader::Speak(strings.unavailable, true);
		return;
	}
	const int life = ReadPercent(LifePath), mana = ReadPercent(ManaPath);
	AsyncLog::InfoF(context, "[vitals] report life=%d mana=%d", life, mana);
	const unsigned missing = (life < 0 ? 1U : 0U) | (mana < 0 ? 2U : 0U);
	std::string text;
	if (life >= 0) text = FormattedPercent(strings.vitalsLife, life);
	if (mana >= 0) {
		if (!text.empty()) text += ". ";
		text += FormattedPercent(strings.vitalsMana, mana);
	}
	if (g_statusSupported && NativeStatus::PlayerPoisoned() == true) {
		if (!text.empty()) text += ". ";
		text += strings.playerPoisoned;
	}
	if (g_statusSupported) {
		const auto mercenary = NativeStatus::MercenaryLife();
		if (mercenary) {
			const int percent = Vitals::Percent(mercenary->currentLife, mercenary->maximumLife);
			if (percent >= 0) {
				if (!text.empty()) text += ". ";
				text += FormattedPercent(strings.mercenaryLife, percent);
			}
		}
	}
	// Asked for, so it interrupts: the player is waiting for this answer.
	if (!text.empty()) ScreenReader::Speak(text, true);
	HintOrbText(missing, true);
}

auto __cdecl CollectWorn(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* worn = static_cast<std::vector<Vitals::Breakage::Item>*>(userData);
	if (worn && item && item->structSize >= D2RL::Items::ItemInfoRequiredSize)
		worn->push_back({item->runtimeId, item->durability, item->maximumDurability, item->bodyLocation});
	return D2RL::Inventory::IterationAction::Continue;
}

// What the character is wearing, logged only when it changes.
//
// The mod cannot write a save: character_saves.cpp opens a .d2s GENERIC_READ
// and reads its 315-byte header, native_memory.h has no write primitive at
// all, and every inventory call here is a read -- createItem, editItem,
// destroyItem and executeLocalPlayerMove appear nowhere in this plugin. None
// of that is a record, though. When the user's equipment went missing between
// two loads there was nothing in the log to say when, because nothing had
// ever logged what was worn. One line per change fixes that: the next time
// anything changes, the log has the second it happened and everything the mod
// was doing around it.
void LogWorn(const D2RL::PluginContext* context, const std::vector<Vitals::Breakage::Item>& worn) {
	std::vector<std::pair<int, uint32_t>> slots;
	slots.reserve(worn.size());
	for (const auto& item : worn) slots.emplace_back(item.bodyLocation, item.id);
	std::sort(slots.begin(), slots.end());
	std::string census;
	for (const auto& [location, id] : slots) {
		char entry[32]{};
		std::snprintf(entry, sizeof(entry), "%d:%u ", location, id);
		census += entry;
	}
	if (census == g_worn) return;
	g_worn = census;
	AsyncLog::InfoF(context, "[equip] worn %zu -- %s", slots.size(),
		census.empty() ? "NOTHING EQUIPPED" : census.c_str());
}

void SweepDurability(const D2RL::PluginContext* context) {
	const D2RL::InventoryService* inventory = nullptr;
	if (!context || context->QueryService(&inventory)
			!= D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceField(inventory, D2RL::InventoryServiceRequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return;
	// inventory.h permits capturing the player and reading its items on the UI
	// thread. Reacquire the handle so a new session cannot leave stale state.
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) { g_breakage.Reset(); g_worn.clear(); return; }
	std::vector<Vitals::Breakage::Item> worn;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Equipment),
		.reserved = 0};
	// A failed enumeration must not read as "wearing nothing", so the census
	// is only taken when the game actually answered.
	if (inventory->forEachInventoryItem(context, player, &filter, CollectWorn, &worn)
		!= D2RL::Inventory::Result::Success) return;
	LogWorn(context, worn);
	const auto& labels = Strings::Current().equipmentSlots;
	for (const int location : g_breakage.Sweep(worn)) {
		const char* slot = location > 0 && static_cast<size_t>(location) < labels.size()
			? labels[location] : Strings::Current().equipment;
		char phrase[200]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().equipmentBroken, slot);
		AsyncLog::InfoF(context, "[vitals] worn item broken at body location %d", location);
		ScreenReader::Speak(phrase, false);
	}
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	g_command = Command::Report;
	return D2RL::Input::ActionResult::Handled;
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else if (arg == "report") g_command = Command::Report;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	g_statusSupported = NativeStatus::Initialize(context);
	(void)context->RegisterConsoleCommand("access-vitals", Console, "Life, mana and broken equipment announcements: on|off|status|report.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "vitals-report", .displayName = "Say life and mana", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::R, D2RL::Input::Modifier::None}, .callback = OnAction,
			.userData = nullptr};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) {
			g_actions.push_back(handle);
		}
	}
	AsyncLog::Info(context, "[vitals] Life, mana and equipment announcements registered.");
	return true;
}

void Unregister() {
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	g_input = nullptr;
	g_context = nullptr;
	g_statusSupported = false;
	g_command = Command::None;
	Reset();
}

void Reset() {
	g_alerts.Reset();
	g_orbAvailability.Reset();
	g_poison.Reset();
	g_mercenary.Reset();
	g_breakage.Reset();
	g_worn.clear();
	g_hintedMask = 0;
	g_nextOrb = 0;
	g_nextDurability = 0;
}

namespace {
// BACK+X on the controller, while no menu is up (the user, 2026-09-21: BACK
// alone became the modifier, "back+X: speak life+mana as it does now"). The
// user reports BACK does nothing in play, so holding it costs the game
// nothing; the X press is kept from the game during play by pad_filter.h.
bool BackRequested() {
	// Always taken, so a press made in a menu cannot be heard later.
	const bool pressed = InputDirection::TakeBackWith(InputDirection::FaceButton::X);
	return pressed && RadarUi::InPlay();
}
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	if (command == Command::Report || BackRequested()) Report(context);
	if (command == Command::On || command == Command::Off) {
		g_enabled = command == Command::On;
		Reset();
	}
	// Report answers with the reading itself, so it must not also say whether
	// the automatic announcements are on.
	if (command == Command::On || command == Command::Off || command == Command::Status)
		ScreenReader::Speak(g_enabled ? Strings::Current().vitalsEnabled : Strings::Current().vitalsDisabled, true);
	if (!g_enabled || !NativeWidgets::IsAvailable()) return;
	const uint64_t now = GetTickCount64();
	if (now >= g_nextOrb) {
		g_nextOrb = now + OrbSampleMs;
		const NativeMemory::ReadScope scope;
		if (!NativeWidgets::FindPanel(HudPanel)) {
			// Outside a game the orbs do not exist: take a fresh baseline on
			// the next character instead of comparing against the last one.
			g_alerts.Reset();
			g_orbAvailability.Reset();
			g_poison.Reset();
			g_mercenary.Reset();
			g_hintedMask = 0;
		} else {
			const int life = ReadPercent(LifePath), mana = ReadPercent(ManaPath);
			Announce(g_alerts.Update(life, mana));
			HintOrbText(g_orbAvailability.Update(life >= 0, mana >= 0));
			if (g_statusSupported) {
				const auto changed = g_poison.Update(NativeStatus::PlayerPoisoned());
				if (changed) ScreenReader::Speak(*changed ? Strings::Current().playerPoisoned
					: Strings::Current().playerPoisonCleared, false);
				const auto mercenary = NativeStatus::MercenaryLife();
				const int percent = mercenary
					? Vitals::Percent(mercenary->currentLife, mercenary->maximumLife) : -1;
				const int announced = g_mercenary.Update(
					mercenary ? std::optional<uint32_t>(mercenary->id) : std::nullopt, percent);
				if (announced >= 0) ScreenReader::Speak(
					FormattedPercent(Strings::Current().mercenaryLife, announced), false);
			}
		}
	}
	if (now >= g_nextDurability) {
		g_nextDurability = now + DurabilitySampleMs;
		const NativeMemory::ReadScope scope;
		SweepDurability(context);
	}
}

}
