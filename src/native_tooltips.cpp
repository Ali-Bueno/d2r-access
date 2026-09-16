#include "native_tooltips.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"

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
constexpr size_t RecordsOffset = 0x168, FooterOffset = 0x28;
constexpr uint64_t MaxRecords = 32; // corruption guard
constexpr uint64_t CaptureMs = 40, ExpiryMs = 180;
std::atomic<uintptr_t> g_panel{0};
std::mutex g_mutex;
Snapshot g_latest;
uint64_t g_lastCopy = 0;
void* g_original = nullptr;

void DrawTooltip(uintptr_t tooltip) {
	Capture(tooltip);
	if (g_original) reinterpret_cast<void(*)(uintptr_t)>(g_original)(tooltip);
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (!context || !NativeWidgets::SupportsTree()) return false;
	const bool installed = context->InstallInlineHook(DrawTooltipRva, DrawTooltipBytes,
		static_cast<uint32_t>(sizeof(DrawTooltipBytes)), reinterpret_cast<void*>(&DrawTooltip), &g_original);
	if (installed) context->LogInfo("[tooltip] Verified native tooltip draw hook installed.");
	else context->LogWarn("[tooltip] Native tooltip hook unavailable or build mismatch; capture disabled.");
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

Snapshot Latest(uint64_t now) {
	std::lock_guard lock(g_mutex);
	if (!g_panel.load() || !g_latest.seenAt || now < g_latest.seenAt || now - g_latest.seenAt > ExpiryMs) return {};
	return g_latest;
}
}
