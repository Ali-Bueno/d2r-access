#include "native_list_rows.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_ui.h"
#include "ui_text.h"

namespace D2RAccess::NativeListRows {
namespace {
using NativeMemory::Peek;
// D2R 3.3.93787; the working files are in the workspace, re/actions/charcreate
// (notes.txt, E) and re/actions/widgets.
//
// READ: ListWidget::AddItem (0x885150) gives each row its place in the list
// (i32 +0x55C), the list that owns it (+0x598) and, for a list that is not a
// hover list, the byte +0x558 = 1. With that byte set a left click on the row
// (0x8CDA00) chooses it (0x886420 writes the place to list +0x248) and acts on
// it only when the click before it on the same row was less than 500 ms
// earlier (0x8CD4B0, the list's own message). Enter acts on the row whose
// place is the list's +0x248, with the pointer anywhere.
// MEASURED 2026-09-27 12:19:20: the message of the character list,
// CharacterSelect:SelectCharacter, was followed by FrontEndNavigation:ToGame.
constexpr size_t TwoClicksOffset = 0x558, PlaceOffset = 0x55C, OwnerOffset = 0x598, ChosenOffset = 0x248;
constexpr int MostRows = 4096;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x885150, 649, 0x33F861CF0DDF6F80ULL}, // a row is added to a list
	{0x8CDA00, 796, 0xBD65F1B2910C3F39ULL}, // a row's messages, the click among them
	{0x886420, 457, 0x96C4CAF0D8E1D185ULL}, // the list chooses a row
	{0x8CD4B0, 72, 0x28B4F63E4837D5D5ULL},  // a row is acted on
};
bool g_verified = false;
}

bool Initialize(const D2RL::PluginContext* context) {
	g_verified = context && LayoutCheck::Verify(context, context->exeBase, Checks, "rows", "a row of a list is not chosen as the cursor comes to it");
	if (g_verified) AsyncLog::Info(context, "[rows] Verified.");
	return g_verified;
}

bool TwoClicks(uintptr_t row) {
	uint8_t flag = 0;
	return g_verified && row && NativeUi::Classify(row).kind == UiText::Kind::ListItem && Peek(row + TwoClicksOffset, flag) && flag == 1;
}

std::optional<Places> Of(uintptr_t row) {
	uintptr_t list = 0;
	int32_t place = -1, chosen = -1;
	if (!TwoClicks(row) || !Peek(row + PlaceOffset, place) || !Peek(row + OwnerOffset, list) || !list
		|| NativeUi::Classify(list).kind != UiText::Kind::List || !Peek(list + ChosenOffset, chosen)
		|| place < 0 || place >= MostRows || chosen < -1 || chosen >= MostRows) return std::nullopt;
	return Places{place, chosen};
}
}
