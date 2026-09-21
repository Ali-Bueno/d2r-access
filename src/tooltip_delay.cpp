#include "tooltip_delay.h"

#include "async_log.h"
#include "native_memory.h"

#include <windows.h>

namespace D2RAccess::TooltipDelay {
namespace {
// TooltipsPanel's reflected fields, READ from its field table in the 3.3.93787
// image (entries from 0x142536CE0, 0x30 apart, each a 4-byte value):
// tooltipMargin +0xA3C, screenEdgePadding +0xA40, showDelayMilliseconds
// +0xA44, minimumTextSize +0xA50.
constexpr size_t ScreenEdgePaddingOffset = 0xA40, ShowDelayOffset = 0xA44;
// The controller layout sets both; nothing else does.
constexpr int32_t ControllerPadding = 50, ControllerDelay = 400;
// The last panel a refusal or a failed write was logged for, so neither is
// written on every poll.
uintptr_t g_leftAlone = 0;

bool Write(uintptr_t address, int32_t value) noexcept {
	__try {
		*reinterpret_cast<volatile int32_t*>(address) = value;
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

void Apply(const D2RL::PluginContext* context, uintptr_t panel) {
	int32_t padding = 0, delay = 0;
	if (!panel || panel == g_leftAlone || !NativeMemory::Peek(panel + ScreenEdgePaddingOffset, padding)
		|| !NativeMemory::Peek(panel + ShowDelayOffset, delay) || delay == 0) return;
	if (padding != ControllerPadding || delay != ControllerDelay) {
		g_leftAlone = panel;
		AsyncLog::InfoF(context, "[tooltip] show delay left alone: padding=%d delay=%d, expected %d and %d",
			padding, delay, ControllerPadding, ControllerDelay);
		return;
	}
	if (!Write(panel + ShowDelayOffset, 0)) {
		g_leftAlone = panel;
		AsyncLog::InfoF(context, "[tooltip] show delay could not be written; tooltips keep their %d ms", delay);
		return;
	}
	AsyncLog::InfoF(context, "[tooltip] controller show delay %d ms -> 0", delay);
}

}
