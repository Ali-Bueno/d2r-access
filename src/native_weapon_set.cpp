#include "native_weapon_set.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_memory.h"
#include <windows.h>

namespace D2RAccess::NativeWeaponSet {
namespace {
using NativeMemory::Peek;
// The client's key/value state registry and its bool getter; docs/weapon-swap.md.
constexpr uintptr_t Registry = 0x8461C0, GetBool = 0x11C860, SetOffset = 0x1195C0, LocalSwap = 0x15B920;
// Operands inside the fingerprinted bytes: `mov rax,[rip+disp32]` at Registry+4
// (next instruction at +0xB) and the weapon-set key's `movabs rdx` immediate at SetOffset+0x5D.
constexpr uintptr_t RegistryDisp = Registry + 7, RegistryNext = Registry + 0xB, KeyImmediate = SetOffset + 0x5D;
constexpr GroundItemLayout::Evidence Checks[] = {
	{Registry, 47, 0xE8EABDA7EA343C5BULL},  // returns the registry global
	{GetBool, 182, 0x8533C5F599857F57ULL},  // registry lookup -> bool
	{SetOffset, 203, 0x7585B777DBDA2B53ULL}, // skill-bar set offset; holds the key
	{LocalSwap, 430, 0xE44CD5443648D895ULL}, // moves 4<->11, 5<->12, then toggles the key
};
uintptr_t g_base = 0, g_registry = 0;
uint64_t g_key = 0;
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = g_registry = 0;
	g_key = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "weapon-set", "weapon swap announcements disabled"))
		return false;
	int32_t disp = 0;
	if (!Peek(context->exeBase + RegistryDisp, disp) || !Peek(context->exeBase + KeyImmediate, g_key) || !g_key) return false;
	g_registry = context->exeBase + RegistryNext + disp;
	g_base = context->exeBase;
	return true;
}

std::optional<bool> SecondSet() noexcept {
	uintptr_t registry = 0;
	// The accessor asserts on an empty global, so the global is read first.
	if (!g_base || !Peek(g_registry, registry) || !registry) return std::nullopt;
	__try { return reinterpret_cast<bool(*)(uintptr_t, uint64_t)>(g_base + GetBool)(registry, g_key); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
}
}
