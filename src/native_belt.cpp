#include "native_belt.h"
#include "belt_reader.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "native_ui.h"
#include <D2RLPlugin/context.h>
#include <windows.h>

namespace D2RAccess::NativeBelt {
namespace {
// BeltWidget::Draw's empty-cell path, verified in the mapped 3.3.93787 image.
// Getter and both arguments are the game's own; see docs/belt.md.
constexpr uintptr_t DefaultItem = 0x2271F0, SelectionMode = 0x14CF40, ControllerMode = 0x77E10;
constexpr GroundItemLayout::Evidence Checks[] = {
	{DefaultItem, 586, 0xFB4CF2AB57773436ULL},
	{SelectionMode, 192, 0xF792A07C71DF5D43ULL},
	{ControllerMode, 41, 0x290F3F607678EB67ULL},
};
uintptr_t g_checkedBase = 0;
bool g_available = false;

std::optional<uintptr_t> Query(uintptr_t base, uintptr_t widget, int column) noexcept {
	__try {
		if (!reinterpret_cast<bool(*)()>(base + ControllerMode)()) return uintptr_t{0};
		const bool mode = reinterpret_cast<bool(*)()>(base + SelectionMode)();
		return reinterpret_cast<uintptr_t(*)(uintptr_t, int, bool, uintptr_t)>(base + DefaultItem)(widget, column, mode, 0);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
}
}
std::optional<uintptr_t> AutomaticItem(const D2RL::PluginContext* context, uintptr_t widget, int column) {
	if (!context || !context->exeBase || column < 0 || column >= BeltReader::Columns
		|| NativeUi::Classify(widget).name != "BeltWidget" || !NativeUi::Visible(widget)) return std::nullopt;
	if (g_checkedBase != context->exeBase) {
		g_checkedBase = context->exeBase;
		g_available = LayoutCheck::Verify(context, context->exeBase, Checks, "belt", "automatic inventory potion unavailable");
	}
	return g_available ? Query(context->exeBase, widget, column) : std::nullopt;
}
}
