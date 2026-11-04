#include "native_pad_approach.h"

#include "async_log.h"
#include "layout_check.h"
#include "pad_approach.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace D2RAccess::NativePadApproach {
namespace {
// D2R 3.3.93787, READ in the image; docs/input-commands.md.
constexpr uintptr_t ControllerManager = 0x144640, DockingTest = 0x190D40, LocalIndex = 0x8B2D0, LocalPlayer = 0x9A480;
constexpr uintptr_t TargetScore = 0x18AF30, ClearSlots = 0x18B8C0, InteractReach = 0x18AED0;
constexpr uintptr_t SlotField = 0xC20, SlotStride = 12;
constexpr uint32_t MonsterUnit = 1; // an NPC is one; objects and items are not
// The primary score table, its distances and weights, category 5.
constexpr uintptr_t NpcReachField = 0xD40 + 0x60 + 5 * sizeof(float), NpcWeightField = 0xD40 + 0xC0 + 5 * sizeof(float);
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{ControllerManager, 123, 0x7C0E28C9AED12207ULL},
	{DockingTest, 250, 0xF08F71A5BADDE351ULL},
	{LocalIndex, 7, 0x512D0A09BEF62F41ULL},
	{LocalPlayer, 165, 0x754EA91CD47DAFC1ULL},
	{TargetScore, 1045, 0x6D5298A01553367BULL},
	{ClearSlots, 505, 0x3DB9B857B7D9BF3BULL},
	{InteractReach, 95, 0xDAB61EF010ECD325ULL},
};
// Both hooked functions open with the same three register saves.
constexpr uint8_t HookPrefix[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x20};
constexpr uint32_t LocalPlayers = 8; // bound in native 0x9A480
constexpr ULONGLONG CheckMs = 100;   // under one subtile at a run: the stash is at hand before the press
constexpr int LoggedChanges = 4;     // the first raise and fall prove the field; walking repeats them

using DockingFn = bool(*)(void* manager, void* player, void* target);
using InReachFn = bool(*)(void* player, const uint32_t* unit, int32_t docking);

uintptr_t g_base = 0;
DockingFn g_docks = nullptr;
InReachFn g_inReach = nullptr;
std::atomic<bool> g_enabled{false};
std::atomic<ULONGLONG> g_nextCheck{0};
PadApproach::State g_state;
int g_logsLeft = LoggedChanges;

// The game walks only to a target within reach plus its docking distance and in a clear
// line. A click has neither limit.
bool Docks(void* manager, void* player, void* target) {
	return g_enabled.load() || g_docks(manager, player, target);
}

// Interaction drops a target past reach plus docking, about 5 subtiles. Only NPCs skip it:
// everything else keeps the game's rule.
bool InReach(void* player, const uint32_t* unit, int32_t docking) {
	return (g_enabled.load() && *unit == MonsterUnit) || g_inReach(player, unit, docking);
}

struct Change { PadApproach::NpcTargeting from, to; bool written = false; };

// Only with a character in play: the game loads its value before that.
Change Apply(bool enabled) noexcept {
	Change change;
	__try {
		const auto index = reinterpret_cast<uint32_t(*)()>(g_base + LocalIndex)();
		if (index >= LocalPlayers || !reinterpret_cast<uintptr_t(*)(uint32_t)>(g_base + LocalPlayer)(index)) return change;
		const uintptr_t manager = reinterpret_cast<uintptr_t(*)()>(g_base + ControllerManager)();
		std::array<int32_t, PadApproach::Slots> ids;
		for (size_t slot = 0; slot < ids.size(); ++slot) ids[slot] = *reinterpret_cast<const int32_t*>(manager + SlotField + slot * SlotStride);
		auto* const reach = reinterpret_cast<float*>(manager + NpcReachField);
		auto* const weight = reinterpret_cast<float*>(manager + NpcWeightField);
		change.from = {*reach, *weight};
		if (const auto next = PadApproach::Next(g_state, enabled && PadApproach::NothingElseAtHand(ids), change.from)) {
			*reach = next->reach;
			*weight = next->weight;
			change.to = *next;
			change.written = true;
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) { change.written = false; }
	return change;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_docks = nullptr;
	g_inReach = nullptr;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "pad-approach", "the pad keeps the game's reach")) return false;
	if (!context->InstallInlineHook(DockingTest, HookPrefix, sizeof(HookPrefix), &Docks, &g_docks) || !g_docks
		|| !context->InstallInlineHook(InteractReach, HookPrefix, sizeof(HookPrefix), &InReach, &g_inReach) || !g_inReach) {
		AsyncLog::Info(context, "[pad-approach] A hook failed: NPCs keep the game's reach.");
		return false;
	}
	g_base = context->exeBase;
	AsyncLog::Info(context, "[pad-approach] Verified.");
	return true;
}

bool Enabled() { return g_enabled.load(); }

void SetEnabled(bool on) {
	g_enabled = on;
	g_nextCheck = 0;
}

void Tick(const D2RL::PluginContext* context) {
	const bool enabled = g_enabled.load();
	if (!g_base || (!enabled && !g_state.raised)) return;
	const ULONGLONG now = GetTickCount64();
	if (now < g_nextCheck.load()) return;
	g_nextCheck = now + CheckMs;
	const Change change = Apply(enabled);
	if (change.written && g_logsLeft > 0) {
		--g_logsLeft;
		AsyncLog::InfoF(context, "[pad-approach] NPC reach %.1f weight %.2f -> %.1f weight %.2f", change.from.reach, change.from.weight, change.to.reach, change.to.weight);
	}
}
}
