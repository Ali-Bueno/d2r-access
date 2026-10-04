#include "native_skill_hotkeys.h"
#include "async_log.h"
#include "layout_check.h"

#include <windows.h>
#include <intrin.h>
#include <array>
#include <mutex>

namespace D2RAccess::NativeSkillHotkeys {
namespace {
// D2R 3.3.93787, whole-image disassembly: docs/keyboard-actions.md.
// BindingDown calls the key handler at 0x12036F; skill handlers tail-jump
// to Select(slot, defaultAttack). Its bool result reports successful selection.
// Keyboard quick cast calls the shared controller-manager path at 0x122F65,
// with pressed=true on key down and false on release. That also returns bool.
constexpr uintptr_t Select = 0x118D50, QuickCast = 0x149550;
constexpr uintptr_t KeyReturn = 0x120373, QuickReturn = 0x122F6A;
constexpr uintptr_t ControllerMode = 0x77E10, BoundSkill = 0x117D20, BoundLeft = 0x117DE0;
constexpr int Slots = 16; // both getters assert slot <= 15
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{Select, 445, 0x52F48A8FE16A96BEULL},
	{QuickCast, 1913, 0x818AADA16F9AE2ECULL},
	{0x11FF30, 1126, 0x3A457989987915C0ULL}, // pins KeyReturn
	{0x122F30, 74, 0x23DC6980027C458CULL},   // pins QuickReturn and press/release
	{ControllerMode, 41, 0x290F3F607678EB67ULL},
	{BoundSkill, 181, 0xDE7A76B2B41A63B9ULL},
	{BoundLeft, 94, 0x95CC2CD6EF2A233AULL},
};
constexpr uint8_t SelectPrefix[] = {0x40, 0x53, 0x55, 0x48, 0x83, 0xEC, 0x38, 0x48, 0x63, 0xD9, 0x0F, 0xB6, 0xEA, 0x83, 0xFB, 0x0F};
constexpr uint8_t QuickPrefix[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0xC8, 0xFD, 0xFF, 0xFF};
using SelectFn = bool(*)(int, bool);
using QuickFn = bool(*)(uintptr_t, uint32_t, int, bool);
SelectFn g_select = nullptr;
QuickFn g_quick = nullptr;
uintptr_t g_base = 0;
std::mutex g_mutex;
// Bounded producer queue; distinct presses, even of the same skill, survive.
std::array<Event, Slots> g_events;
size_t g_count = 0;

bool Binding(int slot, Event& event) noexcept {
	if (!g_base || slot < 0 || slot >= Slots) return false;
	__try {
		if (reinterpret_cast<bool(*)()>(g_base + ControllerMode)()) return false;
		event.slot = slot;
		event.skill = reinterpret_cast<int(*)(int)>(g_base + BoundSkill)(slot);
		event.left = reinterpret_cast<bool(*)(int)>(g_base + BoundLeft)(slot);
		return event.skill >= 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void Capture(int slot, bool quickCast) {
	Event event;
	if (!Binding(slot, event)) return;
	event.quickCast = quickCast;
	std::lock_guard lock(g_mutex);
	if (g_count < g_events.size()) g_events[g_count++] = event;
}

bool ObserveSelect(int slot, bool defaultAttack) {
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const bool accepted = g_select(slot, defaultAttack);
	// Internal restores, mouse selections and physical controller calls are silent.
	if (accepted && !defaultAttack && caller == g_base + KeyReturn) Capture(slot, false);
	return accepted;
}

bool ObserveQuick(uintptr_t manager, uint32_t player, int slot, bool pressed) {
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const bool accepted = g_quick(manager, player, slot, pressed);
	if (accepted && pressed && caller == g_base + QuickReturn) Capture(slot, true);
	return accepted;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "skill-hotkey", "keyboard skill announcements unavailable")) return false;
	g_base = context->exeBase;
	(void)Take();
	const bool select = context->InstallInlineHook(Select, SelectPrefix, sizeof(SelectPrefix), &ObserveSelect, &g_select) && g_select;
	const bool quick = context->InstallInlineHook(QuickCast, QuickPrefix, sizeof(QuickPrefix), &ObserveQuick, &g_quick) && g_quick;
	AsyncLog::InfoF(context, "[skill-hotkey] Verified native observers: selection=%d quick-cast=%d.", select ? 1 : 0, quick ? 1 : 0);
	return select && quick;
}

std::vector<Event> Take() {
	std::lock_guard lock(g_mutex);
	std::vector<Event> events(g_events.begin(), g_events.begin() + g_count);
	g_count = 0;
	return events;
}
}
