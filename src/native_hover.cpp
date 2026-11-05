#include "native_hover.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"

#include <windows.h>

namespace D2RAccess::NativeHover {
namespace {
// D2R 3.3.93787, READ in the image (re/D2R.rebuilt.exe, 2026-10-14) and not yet
// measured in the game. The world target nameplates (0x880550) name the unit
// GetHoveredUnit gives (0xF1900): entry LocalIndex() of an array of eight at
// 0x2A41630, 0x10 bytes each. +0 is set to 1 only by UpdateHover (0xF2B40),
// after it writes the type at +4 and the id at +8, and cleared alone by
// ClearHover (0xF24F0), so type and id outlive it and +0 decides. Whether the
// controller's auto-target fills it as the mouse does is not measured.
constexpr uintptr_t Entries = 0x2A41630, LocalIndex = 0x8B2D0;
// LockHover(index, unit) runs the game's hover filter, then keeps the entry from being
// rewritten until UnlockHover(index); the pad and the game's own tests use them. docs/skill-hotkeys.md
constexpr uintptr_t LockHover = 0xF1A60, UnlockHover = 0xF15C0, UnitById = 0x9A5D0;
constexpr uintptr_t EntrySize = 0x10, TypeOffset = 0x4, IdOffset = 0x8;
constexpr uint32_t LocalPlayers = 8; // the bound the assert helper 0xF1170 puts on the index
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// The first three hold the entries' address unmasked, so a moved array fails them.
constexpr Evidence Checks[] = {
	{0xF1900, 340, 0x5A0A500DE5952B23ULL},  // GetHoveredUnit: +0 gate, +4 type, +8 id -> UnitById
	{0xF2B40, 1195, 0xF11A8AE0085BFE60ULL}, // UpdateHover: the only writer of +4/+8, then +0 = 1
	{0xF24F0, 372, 0x4EF8E6A537788E99ULL},  // ClearHover: +0 = 0 alone
	{LocalIndex, 7, 0x512D0A09BEF62F41ULL},
	{LockHover, 313, 0xFBDF60DE90F3C0B7ULL},
	{UnlockHover, 75, 0x4C5E2BB285C9D6FFULL},
	{UnitById, 33, 0x673F574CCEEDB47EULL},
};
uintptr_t g_base = 0;

uint32_t Index() noexcept {
	__try { return reinterpret_cast<uint32_t (*)()>(g_base + LocalIndex)(); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return LocalPlayers; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "hover", "a shared label is not told apart by the game's own target"))
		return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[hover] Verified.");
	return true;
}

std::optional<Target> Read() noexcept {
	if (!g_base) return std::nullopt;
	const uint32_t index = Index();
	if (index >= LocalPlayers) return std::nullopt;
	const uintptr_t entry = g_base + Entries + EntrySize * index;
	uint8_t hovered = 0;
	Target target;
	if (!NativeMemory::Peek(entry, hovered) || !hovered || !NativeMemory::Peek(entry + TypeOffset, target.type)
		|| !NativeMemory::Peek(entry + IdOffset, target.id)) return std::nullopt;
	return target;
}

bool Hold(uint32_t type, uint32_t id) noexcept {
	const uint32_t index = g_base ? Index() : LocalPlayers;
	if (index >= LocalPlayers) return false;
	__try {
		const uintptr_t unit = reinterpret_cast<uintptr_t (*)(uint32_t, uint32_t)>(g_base + UnitById)(id, type);
		if (!unit) return false;
		reinterpret_cast<void (*)(uint32_t, uintptr_t)>(g_base + LockHover)(index, unit);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
	// The filter refuses without a word: a corpse, for one (docs/skill-hotkeys.md).
	const auto held = Read();
	return held && held->type == type && held->id == id;
}

void Release() noexcept {
	const uint32_t index = g_base ? Index() : LocalPlayers;
	if (index >= LocalPlayers) return;
	__try { reinterpret_cast<void (*)(uint32_t)>(g_base + UnlockHover)(index); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}
}
