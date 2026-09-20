#include "vitals_service.h"
#include "async_log.h"
#include "input_direction.h"
#include "localization.h"
#include "native_memory.h"
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
bool g_enabled = true, g_hinted = false;
uint64_t g_nextOrb = 0, g_nextDurability = 0;
Vitals::Meter g_life, g_mana;
Vitals::Breakage g_breakage;
// The last census of what the character is wearing. See LogWorn.
std::string g_worn;

int ReadPercent(std::span<const std::string> path) {
	const auto orb = Vitals::ParseOrb(NativeWidgets::TextAt(HudPanel, path));
	return orb ? Vitals::Percent(orb->current, orb->maximum) : -1;
}

void Announce(const char* format, int percent) {
	char phrase[100]{};
	std::snprintf(phrase, sizeof(phrase), format, percent);
	// Only a resource that is already critical takes speech away from the
	// player; an ordinary step waits its turn.
	ScreenReader::Speak(phrase, percent <= Vitals::CriticalPercent);
}

// Says once how to make the orb values readable. Without those two options the
// game writes no number anywhere, so there is nothing for the mod to read.
void HintOrbText() {
	if (g_hinted) return;
	const auto life = Localization::Text(LifeOrbTextOption);
	const auto mana = Localization::Text(ManaOrbTextOption);
	if (life.empty() || mana.empty()) return; // the game's labels are not loaded yet; try again later
	g_hinted = true;
	char phrase[300]{};
	std::snprintf(phrase, sizeof(phrase), Strings::Current().vitalsOrbTextMissing, life.c_str(), mana.c_str());
	AsyncLog::InfoF(g_context, "[vitals] orb values unreadable; asking for \"%s\" and \"%s\"", life.c_str(), mana.c_str());
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
	if (life < 0 && mana < 0) {
		// Neither orb carries a number, so the honest answer to the key is
		// how to make one appear. The hint is otherwise spoken once a
		// session; the player just asked, so re-arm it.
		g_hinted = false;
		HintOrbText();
		return;
	}
	char phrase[100]{};
	std::string text;
	if (life >= 0) {
		std::snprintf(phrase, sizeof(phrase), strings.vitalsLife, life);
		text = phrase;
	}
	if (mana >= 0) {
		std::snprintf(phrase, sizeof(phrase), strings.vitalsMana, mana);
		if (!text.empty()) text += ". ";
		text += phrase;
	}
	// Asked for, so it interrupts: the player is waiting for this answer.
	ScreenReader::Speak(text, true);
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
		ScreenReader::Speak(phrase, true);
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
	g_command = Command::None;
	Reset();
}

void Reset() {
	g_life.Reset();
	g_mana.Reset();
	g_breakage.Reset();
	g_worn.clear();
	g_hinted = false;
	g_nextOrb = 0;
	g_nextDurability = 0;
}

namespace {
// BACK on the controller, while no menu is up. The user asked for a pad
// button for this and reports BACK does nothing in play, so it is free. The
// menu gate is the one c4893cf established: the game opens UISwitcher exactly
// when the menu tree is up, so RadarUi::Playing means the player is in the
// world and BACK belongs to us. Nothing is taken from the game -- the button
// still reaches it exactly as before; this only listens.
bool BackRequested() {
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (RadarUi::Inspect(process == GetCurrentProcessId()) != RadarUi::State::Playing) {
		// Still claim the press, so one made in a menu cannot be heard later.
		(void)InputDirection::TakeBackButton();
		return false;
	}
	return InputDirection::TakeBackButton();
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
			g_life.Reset();
			g_mana.Reset();
		} else {
			const int life = ReadPercent(LifePath), mana = ReadPercent(ManaPath);
			if (life < 0 && mana < 0) HintOrbText();
			if (const int announce = life < 0 ? -1 : g_life.Update(life); announce >= 0)
				Announce(Strings::Current().vitalsLife, announce);
			if (const int announce = mana < 0 ? -1 : g_mana.Update(mana); announce >= 0)
				Announce(Strings::Current().vitalsMana, announce);
		}
	}
	if (now >= g_nextDurability) {
		g_nextDurability = now + DurabilitySampleMs;
		const NativeMemory::ReadScope scope;
		SweepDurability(context);
	}
}

}
