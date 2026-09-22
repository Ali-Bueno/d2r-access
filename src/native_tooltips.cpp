#include "native_tooltips.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "layout_check.h"
#include "tooltip_text.h"

#include <windows.h>
#include <algorithm>
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
constexpr size_t RecordSize = 0x2E8;
constexpr uint64_t MaxRecords = 32; // corruption guard
constexpr uint64_t CaptureMs = 40, ExpiryMs = 180;
std::atomic<uintptr_t> g_panel{0};
std::mutex g_mutex;
Snapshot g_latest;
uint64_t g_lastCopy = 0;
void* g_original = nullptr;
DrawPanelFn g_originalPanel = nullptr;

struct StagedRecord { uint64_t index = 0; Record text; };
struct Frame {
	uintptr_t panel = 0, focusPanel = 0, widget = 0;
	uint64_t selection = UINT64_MAX;
	bool active = false, valid = false, primary = false;
	Record first;
	std::vector<StagedRecord> comparisons;
};
Frame g_frame;

void BeginFrame(uintptr_t panel) noexcept {
	try {
		if (!panel || panel != g_panel.load()) {
			std::lock_guard lock(g_mutex);
			g_frame = {};
			return;
		}
		const auto focus = NativeUi::CurrentFocus();
		std::lock_guard lock(g_mutex);
		g_frame = {};
		g_frame.panel = panel;
		g_frame.focusPanel = focus.panel;
		g_frame.widget = focus.widget;
		g_frame.selection = NativeUi::FocusSelection(focus.widget);
		g_frame.active = true;
		g_frame.valid = true;
	} catch (...) { /* Frame bookkeeping must never prevent stock drawing. */ }
}

void EndFrame(bool completed) noexcept {
	try {
		const uint64_t now = GetTickCount64();
		std::lock_guard lock(g_mutex);
		if (completed && g_frame.active && g_frame.valid && g_panel.load() == g_frame.panel) {
			if (g_frame.primary) {
				Snapshot next;
				next.body = std::move(g_frame.first.body);
				next.footer = std::move(g_frame.first.footer);
				for (auto& record : g_frame.comparisons) next.comparisons.push_back(std::move(record.text));
				next.panel = g_frame.focusPanel;
				next.widget = g_frame.widget;
				next.selection = g_frame.selection;
				next.seenAt = now;
				g_latest = std::move(next);
				g_lastCopy = now;
			} else {
				g_latest.comparisons.clear();
			}
		}
		g_frame = {};
	} catch (...) {
		try { std::lock_guard lock(g_mutex); g_frame = {}; } catch (...) {}
	}
}

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
		g_frame = {};
		g_lastCopy = 0;
	}
}

void Reset() { Track(0); }

void Capture(uintptr_t tooltip) noexcept {
	try {
		const uintptr_t panel = g_panel.load();
		struct { uintptr_t data; uint64_t count; } records{};
		if (!panel || !tooltip || !NativeMemory::Peek(panel + RecordsOffset, records)
			|| !records.count || records.count > MaxRecords || tooltip < records.data) return;
		const uintptr_t delta = tooltip - records.data;
		if (delta % RecordSize || delta / RecordSize >= records.count) return;
		const uint64_t index = delta / RecordSize;
		const uint64_t now = GetTickCount64();
		const NativeMemory::ReadScope readScope;
		const auto focus = NativeUi::CurrentFocus();
		const auto selection = NativeUi::FocusSelection(focus.widget);
		{
			std::lock_guard lock(g_mutex);
			if (g_panel.load() != panel) return;
			if (!g_frame.active && index == 0 && g_lastCopy && now - g_lastCopy < CaptureMs
				&& g_latest.panel == focus.panel && g_latest.widget == focus.widget
				&& g_latest.selection == selection) {
				g_latest.seenAt = now;
				return;
			}
		}
		Record text;
		if (!NativeMemory::ReadEngineString(tooltip, text.body)) return;
		(void)NativeMemory::ReadEngineString(tooltip + FooterOffset, text.footer);
		std::lock_guard lock(g_mutex);
		if (g_panel.load() != panel) return;
		if (g_frame.active) {
			if (g_frame.panel != panel || g_frame.focusPanel != focus.panel
				|| g_frame.widget != focus.widget || g_frame.selection != selection) {
				g_frame.valid = false;
				return;
			}
			if (index == 0) {
				g_frame.first = std::move(text);
				g_frame.primary = true;
			} else if (!TooltipText::Read(text.body).lines.empty()) {
				auto found = std::find_if(g_frame.comparisons.begin(), g_frame.comparisons.end(),
					[index](const StagedRecord& record) { return record.index == index; });
				if (found == g_frame.comparisons.end()) g_frame.comparisons.push_back({index, std::move(text)});
				else found->text = std::move(text);
			}
			return;
		}
		if (index != 0) return;
		Snapshot next;
		next.body = std::move(text.body);
		next.footer = std::move(text.footer);
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
	BeginFrame(panel);
	const bool accelerate = Accelerate(panel);
	int32_t previous = 0;
	const bool changed = accelerate && SetDelay(panel, 0, &previous);
	bool completed = false;
	__try {
		original(panel);
		completed = true;
	} __finally {
		if (changed) (void)SetDelay(panel, previous);
		EndFrame(completed);
	}
}

Snapshot Latest(uint64_t now) {
	std::lock_guard lock(g_mutex);
	if (!g_panel.load() || !g_latest.seenAt || now < g_latest.seenAt || now - g_latest.seenAt > ExpiryMs) return {};
	return g_latest;
}
}
