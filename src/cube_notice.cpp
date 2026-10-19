#include "cube_notice.h"

#include "async_log.h"
#include "cube_state.h"
#include "interaction_reader.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "stash_count.h"

#include <windows.h>
#include <cstdio>
#include <string>

namespace D2RAccess::CubeNotice {
namespace {
using namespace InventoryArrivals;
// How long the notice keeps interrupting speech behind it: the length of the sentence.
constexpr uint64_t NoticeLeadMs = 2500;
constexpr uint64_t OpenedLeadMs = 1500;
bool g_wasOpen = false;

void Say(const D2RL::PluginContext* context, const std::vector<Item>& items, const char* format, const char* kind) {
	for (const auto& group : Grouped(items)) {
		const auto& item = group.item;
		const auto name = StashCount::Counted(InteractionReader::ItemName(item.id, item.classId, item.quality), group.count,
			Strings::Current().itemCount);
		char text[300]{};
		std::snprintf(text, sizeof(text), format, name.c_str());
		AsyncLog::InfoF(context, "[cube] %s id=%u class=%u x%d -> \"%s\"", kind, item.id, item.classId, group.count, text);
		// The focus re-reads the cell the item left; the news goes first.
		ScreenReader::Lead(text, GetTickCount64() + NoticeLeadMs);
	}
}
}

void Tick(const D2RL::PluginContext* context) {
	const bool open = CubeState::PanelOpen();
	if (open == g_wasOpen) return;
	g_wasOpen = open;
	if (!open) return;
	AsyncLog::Info(context, "[cube] window opened");
	ScreenReader::Urge(Strings::Current().cubeOpened, GetTickCount64() + OpenedLeadMs);
}

void Announce(const D2RL::PluginContext* context, const std::vector<Item>& before, const std::vector<Item>& now,
	uint32_t heldBefore, uint32_t heldNow) {
	const auto& strings = Strings::Current();
	std::vector<Item> sent, quick, taken, results;
	for (const auto& move : Moves(before, now, heldBefore)) {
		if (move.item.place == Place::Cube) (InStash(move.from) ? quick : sent).push_back(move.item);
		else if (move.from == Place::Cube && move.item.place == Place::Backpack) taken.push_back(move.item);
	}
	// New items with nothing used up are a stack the stash split a piece off; a transmute used its ingredients up.
	const bool transmuted = CubeConsumed(before, now, heldNow) > 0;
	for (const auto& item : NewInCube(before, now, heldBefore)) (transmuted ? results : quick).push_back(item);
	Say(context, sent, strings.itemMovedIntoCube, "into cube");
	Say(context, quick, strings.itemMovedToCube, "quick move to cube");
	Say(context, taken, strings.itemMovedIntoInventory, "into inventory");
	for (const auto& result : results) Say(context, {result}, strings.itemNewInCube, "result");
}
}
