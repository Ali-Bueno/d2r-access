#include "native_buttons.h"

#include "layout_check.h"
#include "native_ui.h"

namespace D2RAccess::NativeButtons {
namespace {
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// D2R 3.3, found 2026-11-01 by comparing the Videos list's unlocked and locked buttons.
constexpr Evidence Checks[] = {
	{0x86EEC0, 120, 0xF330B542F28EF025ULL}, // a button is disabled: +0x67A set, its disabled frame drawn
	{0x86EFB0, 239, 0xDB226E937BECBEA8ULL}, // and enabled: +0x67A cleared, its frame restored
};
}

bool Initialize(const D2RL::PluginContext* context) {
	const bool verified = context && LayoutCheck::Verify(context, context->exeBase, Checks, "buttons",
		"disabled buttons are not said unavailable");
	NativeUi::SetButtonsVerified(verified);
	if (verified) AsyncLog::Info(context, "[buttons] Verified.");
	return verified;
}
}
