#include "native_wall_slide.h"
#include "wall_slide_service.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_world.h"
#include <intrin.h>

namespace D2RAccess::NativeWallSlide {
namespace {
// D2R 3.3.93787: the directional solver's fallback probes. See docs/wall-sliding.md.
constexpr uintptr_t ProbeRva = 0x3AA030;
// Two 45-degree snapped retries followed by the eight-direction fallback.
constexpr uintptr_t SnappedReturn = 0x3AB2A1, CenteredReturn = 0x3AB2F6, AlternateReturn = 0x3AB6B4;
// The solver's first probe, along the stick's own direction; it always precedes the fallbacks.
constexpr uintptr_t DesiredReturn = 0x3AB24C;
constexpr size_t ContextPath = 0x38, PathType = 0x50, PathOwner = 0x40;
constexpr size_t UnitType = 0, UnitPath = 0x38;
// Set by the native directional input handlers 0x429860 and 0xFB370.
constexpr uint32_t DirectionalPath = 17, PlayerUnit = 0;
constexpr uint8_t Prefix[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56};
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{ProbeRva, 1150, 0x9D878363AE3AD736ULL},
	{0x3AAFE0, 2733, 0xD88F0A945B722504ULL},
	{0x3ABD00, 15, 0x44CDEC9785826F56ULL},
	{0x342A70, 105, 0xFF3B628BC1CB1DA4ULL},
	{0x429860, 479, 0x8136100044CDF5F6ULL},
	{0xFB370, 787, 0xC7DE1B209CF34D02ULL},
};
ProbeFn g_original = nullptr;
uintptr_t g_base = 0;
// The desired direction of the solver call under way on this thread.
thread_local float t_desired[2]{};
thread_local bool t_haveDesired = false;
void ProbeMovement(uintptr_t context, const float* start, const float* direction, int distance,
	uint32_t* point, float* travel, float* alignment, int16_t* collision) {
	Dispatch({context, start, direction, distance, point, travel, alignment, collision},
		reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_base, g_original);
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "movement", "normal movement retained")) return false;
	g_base = context->exeBase;
	const bool installed = context->InstallInlineHook(ProbeRva, Prefix, sizeof(Prefix), &ProbeMovement, &g_original);
	if (installed) AsyncLog::Info(context, "[movement] Verified directional fallback probe hook installed.");
	else AsyncLog::Warn(context, "[movement] Hook unavailable; normal movement retained.");
	return installed;
}

bool IsLocalDirectionalAttempt(uintptr_t pathContext) noexcept {
	using NativeMemory::Peek;
	NativeMemory::ReadScope scope;
	uintptr_t path = 0, owner = 0, ownerPath = 0;
	uint32_t type = 0, unitType = ~0U;
	return pathContext && Peek(pathContext + ContextPath, path) && path
		&& Peek(path + PathType, type) && type == DirectionalPath
		&& Peek(path + PathOwner, owner) && owner
		&& Peek(owner + UnitType, unitType) && unitType == PlayerUnit
		&& Peek(owner + UnitPath, ownerPath) && ownerPath == path
		&& NativeWorld::IsLocalPlayer(owner);
}

void Dispatch(const Probe& probe, uintptr_t returnRva, ProbeFn original) {
	if (returnRva == DesiredReturn && probe.direction) {
		t_desired[0] = probe.direction[0];
		t_desired[1] = probe.direction[1];
		t_haveDesired = true;
	}
	const bool retry = returnRva == SnappedReturn || returnRva == CenteredReturn;
	const bool fallback = retry || returnRva == AlternateReturn;
	const bool prevent = fallback && WallSlideService::Prevent(probe.context, t_haveDesired ? t_desired : nullptr, retry);
	original(probe.context, probe.start, probe.direction, prevent ? 0 : probe.distance,
		probe.point, probe.travel, probe.alignment, probe.collision);
}
}
