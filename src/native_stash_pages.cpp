#include "native_stash_pages.h"
#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include <windows.h>
#include <array>

namespace D2RAccess::NativeStashPages {
namespace {
// bladestorm360, 97cb7c4 (game 3.3.93787, D2RCore 1.3.1-beta); not yet re-measured here.
// BankPanelMessage (0x23BCF0) calls the loader's page replacement at 0x23C2A8
// (offline RVA 0x1C9B20, 317 bytes), taking (panel, forward, singleStep); a
// single step ignores physical Ctrl/Shift. The target is resolved from that
// call, never pinned, because loader RVAs move between releases.
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[]{{0x23BCF0, 1505, 0x0606B958AEEBD4E8ULL}};
constexpr uintptr_t HandlerCallRva = 0x23C2A8;
constexpr size_t BodySize = 317;
constexpr uint64_t BodyHash = 0x11116ABEB33A3AAAULL;
// Inside that body: indirect calls to the page-count and season getters.
constexpr uintptr_t CountCallOffset = 0x2B, SeasonCallOffset = 0x3D;
// Panel fields: current page per season (0 current, 1 previous), 8 bytes each.
constexpr size_t CurrentPageOffset = 0x168, PageSlotSize = 8;
constexpr uint8_t SeasonCount = 2;
// x86-64 encodings the resolver reads (little-endian FF 25 / FF 15).
constexpr uint8_t CallRel32 = 0xE8;
constexpr uint16_t JumpIndirect = 0x25FF, CallIndirect = 0x15FF;
constexpr size_t RelCallLength = 5, RelOperand = 1, IndirectLength = 6, IndirectOperand = 2;

using PageFn = uint64_t(*)(uintptr_t, bool, bool);
using CountFn = uint64_t(*)(uintptr_t);
using SeasonFn = uint8_t(*)(uintptr_t);
PageFn g_page = nullptr;
CountFn g_count = nullptr;
SeasonFn g_season = nullptr;
const D2RL::PluginContext* g_context = nullptr;

uintptr_t Relative(uintptr_t instruction, size_t length, size_t operand) {
	int32_t displacement = 0;
	return NativeMemory::Peek(instruction + operand, displacement) ? instruction + length + displacement : 0;
}
uintptr_t Indirect(uintptr_t instruction) {
	uint16_t opcode = 0;
	uintptr_t function = 0;
	if (!NativeMemory::Peek(instruction, opcode) || opcode != CallIndirect) return 0;
	const auto slot = Relative(instruction, IndirectLength, IndirectOperand);
	return slot && NativeMemory::Peek(slot, function) ? function : 0;
}
int Run(uintptr_t panel, int direction, int pages) noexcept {
	__try {
		const uint64_t count = g_count(panel);
		const uint8_t season = g_season(panel);
		const size_t field = CurrentPageOffset + season * PageSlotSize;
		uint64_t current = 0;
		if (season >= SeasonCount || !NativeMemory::Peek(panel + field, current)) return -1;
		const int steps = Steps(current, count, direction, pages);
		for (int i = 0; i < steps; ++i) {
			const uint64_t expected = direction < 0 ? current - 1 : current + 1;
			(void)g_page(panel, direction > 0, true);
			if (!NativeMemory::Peek(panel + field, current) || current != expected) return -1;
		}
		return current < count ? steps : -1;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
}
bool Initialize(const D2RL::PluginContext* context) {
	g_page = nullptr; g_count = nullptr; g_season = nullptr; g_context = context;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks,
		"stash-pages", "native shared page commands unavailable")) return false;
	const auto call = context->exeBase + HandlerCallRva;
	uint8_t opcode = 0;
	if (!NativeMemory::Peek(call, opcode) || opcode != CallRel32) return false;
	auto target = Relative(call, RelCallLength, RelOperand);
	uint16_t jump = 0;
	if (NativeMemory::Peek(target, jump) && jump == JumpIndirect) {
		const auto slot = Relative(target, IndirectLength, IndirectOperand);
		if (!slot || !NativeMemory::Peek(slot, target)) return false;
	}
	// Refuse an unfamiliar loader body rather than guess its arguments.
	std::array<uint8_t, BodySize> bytes{};
	if (!target || !NativeMemory::Copy(target, bytes.data(), bytes.size()) || LayoutCheck::Hash(bytes) != BodyHash) {
		AsyncLog::Warn(context, "[stash-pages] Page command replacement differs; shared page keys unavailable.");
		return false;
	}
	const auto count = Indirect(target + CountCallOffset), season = Indirect(target + SeasonCallOffset);
	if (!count || !season) return false;
	g_page = reinterpret_cast<PageFn>(target);
	g_count = reinterpret_cast<CountFn>(count);
	g_season = reinterpret_cast<SeasonFn>(season);
	AsyncLog::Info(context, "[stash-pages] Verified modifier-independent native page commands.");
	return true;
}
int Change(uintptr_t panel, int direction, int pages) {
	if (!g_page || !panel || (direction != -1 && direction != 1) || (pages != OnePage && pages != TenPages)) return -1;
	const int moved = Run(panel, direction, pages);
	if (moved < 0) AsyncLog::Warn(g_context, "[stash-pages] Native page change refused or unreadable.");
	return moved;
}
}
