#include "native_trade.h"
#include "async_log.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_memory.h"
#include <windows.h>
#include <atomic>

namespace D2RAccess::NativeTrade {
namespace {
using NativeMemory::Peek;
// Trade module globals, written by the snapshot parser 0x1EDE70, the outgoing
// request 0x1ED7E0 and the reset 0x1F32C0; docs/trade.md.
constexpr uintptr_t PendingId = 0x2A96AB0, ActiveId = 0x2A96AB8, Partner = 0x2A96AC0, PartnerGold = 0x2A96AC4,
	PartnerAccepted = 0x2A96AC8, PartnerItemCount = 0x2A96AD8, WeAccepted = 0x2A96AF0, Requester = 0x2A96B10;
constexpr int32_t NoPartner = -1;
// Party roster lookup by unit id (0x136200) and the name its getter 0x136300 returns.
constexpr uintptr_t RosterFind = 0x136200;
constexpr size_t RosterName = 0xFF, NameBytes = 64;
// S2C 0x2E subtype 3, the only reply that ends a trade with the exchange made.
constexpr uintptr_t CompletedHandler = 0x1EDC50;
constexpr uint8_t CompletedPrefix[] = {0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0xF9};
constexpr GroundItemLayout::Evidence Checks[] = {
	{0x1EDE70, 812, 0xA63BEFFE970BCE2DULL}, // snapshot parser: ids, partner, gold, accepted, items
	{0x1F32C0, 236, 0x2297E19E075771DAULL}, // session reset: ids 0, units -1
	{0x1ED7E0, 689, 0xF85A25DA150CDF76ULL}, // outgoing request: pending id, partner, requester byte
	{0x1EFF60, 183, 0x29605BDFB7BD11C2ULL}, // partner name = roster(Partner)
	{RosterFind, 61, 0x29DCD4F96F16A6D6ULL},
	{0x136300, 64, 0x509A4708BC35FC8CULL},  // roster entry -> entry + 0xFF
	{CompletedHandler, 269, 0xBEE6AF7FCD51298AULL},
};
uintptr_t g_base = 0;
std::atomic<bool> g_hooked{false}, g_completed{false};
void* g_original = nullptr;

// Passes every register argument and the result through, so the stock handler runs unchanged.
uintptr_t __cdecl OnCompleted(uintptr_t packet, uintptr_t b, uintptr_t c, uintptr_t d) noexcept {
	g_completed.store(true, std::memory_order_release);
	return reinterpret_cast<uintptr_t(__cdecl*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t)>(g_original)(packet, b, c, d);
}

uintptr_t FindRosterEntry(int32_t unit) noexcept {
	__try { return reinterpret_cast<uintptr_t(*)(int32_t)>(g_base + RosterFind)(unit); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "trade", "trade announcements disabled")) return false;
	g_base = context->exeBase;
	return true;
}

bool InstallCompletionHook(const D2RL::PluginContext* context) {
	if (!g_base || g_hooked.load()) return g_hooked.load();
	if (!context->InstallInlineHook(CompletedHandler, CompletedPrefix, sizeof(CompletedPrefix),
		reinterpret_cast<void*>(&OnCompleted), &g_original)) {
		AsyncLog::Warn(context, "[trade] Completion hook unavailable; a finished trade is reported as ended.");
		return false;
	}
	g_hooked.store(true);
	return true;
}

bool CompletionHooked() { return g_hooked.load(); }

std::optional<Trade::State> Read() noexcept {
	if (!g_base) return std::nullopt;
	uint32_t pending = 0, active = 0, partnerGold = 0, partnerAccepted = 0, weAccepted = 0;
	int32_t partner = NoPartner;
	uint64_t items = 0;
	uint8_t requester = 0;
	if (!Peek(g_base + PendingId, pending) || !Peek(g_base + ActiveId, active) || !Peek(g_base + Partner, partner)
		|| !Peek(g_base + PartnerGold, partnerGold) || !Peek(g_base + PartnerAccepted, partnerAccepted)
		|| !Peek(g_base + PartnerItemCount, items) || !Peek(g_base + WeAccepted, weAccepted)
		|| !Peek(g_base + Requester, requester)) return std::nullopt;
	Trade::State state;
	state.pending = pending != 0;
	state.active = active != 0;
	state.requester = requester != 0;
	state.partner = partner;
	state.partnerGold = partnerGold;
	state.partnerAccepted = partnerAccepted != 0;
	state.partnerItems = items;
	state.weAccepted = weAccepted != 0;
	return state;
}

std::string PartnerName() {
	int32_t partner = NoPartner;
	if (!g_base || !Peek(g_base + Partner, partner) || partner == NoPartner) return {};
	const uintptr_t entry = FindRosterEntry(partner);
	char name[NameBytes]{};
	if (!entry || !NativeMemory::Copy(entry + RosterName, name, sizeof(name) - 1)) return {};
	return name;
}

bool TakeCompleted() { return g_completed.exchange(false, std::memory_order_acq_rel); }
}
