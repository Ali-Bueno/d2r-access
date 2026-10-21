#include "exploration_cue_service.h"
#include "async_log.h"
#include "ground_items.h"
#include "item_audio.h"
#include "tutorial.h"

#include <optional>
#include <tuple>

namespace D2RAccess::ExplorationCueService {
namespace {
using RoomKey = std::tuple<uint32_t, int, int>;
std::optional<RoomKey> g_nearest;

GroundItems::Voice Voice(Radar::Position source, Radar::Position player, GroundItems::Sound sound) {
	GroundItems::Item item;
	item.position = {source.x, source.y};
	item.sound = sound;
	return GroundItems::Spatialize(item, {player.x, player.y});
}
}

void Update(const D2RL::PluginContext* context, const NativeLevelExits::Layout& layout,
	Radar::Position player, const ExploredRooms::Memory& memory, bool maze) {
	if (Tutorial::QuietWorld()) { Silence(); return; }
	const auto selection = ExplorationCues::Select(layout, player, memory, maze);
	std::vector<GroundItems::Voice> voices;
	std::optional<RoomKey> nearest;
	if (selection.nearest) {
		const auto& room = *selection.nearest;
		nearest = RoomKey{layout.level, room.rect.x, room.rect.y};
		voices.push_back(Voice(room.position, player, GroundItems::Sound::UnexploredRoom));
	}
	if (nearest != g_nearest) {
		if (selection.nearest) AsyncLog::InfoF(context, "[exploration-audio] nearest room=%d,%d level=%u",
			selection.nearest->rect.x, selection.nearest->rect.y, layout.level);
		else AsyncLog::Info(context, "[exploration-audio] no nearby unexplored room");
		g_nearest = nearest;
	}
	ItemAudio::UpdateExploration(std::move(voices));
	if (selection.entered) {
		ItemAudio::PlayExplorationArrival(Voice(player, player, GroundItems::Sound::InsideUnexploredRoom));
		AsyncLog::InfoF(context, "[exploration-audio] entered room=%d,%d level=%u",
			selection.entered->rect.x, selection.entered->rect.y, layout.level);
	}
}

void Silence() {
	ItemAudio::SilenceExploration();
	g_nearest.reset();
}
}
