#include "native_tooltips.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "layout_check.h"

#include <windows.h>
#include <atomic>
#include <mutex>

namespace D2RAccess::NativeTooltips {
namespace {
// D2R 3.3.93787: TooltipsPanel::Draw (0x880550) draws 0x2E8-byte
// records, then destroys their strings and clears the count every frame.
// The primary record is always index zero; comparisons and button glyphs
// follow it. See docs/native-tooltips.md for live stash verification.
constexpr uintptr_t DrawTooltipRva = 0x8DA7E0;
constexpr uint8_t DrawTooltipBytes[] = {
	0x40, 0x56, 0x48, 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00,
	0x48, 0x8B, 0x05, 0xD8, 0x0A, 0x0F, 0x02,
};
// D2R 3.3.93787 TooltipsPanel::Draw. The complete-function fingerprint guards
// ShowDelayOffset; derivation and disassembly are in docs/native-tooltips.md.
constexpr uintptr_t DrawPanelRva = 0x880550;
constexpr uint8_t DrawPanelBytes[] = {
	0x4C, 0x8B, 0xDC, 0x55, 0x53, 0x56, 0x57, 0x41,
	0x55, 0x49, 0x8D, 0xAB, 0x78, 0xFD, 0xFF, 0xFF,
};
constexpr size_t ShowDelayOffset = 0xA44;
struct DrawPanelEvidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr DrawPanelEvidence DrawPanelChecks[] {{ DrawPanelRva, 1943, 0xB3C9FE1D88CB4C5C }};
constexpr size_t RecordsOffset = 0x168, FooterOffset = 0x28;
constexpr uint64_t MaxRecords = 32; // corruption guard
constexpr uint64_t CaptureMs = 40, ExpiryMs = 180;
std::atomic<uintptr_t> g_panel{0};
std::mutex g_mutex;
Snapshot g_latest;
uint64_t g_lastCopy = 0;
void* g_original = nullptr;
DrawPanelFn g_originalPanel = nullptr;

void DrawTooltip(uintptr_t tooltip) {
	Capture(tooltip);
	if (g_original) reinterpret_cast<void(*)(uintptr_t)>(g_original)(tooltip);
}

void DrawPanelHook(uintptr_t panel) { DrawPanel(panel, g_originalPanel); }

bool Accelerate(uintptr_t panel) {
	if (!panel || panel != g_panel.load()) return false;
	switch (NativeUi::Classify(NativeUi::CurrentFocus().widget).kind) {
		case UiText::Kind::InventoryGrid:
		case UiText::Kind::InventorySlot:
		case UiText::Kind::InventoryItem: return true;
		default: return false;
	}
}

bool SetDelay(uintptr_t panel, int32_t value, int32_t* previous = nullptr) noexcept {
	__try {
		auto* delay = reinterpret_cast<int32_t*>(panel + ShowDelayOffset);
		if (previous) *previous = *delay;
		*delay = value;
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context || !NativeWidgets::SupportsTree()) return false;
	const bool installed = context->InstallInlineHook(DrawTooltipRva, DrawTooltipBytes,
		static_cast<uint32_t>(sizeof(DrawTooltipBytes)), reinterpret_cast<void*>(&DrawTooltip), &g_original);
	if (installed) context->LogInfo("[tooltip] Verified native tooltip draw hook installed.");
	else context->LogWarn("[tooltip] Native tooltip hook unavailable or build mismatch; capture disabled.");
	if (installed && LayoutCheck::Verify(context, context->exeBase, DrawPanelChecks, "tooltip",
		"controller tooltip delay retained")) {
		const bool accelerated = context->InstallInlineHook(DrawPanelRva, DrawPanelBytes,
			static_cast<uint32_t>(sizeof(DrawPanelBytes)), reinterpret_cast<void*>(&DrawPanelHook),
			reinterpret_cast<void**>(&g_originalPanel));
		if (accelerated) context->LogInfo("[tooltip] Verified controller tooltip delay hook installed.");
		else context->LogWarn("[tooltip] Controller tooltip delay hook unavailable; stock delay retained.");
	}
	return installed;
}

void Track(uintptr_t panel) {
	if (g_panel.exchange(panel) != panel) {
		std::lock_guard lock(g_mutex);
		g_latest = {};
		g_lastCopy = 0;
	}
}

void Reset() { Track(0); }

void Capture(uintptr_t tooltip) noexcept {
	try {
		const uintptr_t panel = g_panel.load();
		struct { uintptr_t data; uint64_t count; } records{};
		if (!panel || !tooltip || !NativeMemory::Peek(panel + RecordsOffset, records)
			|| !records.count || records.count > MaxRecords || records.data != tooltip) return;
		const uint64_t now = GetTickCount64();
		const NativeMemory::ReadScope readScope;
		const auto focus = NativeUi::CurrentFocus();
		const auto selection = NativeUi::FocusSelection(focus.widget);
		std::lock_guard lock(g_mutex);
		if (g_panel.load() != panel) return;
		if (g_lastCopy && now - g_lastCopy < CaptureMs && g_latest.panel == focus.panel
			&& g_latest.widget == focus.widget && g_latest.selection == selection) {
			g_latest.seenAt = now;
			return;
		}
		Snapshot next;
		if (!NativeMemory::ReadEngineString(tooltip, next.body)) return;
		(void)NativeMemory::ReadEngineString(tooltip + FooterOffset, next.footer);
		next.panel = focus.panel;
		next.widget = focus.widget;
		next.selection = selection;
		next.seenAt = now;
		g_latest = std::move(next);
		g_lastCopy = now;
	} catch (...) { /* An accessibility copy must never prevent stock drawing. */ }
}

void DrawPanel(uintptr_t panel, DrawPanelFn original) {
	if (!original) return;
	const bool accelerate = Accelerate(panel);
	int32_t previous = 0;
	const bool changed = accelerate && SetDelay(panel, 0, &previous);
	__try {
		original(panel);
	} __finally {
		if (changed) (void)SetDelay(panel, previous);
	}
}

Snapshot Latest(uint64_t now) {
	std::lock_guard lock(g_mutex);
	if (!g_panel.load() || !g_latest.seenAt || now < g_latest.seenAt || now - g_latest.seenAt > ExpiryMs) return {};
	return g_latest;
}
}
