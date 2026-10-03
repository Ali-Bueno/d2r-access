#include "native_belt_fill.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_ui.h"

#include <windows.h>

namespace D2RAccess::NativeBeltFill {
namespace {
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/belt.
//
// The belt's message handler (0x2283E0, vtable 0x1CE2FA0 +0x20) ends the
// controller's hold with
//   if (!CanPopulateBelt(belt)) return;
//   owner = GetOwner(belt); inventory = GetInventory(owner, file, line);
//   if (CursorItem(inventory)) return;
//   if (Plan(owner, ids, cells, slots, &count) && count > 0) {
//       Send(ids, cells, slots, count); PlaySound(0xEB, 0, 0, 0, 0); }
// with ids and cells 16 dwords each and slots 16 bytes. The controller check
// (0x77E10) is the handler's first call and is in none of these functions.
// Plan keeps each potion of the inventory that the belt's own slot finder
// (0x3873E0) has a free slot for. Send builds message 0x5D, 98 bytes, in a
// buffer on its stack that holds 16 entries. The server (0x4AC160) finds the
// slots again with the same finder and refuses the whole message, before any
// potion moves, if one entry differs.
constexpr uintptr_t CanPopulateBelt = 0x226060, GetOwner = 0x2A7810, GetInventory = 0x34A360, CursorItem = 0x388A70;
constexpr uintptr_t Plan = 0x386FF0, Send = 0x0ECA70, PlaySound = 0x1A0780;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{CanPopulateBelt, 371, 0xDA6BF245D38C0CF2ULL},
	{GetOwner, 92, 0xA9B7A64D3EDBDE8BULL},
	{GetInventory, 165, 0xCC327F6CEB48B813ULL},
	{CursorItem, 95, 0x6B5CBD1203A175EEULL},
	{Plan, 998, 0x04F99E81876D499DULL},
	{Send, 150, 0x9C7543FD2F9F241EULL},
	{PlaySound, 1143, 0x8185506434EF72B4ULL},
	{0x2283E0, 747, 0xADD0951C0DFEEC67ULL}, // the handler the calls are copied from
	{0x3873E0, 682, 0x9D2916DB3C802CDDULL}, // the slot finder
};
constexpr int Slots = 16;          // what the message holds
constexpr int Room = 64;           // what the plan is given to write into; it writes one entry for each free slot
constexpr int FilledSound = 0xEB;  // the sound the handler plays after sending
constexpr const char* File = "d2r-access"; // the file and line of the caller; this build reads neither
uintptr_t g_base = 0;

Result Call(uintptr_t belt, int& count) noexcept {
	__try {
		if (!reinterpret_cast<bool(*)(uintptr_t)>(g_base + CanPopulateBelt)(belt)) return Result::Nothing;
		const uintptr_t owner = reinterpret_cast<uintptr_t(*)(uintptr_t)>(g_base + GetOwner)(belt);
		if (!owner) return Result::Refused;
		const uintptr_t inventory = reinterpret_cast<uintptr_t(*)(uintptr_t, const char*, int)>(g_base + GetInventory)(owner, File, 0);
		if (!inventory || reinterpret_cast<uintptr_t(*)(uintptr_t)>(g_base + CursorItem)(inventory)) return Result::Refused;
		uint32_t ids[Room]{}, cells[Room]{};
		uint8_t slots[Room]{};
		int32_t planned = 0;
		if (!reinterpret_cast<int(*)(uintptr_t, uint32_t*, uint32_t*, uint8_t*, int32_t*)>(g_base + Plan)(owner, ids, cells, slots, &planned)
			|| planned <= 0) return Result::Nothing;
		count = planned;
		if (planned > Slots) return Result::Refused;
		reinterpret_cast<void(*)(const uint32_t*, const uint32_t*, const uint8_t*, uint8_t)>(g_base + Send)(ids, cells, slots,
			static_cast<uint8_t>(planned));
		reinterpret_cast<void(*)(int, uintptr_t, int, int, uint8_t)>(g_base + PlaySound)(FilledSound, 0, 0, 0, 0);
		return Result::Sent;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return Result::Unavailable; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "belt-fill", "filling the belt is off")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[belt-fill] Verified.");
	return true;
}

bool Available() { return g_base != 0; }

Result Fill(const D2RL::PluginContext* context, uintptr_t belt) {
	int count = 0;
	const Result result = Available() && NativeUi::Visible(belt) && NativeUi::Classify(belt).name == "BeltWidget"
		? Call(belt, count) : Result::Unavailable;
	AsyncLog::InfoF(context, "[belt-fill] %s, %d planned", result == Result::Sent ? "sent" : result == Result::Nothing ? "nothing to fill"
		: result == Result::Refused ? "refused" : "failed", count);
	return result;
}
}
