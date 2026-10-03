#include "native_swap.h"

#include "async_log.h"
#include "layout_check.h"

#include <windows.h>

namespace D2RAccess::NativeSwap {
namespace {
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/swap.
//
// The table of key actions (0x22A7930, records of 0x18 bytes) holds 0x11BF00
// for action 44, the one the character's .key file binds to W. It takes no
// argument:
//   if (!(Free(InputManager()) || UIVisible(1))) return;          0x13DE60, 0x0CE500
//   if (!FreeToAct(ControllerManager(), LocalIndex())) return;    0x14B380
//   Swap(GetBool(Registry(), weapon set key));                    0x1E33A0, a tail jump
// FreeToAct refuses a character in the middle of a blow, a throw, a kick or a
// skill. Swap reads the items at body locations 4, 5, 11 and 12 and sends the
// server message 0x50 with them and the set in use, which the server checks
// against its own. With a trade open it swaps on the client alone, as the
// game's key does. The controller check is in the dispatcher that calls the
// handler (0x11FF30), not in the handler.
//
// FreeToAct answers yes for no player at all and Swap then reads through the
// null unit, so the player is looked up here first.
constexpr uintptr_t KeyHandler = 0x11BF00, LocalIndex = 0x8B2D0, LocalPlayer = 0x9A480;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{KeyHandler, 112, 0x5A8F3B9AD7FA91E5ULL},
	{0x1E33A0, 629, 0xAD41CF37E9D64AA1ULL},  // the swap: void(uint8 set in use)
	{0x13DE60, 36, 0x4E8F9679DA45E341ULL},   // input is free
	{0x14B380, 424, 0x55B80D0B92085E4CULL},  // the character is free to act
	{0x13CE90, 123, 0x6E472424227624BDULL},  // InputManager
	{0x144640, 123, 0x7C0E28C9AED12207ULL},  // ControllerManager
	{0x0ED2F0, 135, 0xDC9A3CAFBC31EFC4ULL},  // builds message 0x50
	{LocalIndex, 7, 0x512D0A09BEF62F41ULL},
	{LocalPlayer, 165, 0x754EA91CD47DAFC1ULL},
};
constexpr uint32_t LocalPlayers = 8; // bound in native 0x9A480
uintptr_t g_base = 0;

Result Call() noexcept {
	__try {
		const auto index = reinterpret_cast<uint32_t(*)()>(g_base + LocalIndex)();
		if (index >= LocalPlayers || !reinterpret_cast<uintptr_t(*)(uint32_t)>(g_base + LocalPlayer)(index)) return Result::NoPlayer;
		reinterpret_cast<void(*)()>(g_base + KeyHandler)();
		return Result::Asked;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return Result::Unavailable; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "swap", "the weapon swap key is off")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[swap] Verified.");
	return true;
}

bool Available() { return g_base != 0; }

Result Swap(const D2RL::PluginContext* context) {
	const Result result = Available() ? Call() : Result::Unavailable;
	AsyncLog::InfoF(context, "[swap] the game's swap key handler: %s",
		result == Result::Asked ? "ran" : result == Result::NoPlayer ? "not run, no player" : "failed");
	return result;
}
}
