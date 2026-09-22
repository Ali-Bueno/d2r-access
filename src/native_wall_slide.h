#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::NativeWallSlide {
using ProbeFn = void(*)(uintptr_t, const float*, const float*, int, uint32_t*, float*, float*, int16_t*);
struct Probe {
	uintptr_t context;
	const float* start;
	const float* direction;
	int distance;
	uint32_t* point;
	float* travel;
	float* alignment;
	int16_t* collision;
};
bool Initialize(const D2RL::PluginContext* context);
bool IsLocalDirectionalAttempt(uintptr_t pathContext) noexcept;
// Alternate directions get zero travel through the original probe's normal initialization.
void Dispatch(const Probe& probe, uintptr_t returnRva, ProbeFn original);
}
