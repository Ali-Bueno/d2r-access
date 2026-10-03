#include "native_belt_rows.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"

#include <windows.h>

namespace D2RAccess::NativeBeltRows {
namespace {
using NativeMemory::Peek;
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/belt.
//
// BeltWidget::Update (0x228DB0) writes every frame the slots of the belt worn
// to +0x6A4 (0x228E57) and whether the belt is drawn in full to +0x6A0
// (0x228E85): not UI state 9, and UI state 0x1A or the pointer over the belt.
// The hit test (vtable 0x1CE2FA0 +0x98, 0x226C60) takes a point of the screen
// and gives {slot, 0}, or leaves the selection alone: it counts the rows above
// the bottom one only while the belt is drawn in full (0x2275E0).
// Key action 0x16, "show belt" (0x11B1D0), takes no argument: with a belt worn
// it toggles UI state 0x1A (0xCDE00). UIVisible (0xCE500) reads the state.
constexpr uintptr_t ShowBelt = 0x11B1D0, UiVisible = 0xCE500;
constexpr size_t ExpandedOffset = 0x6A0, SlotsOffset = 0x6A4, HitTestSlot = 0x98;
constexpr int BeltShown = 0x1A;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x228DB0, 1301, 0x65CC58D1AE2C40CDULL}, // BeltWidget::Update
	{0x226C60, 296, 0xC17C60D2CE81C7ADULL},  // the hit test
	{0x226E30, 153, 0x2666A3C1782BC744ULL},  // a slot's rectangle, vtable +0x80
	{0x2275E0, 81, 0x06D948408375ADBEULL},   // the rows drawn now
	{ShowBelt, 96, 0xD5639C95C17F04D8ULL},
	{0x0CDE00, 1610, 0x75D8098CAD6E1954ULL}, // toggles a UI state
	{UiVisible, 15, 0xA95E59F530B41744ULL},
};
constexpr uintptr_t HitTestRva = 0x226C60;
struct Selection { int32_t x, y; };
struct Point { int32_t x, y; };
uintptr_t g_base = 0;

bool IsBelt(uintptr_t belt) {
	return g_base && NativeUi::Visible(belt) && NativeUi::Classify(belt).name == "BeltWidget";
}

std::optional<bool> ReadShown() noexcept {
	__try { return reinterpret_cast<uint8_t(*)(int)>(g_base + UiVisible)(BeltShown) != 0; }
	__except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
}

bool RunShowBelt() noexcept {
	__try { reinterpret_cast<void(*)()>(g_base + ShowBelt)(); return true; }
	__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int HitTest(uintptr_t function, uintptr_t belt, Point point) noexcept {
	Selection selection{-1, -1};
	__try { reinterpret_cast<Selection*(*)(uintptr_t, Selection*, Point)>(function)(belt, &selection, point); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
	return selection.x >= 0 && selection.x < MostSlots ? selection.x : -1;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "belt-rows", "only the belt's bottom row is reached")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[belt-rows] Verified.");
	return true;
}

bool Available() { return g_base != 0; }

int Slots(uintptr_t belt) {
	int32_t slots = 0;
	if (!IsBelt(belt) || !Peek(belt + SlotsOffset, slots) || slots < Columns || slots > MostSlots || slots % Columns) return 0;
	return slots;
}

bool Expanded(uintptr_t belt) {
	uint8_t expanded = 0;
	return IsBelt(belt) && Peek(belt + ExpandedOffset, expanded) && expanded == 1;
}

std::optional<bool> Shown() { return g_base ? ReadShown() : std::nullopt; }

bool ToggleShown(const D2RL::PluginContext* context) {
	const auto before = Shown();
	const bool ran = g_base && RunShowBelt();
	const auto after = Shown();
	AsyncLog::InfoF(context, "[belt-rows] show belt: %s, shown %d -> %d", ran ? "ran" : "failed",
		before ? (*before ? 1 : 0) : -1, after ? (*after ? 1 : 0) : -1);
	return ran;
}

int SlotAt(uintptr_t belt, NativeUi::Cell point) {
	uintptr_t vtable = 0, function = 0;
	// The belt's own hit test and no other class's: the slot is read by value.
	if (!IsBelt(belt) || !Peek(belt, vtable) || !Peek(vtable + HitTestSlot, function) || function != g_base + HitTestRva) return -1;
	return HitTest(function, belt, Point{point.x, point.y});
}
}
