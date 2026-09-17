#include "vitals_service.h"
#include "async_log.h"
#include "localization.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "vitals.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/inventory.h>
#include <D2RLPlugin/item.h>

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace D2RAccess::VitalsService {
namespace {
enum class Command { None, On, Off, Status };

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
std::atomic<Command> g_command{Command::None};
bool g_enabled = true, g_hinted = false;
uint64_t g_nextOrb = 0, g_nextDurability = 0;
Vitals::Meter g_life, g_mana;
Vitals::Breakage g_breakage;

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

auto __cdecl CollectWorn(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* worn = static_cast<std::vector<Vitals::Breakage::Item>*>(userData);
	if (worn && item && item->structSize >= D2RL::Items::ItemInfoRequiredSize)
		worn->push_back({item->runtimeId, item->durability, item->maximumDurability, item->bodyLocation});
	return D2RL::Inventory::IterationAction::Continue;
}

void SweepDurability(const D2RL::PluginContext* context) {
	const D2RL::InventoryServiceV1* inventory = nullptr;
	if (!context || context->QueryService(D2RL::ServiceId::Inventory, D2RL::InventoryServiceV1Version, &inventory)
			!= D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInventoryServiceV1Field(inventory, D2RL::InventoryServiceV1RequiredSize)
		|| !inventory->getLocalPlayer || !inventory->forEachInventoryItem) return;
	// inventory.h permits capturing the player and reading its items on the UI
	// thread. Reacquire the handle so a new session cannot leave stale state.
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (inventory->getLocalPlayer(context, &player) != D2RL::Inventory::Result::Success
		|| player == D2RL::InvalidPlayerHandle) { g_breakage.Reset(); return; }
	std::vector<Vitals::Breakage::Item> worn;
	const D2RL::Inventory::ItemFilter filter{
		.structSize = static_cast<uint32_t>(sizeof(D2RL::Inventory::ItemFilter)),
		.flags = 0,
		.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Equipment),
		.reserved = 0};
	if (inventory->forEachInventoryItem(context, player, &filter, CollectWorn, &worn)
		!= D2RL::Inventory::Result::Success) return;
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

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	(void)context->RegisterConsoleCommand("access-vitals", Console, "Life, mana and broken equipment announcements: on|off|status.");
	AsyncLog::Info(context, "[vitals] Life, mana and equipment announcements registered.");
	return true;
}

void Unregister() {
	g_context = nullptr;
	g_command = Command::None;
	Reset();
}

void Reset() {
	g_life.Reset();
	g_mana.Reset();
	g_breakage.Reset();
	g_hinted = false;
	g_nextOrb = 0;
	g_nextDurability = 0;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	if (command == Command::On || command == Command::Off) {
		g_enabled = command == Command::On;
		Reset();
	}
	if (command != Command::None)
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
