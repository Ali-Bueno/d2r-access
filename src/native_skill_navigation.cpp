#include "native_skill_navigation.h"
#include "native_memory.h"
#include "native_widgets.h"
#include "async_log.h"
#include <windows.h>
#include <atomic>
#include <vector>

namespace D2RAccess::NativeSkillNavigation {
namespace {
// D2R 3.3.93787. Native cursor update/reset, cursor positioning, and the
// FocusableWidget selection contract are derived in docs/controller-navigation.md.
constexpr uintptr_t CursorUpdateRva = 0x234820, CursorResetRva = 0x2347B0;
constexpr uintptr_t CursorPositionRva = 0x14F690, SelectRva = 0x876CA0;
constexpr size_t FocusRectSlot = 0x80, SelectSlot = 0xA0, OwnerPanelOffset = 0x550;
constexpr size_t CursorInterpolatingOffset = 0x68; // 0x234820 interpolates an old snap until cleared
constexpr uint8_t UpdatePrefix[] = {0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF9};
struct Rect { int x, y, width, height; };
struct Point { int x, y; };
uintptr_t g_base = 0;
void* g_original = nullptr;
std::atomic<bool> g_hold{false};

bool Verify(uintptr_t base, uintptr_t rva, size_t size, uint64_t expected) {
	std::vector<uint8_t> bytes(size);
	if (!NativeMemory::Copy(base + rva, bytes.data(), bytes.size())) return false;
	// FNV-1a of each complete function in the analyzed live image.
	uint64_t hash = 14695981039346656037ULL;
	for (const auto byte : bytes) hash = (hash ^ byte) * 1099511628211ULL;
	return hash == expected;
}

bool ResetCursor(uintptr_t cursor) noexcept {
	__try {
		// Cancel only the pending cursor interpolation, then use the stock reset
		// to synchronize its position and velocity with the newly placed cursor.
		*reinterpret_cast<uint8_t*>(cursor + CursorInterpolatingOffset) = 0;
		reinterpret_cast<void(*)(uintptr_t)>(g_base + CursorResetRva)(cursor);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void UpdateCursor(uintptr_t cursor, uint64_t now) {
	if (g_hold.load() && ResetCursor(cursor)) return;
	if (g_original) reinterpret_cast<void(*)(uintptr_t, uint64_t)>(g_original)(cursor, now);
}
bool ReadRect(uintptr_t widget, Rect& rect) noexcept {
	uintptr_t vtable = 0, function = 0;
	if (!NativeMemory::Peek(widget, vtable) || !NativeMemory::Peek(vtable + FocusRectSlot, function)
		|| !NativeMemory::Executable(function)) return false;
	__try {
		reinterpret_cast<Rect*(*)(uintptr_t, Rect*, uint64_t)>(function)(widget, &rect, NativeUi::FocusSelection(widget));
		return rect.width > 0 && rect.height > 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SelectNative(uintptr_t widget, const Rect& rect, uint64_t selection) noexcept {
	__try {
		const Point center{rect.x + rect.width / 2, rect.y + rect.height / 2};
		reinterpret_cast<void(*)(Point)>(g_base + CursorPositionRva)(center);
		return reinterpret_cast<bool(*)(uintptr_t, uint64_t)>(g_base + SelectRva)(widget, selection);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_hold = false;
	if (!context || !NativeWidgets::SupportsTree()) return false;
	const auto base = context->exeBase;
	if (!Verify(base, CursorUpdateRva, 758, 0x86934E582330B085ULL)
		|| !Verify(base, CursorResetRva, 110, 0xC61EB6D7B85C9703ULL)
		|| !Verify(base, CursorPositionRva, 142, 0x516E55B8938F9F06ULL)
		|| !Verify(base, SelectRva, 209, 0xDBCE05A4C6AE6ED0ULL)) {
		AsyncLog::Warn(context, "[skill-grid] Native code mismatch; cursor navigation retained.");
		return false;
	}
	g_base = base;
	if (!context->InstallInlineHook(CursorUpdateRva, UpdatePrefix, sizeof(UpdatePrefix), reinterpret_cast<void*>(&UpdateCursor), &g_original)) {
		g_base = 0;
		AsyncLog::Warn(context, "[skill-grid] Cursor hook unavailable; cursor navigation retained.");
		return false;
	}
	AsyncLog::Info(context, "[skill-grid] Verified cursor adapter installed.");
	return true;
}
bool Available() { return g_base != 0 && g_original != nullptr; }
void HoldCursor(bool hold) { g_hold = hold && Available(); }
NativeUi::Cell Position(uintptr_t widget) {
	Rect rect{};
	if (!Available() || !NativeUi::Visible(widget) || !ReadRect(widget, rect)) return {};
	return {rect.x + rect.width / 2, rect.y + rect.height / 2};
}
bool Select(NativeUi::Focus focus) {
	if (!Available() || !NativeUi::Visible(focus.panel) || !NativeUi::Visible(focus.widget)) return false;
	uintptr_t owner = 0, vtable = 0, select = 0;
	Rect rect{};
	if (!NativeMemory::Peek(focus.widget + OwnerPanelOffset, owner) || owner != focus.panel
		|| !NativeMemory::Peek(focus.widget, vtable) || !NativeMemory::Peek(vtable + SelectSlot, select)
		|| select != g_base + SelectRva || !ReadRect(focus.widget, rect)) return false;
	return SelectNative(focus.widget, rect, NativeUi::FocusSelection(focus.widget));
}
}
