#include "native_eclipse.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"

#include <atomic>

namespace D2RAccess::NativeEclipse {
namespace {
// D2R 3.3.94995, read in the image, not yet measured live: a pointer to the act the client holds,
// its environment at +8, and there the flag the game tests for darkness. Evidence: docs/quests.md.
constexpr uintptr_t ActRva = 0x2A233B0, ActEnvironment = 0x8, EnvironmentDark = 0x34;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x351FD0, 51, 0x1968C19BD72EA8F2ULL},  // the game's own test: cmp [env+0x34], 0
	{0x2EF120, 5, 0x7156601FF83CC292ULL},   // the act's environment at +8
	{0x12CB50, 217, 0x12F4F50965983667ULL}, // the darkness packet's handler, which reads the act global
};
const D2RL::PluginContext* g_context = nullptr;
std::atomic<uintptr_t> g_base{0};
std::atomic<int> g_logged{-1}; // the last flag logged, -1 for none
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_context = context;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "eclipse", "the darkness not read")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[quest] Verified eclipse reader.");
	return true;
}

std::optional<Sky> Read() {
	if (!g_base.load()) return std::nullopt;
	const NativeMemory::ReadScope scope;
	uintptr_t act = 0, environment = 0;
	int32_t flag = 0;
	if (!NativeMemory::Peek(g_base.load() + ActRva, act) || !act || !NativeMemory::Peek(act + ActEnvironment, environment)
		|| !environment || !NativeMemory::Peek(environment + EnvironmentDark, flag)) return std::nullopt;
	const bool dark = flag != 0;
	if (g_logged.exchange(dark) != static_cast<int>(dark)) AsyncLog::InfoF(g_context, "[quest] eclipse flag=%d", flag);
	return Sky{act, dark};
}
}
