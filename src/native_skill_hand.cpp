#include "native_skill_hand.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"

#include <windows.h>

namespace D2RAccess::NativeSkillHand {
namespace {
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/widgets (notes.txt, D).
//
// The HUD's skill buttons (SkillSelectButtonWidget, OnClick 0x2389F0) write
// their side to the UI registry under one key and open UI state 3, the panel
// SkillSelect: 1 for LeftSkillButton, 2 for RightSkillButton. The handler of
// the game's key S (key action 13, 0x119F30) writes 2 and toggles the state.
// 0x238240 takes no argument and gives the side back: it looks the key up in
// the registry (0x8461C0) and reads the value, 0 when the key was never set.
constexpr uintptr_t ChosenSide = 0x238240;
constexpr uintptr_t ButtonClick = 0x2389F0;
constexpr size_t ClickSlot = 0xC0, SideOffset = 0xB88;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{ChosenSide, 143, 0xB7370C1DF57D057CULL},
	{0x8461C0, 47, 0xE8EABDA7EA343C5BULL},   // the UI registry
	{0x0BFDC0, 35, 0x6C5786B0EFE2D0F7ULL},   // asked only of a node without a value
	{ButtonClick, 546, 0xB9B54A9E9333B2D6ULL}, // a skill button's click
	{0x119F30, 54, 0x4DE9613B2F95CC5CULL},   // the game's key for the right hand's list
};
constexpr uint32_t Left = 1, Right = 2;
uintptr_t g_base = 0;
using ClickFn = void(*)(uintptr_t);
ClickFn g_click = nullptr;

bool ClickLeft(uintptr_t button) noexcept {
	// This is the same UI-thread callback a mouse release on the HUD invokes.
	// Its native side switch opens/closes the list or switches it from right
	// to left. Do not queue an OS click: gameplay correctly retires menu input.
	__try {
		g_click(button);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

Hand Call() noexcept {
	__try {
		const uint32_t side = reinterpret_cast<uint32_t(*)()>(g_base + ChosenSide)();
		return side == Left ? Hand::Left : side == Right ? Hand::Right : Hand::None;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return Hand::None; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_click = nullptr;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "skill-hand", "the hand of the list of skills is not said")) return false;
	g_base = context->exeBase;
	g_click = reinterpret_cast<ClickFn>(g_base + ButtonClick);
	AsyncLog::Info(context, "[skill-hand] Verified.");
	return true;
}

bool Available() { return g_base != 0; }

Hand Chosen() { return Available() ? Call() : Hand::None; }

bool ActivateLeft(uintptr_t button) noexcept {
	if (!Available() || !g_click || !button) return false;
	{
		// Finish this local validation pass before invoking the native action.
		// The virtual slot proves this is the verified button type; the side
		// rejects right-hand HUD buttons and skill/quick-bind list clones.
		NativeMemory::ReadScope scope;
		uintptr_t table = 0, click = 0;
		uint32_t side = 0;
		if (!NativeMemory::Peek(button, table) || !table
			|| !NativeMemory::Peek(table + ClickSlot, click)
			|| click != reinterpret_cast<uintptr_t>(g_click)
			|| !NativeMemory::Peek(button + SideOffset, side) || side != Left) return false;
	}
	return ClickLeft(button);
}
}
