#include "native_sort.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"

#include <windows.h>
#include <algorithm>
#include <vector>

namespace D2RAccess::NativeSort {
namespace {
// D2R 3.3.93787, READ in the image and not yet measured in the game. The
// inventory grid's message handler (0x2C6BD0, vtable 0x1CF3800 +0x20) ends the
// controller's hold with
//   owner = GetOwner(grid); if (CanSortPage(owner, page)) SortPage(id, page);
// taking page from grid +0x630 and id from grid +0x5C4. The controller check
// (0x77E10) is in that handler alone, not in the three functions. SortPage
// lays the page's items out again and sends the server one message with every
// item's new cell (0x4E), so no item leaves its page.
constexpr uintptr_t SortPageRva = 0x160B00, CanSortPageRva = 0x15A680, GetOwnerRva = 0x2A7810;
constexpr size_t OwnerIdOffset = 0x5C4, OwnerTypeOffset = 0x5C8, PageOffset = 0x630; // SetOwner 0x2AAB40
constexpr uint32_t NoOwner = 0xFFFFFFFF, PlayerType = 0;
constexpr uint8_t EquippedPage = 1, LastPage = 4; // the server takes pages under 5 (0x4AE440)
// While a trade is open SortPage takes another path, which was not read through.
constexpr const char* TradePanel = "TradeLayout";
uintptr_t g_base = 0;

bool Verify(uintptr_t base, uintptr_t rva, size_t size, uint64_t expected) {
	std::vector<uint8_t> bytes(size);
	if (!NativeMemory::Copy(base + rva, bytes.data(), bytes.size())) return false;
	// FNV-1a of each complete function in the analyzed live image.
	uint64_t hash = 14695981039346656037ULL;
	for (const auto byte : bytes) hash = (hash ^ byte) * 1099511628211ULL;
	return hash == expected;
}

Result Call(uintptr_t grid, uint32_t id, uint8_t page) noexcept {
	__try {
		const uintptr_t owner = reinterpret_cast<uintptr_t(*)(uintptr_t)>(g_base + GetOwnerRva)(grid);
		if (!owner || !reinterpret_cast<bool(*)(uintptr_t, uint8_t)>(g_base + CanSortPageRva)(owner, page)) return Result::Refused;
		reinterpret_cast<void(*)(uint32_t, uint8_t)>(g_base + SortPageRva)(id, page);
		return Result::Sorted;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return Result::Unavailable; }
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !NativeWidgets::SupportsTree()) return false;
	const auto base = context->exeBase;
	if (!Verify(base, SortPageRva, 1410, 0xB07CBBE119BDD898ULL)
		|| !Verify(base, CanSortPageRva, 244, 0xBC9A4B4A8B98E948ULL)
		|| !Verify(base, GetOwnerRva, 92, 0x480CF1F27271F67EULL)) {
		AsyncLog::Warn(context, "[sort] Native code mismatch; sorting unavailable.");
		return false;
	}
	g_base = base;
	AsyncLog::Info(context, "[sort] Verified.");
	return true;
}

bool Available() { return g_base != 0; }

Result Sort(const D2RL::PluginContext* context, uintptr_t grid) {
	if (!Available() || !NativeUi::Visible(grid) || NativeUi::Classify(grid).kind != UiText::Kind::InventoryGrid)
		return Result::Unavailable;
	uint32_t id = NoOwner, type = 0;
	uint8_t page = 0xFF;
	if (!NativeMemory::Peek(grid + OwnerIdOffset, id) || !NativeMemory::Peek(grid + OwnerTypeOffset, type)
		|| !NativeMemory::Peek(grid + PageOffset, page)) return Result::Unavailable;
	const auto& open = PanelState::OpenPanels();
	const bool trading = std::find(open.begin(), open.end(), TradePanel) != open.end();
	Result result = Result::Refused;
	if (id != NoOwner && type == PlayerType && page != EquippedPage && page <= LastPage && !trading) result = Call(grid, id, page);
	AsyncLog::InfoF(context, "[sort] grid %s owner id=%u type=%u page=%u trading=%u -> %s", NativeWidgets::ReadName(grid).c_str(),
		id, type, static_cast<unsigned>(page), trading ? 1u : 0u,
		result == Result::Sorted ? "sorted" : result == Result::Refused ? "refused" : "failed");
	return result;
}
}
