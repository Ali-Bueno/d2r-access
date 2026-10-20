#include "native_stash_pages.h"
#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include <windows.h>
#include <array>

namespace D2RAccess::NativeStashPages {
namespace {
// BankPanelMessage SharedStashLeft/Right (0x23BCF0) call the loader's page
// replacement at 0x23C2A8. Resolve that existing call/thunk rather than pinning
// a D2RCore RVA that moves between loader releases. The verified replacement
// (1.3.1-beta, offline RVA 0x1C9B20) has a native single-step/wrap argument:
// true bypasses physical Ctrl/Shift; false selects 10/5/1. We clamp beforehand
// so its single-step mode cannot wrap, then issue at most ten native commands.
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[]{{0x23BCF0,1505,0x0606B958AEEBD4E8ULL}};
constexpr size_t BodySize = 317;
constexpr uint64_t BodyHash = 0x11116ABEB33A3AAAULL;
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
	if (!NativeMemory::Peek(instruction, opcode) || opcode != 0x15FF) return 0;
	const auto slot = Relative(instruction, 6, 2);
	return slot && NativeMemory::Peek(slot, function) ? function : 0;
}
bool Run(uintptr_t panel, int direction, int pages) noexcept {
	__try {
		const uint64_t count = g_count(panel);
		const uint8_t season = g_season(panel);
		uint64_t current = 0;
		if (season > 1 || !NativeMemory::Peek(panel + 0x168 + season * 8, current)) return false;
		const int steps = Steps(current, count, direction, pages);
		for (int i = 0; i < steps; ++i) {
			const uint64_t expected = direction < 0 ? current - 1 : current + 1;
			(void)g_page(panel, direction > 0, true);
			if (!NativeMemory::Peek(panel + 0x168 + season * 8, current) || current != expected) return false;
		}
		return current < count;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool Initialize(const D2RL::PluginContext* context) {
	g_page = nullptr; g_count = nullptr; g_season = nullptr; g_context = context;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks,
		"stash-pages", "native shared page commands unavailable")) return false;
	const auto call = context->exeBase + 0x23C2A8;
	uint8_t opcode = 0;
	if (!NativeMemory::Peek(call, opcode) || opcode != 0xE8) return false;
	auto target = Relative(call, 5, 1);
	uint16_t jump = 0;
	if (NativeMemory::Peek(target, jump) && jump == 0x25FF) {
		const auto slot = Relative(target, 6, 2);
		if (!slot || !NativeMemory::Peek(slot, target)) return false;
	}
	// The small loader wrapper is read once at initialization. Refuse an
	// unfamiliar implementation instead of guessing argument/layout semantics.
	std::array<uint8_t, BodySize> bytes{};
	if (!target || !NativeMemory::Copy(target, bytes.data(), bytes.size()) || LayoutCheck::Hash(bytes) != BodyHash) {
		AsyncLog::Warn(context, "[stash-pages] Page command replacement differs; shortcut unavailable.");
		return false;
	}
	const auto count = Indirect(target + 0x2B), season = Indirect(target + 0x3D);
	if (!count || !season) return false;
	g_page = reinterpret_cast<PageFn>(target);
	g_count = reinterpret_cast<CountFn>(count);
	g_season = reinterpret_cast<SeasonFn>(season);
	AsyncLog::Info(context, "[stash-pages] Verified modifier-independent native page commands.");
	return true;
}
bool Change(uintptr_t panel, int direction, int pages) {
	if (!g_page || !panel || (direction != -1 && direction != 1) || (pages != 1 && pages != 5 && pages != 10)) return false;
	if (Run(panel, direction, pages)) return true;
	AsyncLog::Warn(g_context, "[stash-pages] Native page change refused or unreadable.");
	return false;
}
}
