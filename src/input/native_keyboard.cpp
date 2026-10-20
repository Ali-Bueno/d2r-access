#include "native_keyboard.h"
#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include <atomic>

namespace D2RAccess::NativeKeyboard {
namespace {
// 3.3.93787: 0x122990 matches the packed key at binding+4, then passes
// the record (action id +0) to BindingDown before invoking the native action.
// Character action zero calls 0x1196F0, toggling UI state 2. Intercepting
// after that action is too late: it has already opened Character.
constexpr uintptr_t BindingDown = 0x11FF30;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{BindingDown, 1126, 0x3A457989987915C0ULL},
	{0x122990, 1436, 0x36BFAF1CC4DDA0F4ULL},
};
constexpr uint8_t Prefix[] = {0x55,0x56,0x41,0x56,0x41,0x57,0x48,0x8D,0xAC,0x24,0xD8,0xFD,0xFF,0xFF};
using Dispatch = bool(*)(uintptr_t, uintptr_t, bool);
Dispatch g_original = nullptr;
std::atomic<Redirect> g_redirect{nullptr};
std::atomic<uint32_t> g_claims{0};

bool Observe(uintptr_t binding, uintptr_t table, bool forced) {
	uint32_t action = ~0u;
	uint16_t key = 0;
	const auto redirect = g_redirect.load(std::memory_order_relaxed);
	bool read = false;
	if (redirect) {
		const NativeMemory::ReadScope scope;
		read = NativeMemory::Peek(binding, action) && NativeMemory::Peek(binding + 4, key);
	}
	if (!read)
		return g_original(binding, table, forced);
	return Route(action, key, forced, [&](uint16_t packed) {
		if (!redirect(packed)) return false;
		g_claims.fetch_add(1, std::memory_order_relaxed);
		return true;
	}, [&] { return g_original(binding, table, forced); });
}
}
bool Initialize(const D2RL::PluginContext* context, Redirect redirect) {
	g_redirect = nullptr;
	if (!context || !redirect || !LayoutCheck::Verify(context, context->exeBase, Checks,
		"keyboard-native", "native Character binding protection unavailable")) return false;
	if (!context->InstallInlineHook(BindingDown, Prefix, sizeof(Prefix), &Observe, &g_original) || !g_original) return false;
	g_redirect = redirect;
	AsyncLog::Info(context, "[keyboard-native] Verified pre-dispatch Character binding protection.");
	return true;
}
void Reset() { g_redirect = nullptr; g_claims = 0; }
uint32_t TakeClaims() { return g_claims.exchange(0); }
}
