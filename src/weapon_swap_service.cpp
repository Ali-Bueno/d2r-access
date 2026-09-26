#include "weapon_swap_service.h"
#include "async_log.h"
#include "equipment_census.h"
#include "native_weapon_set.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "weapon_swap.h"
#include <windows.h>
#include <atomic>
#include <string>
#include <vector>

namespace D2RAccess::WeaponSwapService {
namespace {
constexpr uint64_t SampleMs = 100; // a swap answers a button press
std::atomic<bool> g_enabled = true;
bool g_supported = false;
uint64_t g_nextSample = 0;
WeaponSwap::Tracker g_tracker;

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") g_enabled = true;
	else if (args == "off") g_enabled = false;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_supported = NativeWeaponSet::Initialize(context);
	g_enabled = RadarConfig::ReadEnabled(context, "[weapon_swap]").value_or(true);
	(void)context->RegisterConsoleCommand("access-weapon-swap", Console, "Name the weapons in hand after a weapon swap: on|off.");
	return g_supported;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_supported || !g_enabled) return;
	const uint64_t now = GetTickCount64();
	if (now < g_nextSample) return;
	g_nextSample = now + SampleMs;
	std::vector<Vitals::Breakage::Item> worn;
	switch (EquipmentCensus::Read(context, worn)) {
	case EquipmentCensus::Result::Unavailable: return;
	// Out of a game the set is stale; the next game starts a new baseline.
	case EquipmentCensus::Result::NoPlayer: g_tracker.Reset(); return;
	case EquipmentCensus::Result::Read: break;
	}
	WeaponSwap::Hands hands;
	const Vitals::Breakage::Item* inHand[2] = {};
	for (const auto& item : worn) {
		if (item.bodyLocation == WeaponSwap::RightHand) { hands.right = item.id; inHand[0] = &item; }
		else if (item.bodyLocation == WeaponSwap::LeftHand) { hands.left = item.id; inHand[1] = &item; }
	}
	const auto second = NativeWeaponSet::SecondSet();
	const auto set = g_tracker.Update(now, second, hands);
	if (!set) return;
	std::vector<std::string> weapons;
	for (const auto* item : inHand)
		if (item) weapons.push_back(EquipmentCensus::Name(*item));
	AsyncLog::InfoF(context, "[weapon-swap] set=%d right=%u left=%u", *set, hands.right, hands.left);
	// It answers the button just pressed, so it takes over from older speech.
	ScreenReader::Speak(WeaponSwap::Phrase(*set, weapons), true);
}
}
