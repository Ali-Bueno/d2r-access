#include "native_footsteps.h"
#include "footstep_service.h"
#include "native_memory.h"
#include "native_world.h"
#include "async_log.h"

#include <windows.h>
#include <vector>

namespace D2RAccess::NativeFootsteps {
namespace {
// D2R 3.3.93787 HD footfall event: floor sound plus armor footstep_foley.
// The caller (0x7D9800) accepts a null sound handle. See docs/footsteps.md.
constexpr uintptr_t PlayRva = 0x7D9390, ResolveRva = 0x769B50;
constexpr size_t UnitType = 0, UnitMode = 0xC, UnitPath = 0x38, PathPosition = 0;
// D2C_PlayerModes (D2MOO Units/Player.h); native 0x7D96D0 also tests run == 3.
constexpr uint32_t Walk = 2, Run = 3, TownWalk = 6;
constexpr uint8_t PlayPrefix[] = {0x40, 0x55, 0x56, 0x41, 0x55, 0x48, 0x8D, 0x6C, 0x24, 0xB9, 0x48, 0x81, 0xEC, 0xA0, 0, 0, 0};
uintptr_t g_resolve = 0;
void* g_original = nullptr;

uintptr_t Play(uint32_t entity) {
	if (FootstepService::Suppress(entity)) return 0;
	return reinterpret_cast<uintptr_t(*)(uint32_t)>(g_original)(entity);
}
uintptr_t Resolve(uint32_t entity) noexcept {
	__try { return g_resolve ? reinterpret_cast<uintptr_t(*)(uint32_t)>(g_resolve)(entity) : 0; }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool Verify(uintptr_t base, uintptr_t rva, size_t size, uint64_t expected) {
	std::vector<uint8_t> bytes(size);
	if (!NativeMemory::Copy(base + rva, bytes.data(), bytes.size())) return false;
	// FNV-1a, complete functions compared with the running executable before hooking.
	uint64_t hash = 14695981039346656037ULL;
	for (const auto byte : bytes) hash = (hash ^ byte) * 1099511628211ULL;
	return hash == expected;
}
}
bool Initialize(const D2RL::PluginContext* context) {
	if (!context) return false;
	const uintptr_t base = context->exeBase;
	if (!Verify(base, PlayRva, 926, 0x23C50C43C505B306ULL)
		|| !Verify(base, ResolveRva, 90, 0xE8700C4429FA18D7ULL)
		|| !Verify(base, 0x7D9800, 297, 0xECB8CBDF6CE9625AULL)) {
		AsyncLog::Warn(context, "[footsteps] Native footfall code mismatch; normal game sounds retained.");
		return false;
	}
	g_resolve = base + ResolveRva;
	const bool installed = context->InstallInlineHook(PlayRva, PlayPrefix, sizeof(PlayPrefix), reinterpret_cast<void*>(&Play), &g_original);
	if (installed) AsyncLog::Info(context, "[footsteps] Verified HD footfall hook installed; local stalled steps only.");
	else { g_resolve = 0; AsyncLog::Warn(context, "[footsteps] Hook unavailable; normal game sounds retained."); }
	return installed;
}
Footsteps::Sample Read(uint32_t entity) noexcept {
	Footsteps::Sample result{.entity = entity};
	const uintptr_t unit = Resolve(entity);
	if (!unit) return result;
	result.local = NativeWorld::IsLocalPlayer(unit);
	if (!result.local) { result.valid = true; return result; }
	uint32_t type = ~0U, mode = ~0U;
	uintptr_t path = 0;
	if (!NativeMemory::Peek(unit + UnitType, type) || type != 0 || !NativeMemory::Peek(unit + UnitMode, mode)
		|| !NativeMemory::Peek(unit + UnitPath, path) || !path || !NativeMemory::Peek(path + PathPosition, result.position)) return result;
	result.valid = true;
	result.locomotion = mode == Walk || mode == Run || mode == TownWalk;
	return result;
}
}
