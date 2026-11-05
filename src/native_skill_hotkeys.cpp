#include "native_skill_hotkeys.h"
#include "async_log.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_hover.h"
#include "native_skill_bar.h"

#include <windows.h>
#include <intrin.h>
#include <array>
#include <mutex>

namespace D2RAccess::NativeSkillHotkeys {
namespace {
// D2R 3.3.93787 (code identical on 3.3.94995), bladestorm360's disassembly; docs/skill-hotkeys.md.
// Select(slot, defaultAttack) is where the skill handlers end; QuickCast(manager, player, slot, pressed)
// is the shared quick-cast path. Both return whether the game accepted the call.
constexpr uintptr_t Select = 0x118D50, QuickCast = 0x149550;
// Return addresses inside the keyboard dispatcher (0x11FF30) and the quick-cast key handler (0x122F30):
// the call into Select / QuickCast from a key, not from the game restoring a skill or a pad button.
constexpr uintptr_t KeyReturn = 0x120373, QuickReturn = 0x122F6A;
constexpr GroundItemLayout::Evidence Checks[] = {
	{Select, 445, 0x52F48A8FE16A96BEULL},
	{QuickCast, 1913, 0x818AADA16F9AE2ECULL},
	{0x11FF30, 1126, 0x3A457989987915C0ULL}, // pins KeyReturn
	{0x122F30, 74, 0x23DC6980027C458CULL},   // pins QuickReturn and press/release
};
// Entry bytes the inline hooks overwrite (checked by the loader before it patches).
constexpr uint8_t SelectPrefix[] = {0x40, 0x53, 0x55, 0x48, 0x83, 0xEC, 0x38, 0x48, 0x63, 0xD9, 0x0F, 0xB6, 0xEA, 0x83, 0xFB, 0x0F};
constexpr uint8_t QuickPrefix[] = {0x40, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0xC8, 0xFD, 0xFF, 0xFF};
constexpr size_t QueueSize = 16; // one per binding slot; distinct presses of one skill all survive
using SelectFn = bool(*)(int, bool);
using QuickFn = bool(*)(uintptr_t, uint32_t, int, bool);
SelectFn g_select = nullptr;
QuickFn g_quick = nullptr;
uintptr_t g_base = 0;
const D2RL::PluginContext* g_context = nullptr;
bool (*g_aim)(int) = nullptr;
void (*g_cast)(int) = nullptr;
int g_aimedSlot = -1; // the quick cast key whose target the game is holding
std::mutex g_mutex;
std::array<Event, QueueSize> g_events;
size_t g_count = 0;

void Capture(int slot, bool quickCast) {
	const auto skill = NativeSkillBar::KeyboardSkill(slot);
	if (!skill) return;
	std::lock_guard lock(g_mutex);
	if (g_count < g_events.size()) g_events[g_count++] = {slot, *skill, quickCast};
}

bool ObserveSelect(int slot, bool defaultAttack) {
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const bool accepted = g_select(slot, defaultAttack);
	if (accepted && !defaultAttack && caller == g_base + KeyReturn) Capture(slot, false);
	return accepted;
}

bool ObserveQuick(uintptr_t manager, uint32_t player, int slot, bool pressed) {
	const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
	const auto skill = pressed && g_aim ? NativeSkillBar::KeyboardSkill(slot) : std::nullopt;
	const bool aimed = skill && g_aim(*skill);
	if (aimed) g_aimedSlot = slot;
	const bool accepted = g_quick(manager, player, slot, pressed);
	if (accepted && skill && g_cast) g_cast(*skill);
	// The cast goes out on a later frame: the target stays until the key comes up.
	if (!pressed && slot == g_aimedSlot) {
		g_aimedSlot = -1;
		NativeHover::Release();
	}
	if (pressed) AsyncLog::InfoF(g_context, "[skill-hotkey] quick cast slot=%d skill=%d accepted=%d aimed=%d", slot, skill.value_or(-1),
		accepted ? 1 : 0, aimed ? 1 : 0);
	if (accepted && pressed && caller == g_base + QuickReturn) Capture(slot, true);
	return accepted;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_select = nullptr;
	g_quick = nullptr;
	g_base = 0;
	if (!context || !NativeSkillBar::Ready()) {
		if (context) AsyncLog::Info(context, "[skill-hotkey] Skill reading unavailable; keyboard skill announcements are off.");
		return false;
	}
	if (!LayoutCheck::Verify(context, context->exeBase, Checks, "skill-hotkey", "keyboard skill announcements are off")) return false;
	g_base = context->exeBase;
	g_context = context;
	const bool select = context->InstallInlineHook(Select, SelectPrefix, sizeof(SelectPrefix), &ObserveSelect, &g_select) && g_select;
	const bool quick = context->InstallInlineHook(QuickCast, QuickPrefix, sizeof(QuickPrefix), &ObserveQuick, &g_quick) && g_quick;
	AsyncLog::InfoF(context, "[skill-hotkey] Observers installed: selection=%d quick-cast=%d.", select ? 1 : 0, quick ? 1 : 0);
	return select || quick;
}

void SetQuickAim(bool (*aim)(int skill), void (*cast)(int skill)) {
	g_aim = aim;
	g_cast = cast;
}

std::vector<Event> Take() {
	std::lock_guard lock(g_mutex);
	std::vector<Event> events(g_events.begin(), g_events.begin() + g_count);
	g_count = 0;
	return events;
}
}
