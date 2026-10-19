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
// A sound instance's volume (0-255, the instance's +0x24): the caller sets the
// footfall's after this returns, then the game's sliders scale it. See docs/footsteps.md.
constexpr uintptr_t GetVolumeRva = 0x19E9D0, SetVolumeRva = 0x19F900;
uintptr_t g_resolve = 0, g_getVolume = 0, g_setVolume = 0;
void* g_original = nullptr;

const D2RL::PluginContext* g_context = nullptr;
constexpr int BoostLogs = 3; // the first boosted steps are logged, for the live check
int g_boostLogs = 0;

uintptr_t PlayOriginal(uint32_t entity) { return reinterpret_cast<uintptr_t(*)(uint32_t)>(g_original)(entity); }

// A louder step is further footfalls of the same sound, the last one quieter (an instance caps at 255).
void PlayExtras(uint32_t entity, const Footsteps::Extras& extras) {
	int played = 0, stock = -1;
	for (int i = 0; i < extras.whole; ++i) played += PlayOriginal(entity) != 0;
	if (extras.partialPercent > 0) {
		if (const auto handle = PlayOriginal(entity)) {
			++played;
			stock = Volume(handle);
			if (stock > 0) SetVolume(handle, Footsteps::ScaleVolume(stock, extras.partialPercent));
		}
	}
	if (g_boostLogs < BoostLogs) {
		++g_boostLogs;
		AsyncLog::InfoF(g_context, "[footsteps] boost whole=%d partial=%d%% played=%d partial_stock=%d", extras.whole, extras.partialPercent, played, stock);
	}
}

uintptr_t Play(uint32_t entity) {
	if (FootstepService::Suppress(entity)) return 0;
	const auto handle = PlayOriginal(entity);
	if (!handle) return handle;
	const auto extras = FootstepService::Played(entity, handle);
	if (extras.whole > 0 || extras.partialPercent > 0) PlayExtras(entity, extras);
	return handle;
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
	g_context = context;
	const uintptr_t base = context->exeBase;
	if (!Verify(base, PlayRva, 926, 0x23C50C43C505B306ULL)
		|| !Verify(base, ResolveRva, 90, 0xE8700C4429FA18D7ULL)
		|| !Verify(base, 0x7D9800, 297, 0xECB8CBDF6CE9625AULL)) {
		AsyncLog::Warn(context, "[footsteps] Native footfall code mismatch; normal game sounds retained.");
		return false;
	}
	g_resolve = base + ResolveRva;
	if (Verify(base, GetVolumeRva, 57, 0x018164D7729BD324ULL) && Verify(base, SetVolumeRva, 56, 0x988923A1DB8C994AULL)) {
		g_getVolume = base + GetVolumeRva;
		g_setVolume = base + SetVolumeRva;
	} else AsyncLog::Warn(context, "[footsteps] Sound volume code mismatch; footstep volume unavailable.");
	const bool installed = context->InstallInlineHook(PlayRva, PlayPrefix, sizeof(PlayPrefix), reinterpret_cast<void*>(&Play), &g_original);
	if (installed) AsyncLog::Info(context, "[footsteps] Verified HD footfall hook installed; local stalled steps only.");
	else { g_resolve = g_getVolume = g_setVolume = 0; AsyncLog::Warn(context, "[footsteps] Hook unavailable; normal game sounds retained."); }
	return installed;
}
bool VolumeSupported() { return g_getVolume && g_setVolume; }
int Volume(uintptr_t handle) noexcept {
	__try { return g_getVolume ? reinterpret_cast<int(*)(uintptr_t)>(g_getVolume)(handle) : -1; }
	__except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
void SetVolume(uintptr_t handle, int volume) noexcept {
	__try { if (g_setVolume) reinterpret_cast<void(*)(uintptr_t, int)>(g_setVolume)(handle, volume); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
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
	result.mode = mode;
	result.locomotion = mode == Walk || mode == Run || mode == TownWalk;
	return result;
}
}
