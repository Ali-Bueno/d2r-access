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
constexpr uintptr_t EntrySize = 0x10, TypeOffset = 0x4, IdOffset = 0x8;
constexpr uint32_t LocalPlayers = 8; // the bound the assert helper 0xF1170 puts on the index
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// The first three hold the entries' address unmasked, so a moved array fails them.
constexpr Evidence Checks[] = {
	{0xF1900, 340, 0x5A0A500DE5952B23ULL},  // GetHoveredUnit: +0 gate, +4 type, +8 id -> UnitById
	{0xF2B40, 1195, 0xF11A8AE0085BFE60ULL}, // UpdateHover: the only writer of +4/+8, then +0 = 1
	{0xF24F0, 372, 0x4EF8E6A537788E99ULL},  // ClearHover: +0 = 0 alone
	{LocalIndex, 7, 0x512D0A09BEF62F41ULL},
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
}
