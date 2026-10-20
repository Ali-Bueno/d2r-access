#pragma once
#include <D2RLPlugin/api.h>
#include <cstdint>

namespace D2RAccess::NativeKeyboard {
using Redirect = bool(*)(uint16_t packedKey) noexcept;
bool Initialize(const D2RL::PluginContext* context, Redirect redirect);
void Reset();
// UI pump drains bounded diagnostic counters; the native hook only increments.
uint32_t TakeClaims();

// BindingDown's action zero is Character. Only a claimed keyboard binding
// may replace it; native C, forced controller actions and unrelated actions
// must reach the original dispatcher untouched. Used by the actual hook and
// isolated producer-order fixtures.
template<class Claim, class Original>
bool Route(uint32_t action, uint16_t key, bool forced, Claim&& claim, Original&& original) {
	if (!forced && action == 0 && claim(key)) return true;
	return original();
}
}
