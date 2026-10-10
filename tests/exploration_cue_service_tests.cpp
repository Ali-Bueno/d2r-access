#include "exploration_cue_service.h"
#include "async_log.h"
#include "ground_items.h"
#include "item_audio.h"
#include "test_support.h"

#include <initializer_list>
#include <utility>
#include <vector>

using namespace D2RAccess;

namespace {
std::vector<GroundItems::Voice> loops;
std::vector<GroundItems::Voice> arrivals;
int silences = 0;

NativeLevelExits::Layout Layout(NativeLevelExits::TileRect bounds,
	std::initializer_list<NativeLevelExits::LevelRoom> rooms) {
	NativeLevelExits::Layout layout;
	layout.level = 5;
	layout.rect = bounds;
	layout.rooms = rooms;
	return layout;
}

void Reset() {
	ExplorationCueService::Silence();
	loops.clear();
	arrivals.clear();
	silences = 0;
}

void OneMovingTarget() {
	Reset();
	const auto layout = Layout({0, 0, 12, 2}, {
		{{0, 0, 2, 2}, true}, {{2, 0, 8, 2}, true}, {{10, 0, 2, 2}, true}});
	ExploredRooms::Memory memory;
	memory.Visit(5, 2, 0);
	ExplorationCueService::Update(nullptr, layout, {25, 5}, memory, true);
	Check(loops.size() == 1 && arrivals.empty(), "several eligible rooms produce one loop and no revisit arrival");
	Check(loops[0].sound == GroundItems::Sound::UnexploredRoom && loops[0].right < 0
		&& loops[0].front > 0 && loops[0].gain > 0 && loops[0].gain < GroundItems::LoopVolume,
		"the nearest left room has the exploration sound, direction, and attenuated gain");
	const auto left = loops[0];
	ExplorationCueService::Update(nullptr, layout, {49, 5}, memory, true);
	Check(loops.size() == 1 && loops[0].sound == GroundItems::Sound::UnexploredRoom
		&& loops[0].right > 0 && loops[0].front < 0 && loops[0].gain > left.gain,
		"moving within the current room switches to the nearer right room and updates attenuation");
	const auto expected = GroundItems::Spatialize(
		GroundItems::Item{.position = {50, 5}, .sound = GroundItems::Sound::UnexploredRoom}, {49, 5});
	Check(loops[0].right == expected.right && loops[0].front == expected.front
		&& loops[0].gain == expected.gain, "the service spatializes the selected in-room point");
}

void ArrivalsAndPersistence() {
	Reset();
	const auto layout = Layout({0, 0, 6, 2}, {
		{{0, 0, 2, 2}, true}, {{2, 0, 2, 2}, true}, {{4, 0, 2, 2}, true}});
	ExploredRooms::Memory memory;
	memory.Mark(5, 2, 0);
	ExplorationCueService::Update(nullptr, layout, {5, 5}, memory, true);
	Check(loops.size() == 1 && arrivals.size() == 1, "first observation loops toward one room and plays one arrival");
	Check(arrivals[0].sound == GroundItems::Sound::InsideUnexploredRoom
		&& arrivals[0].gain == GroundItems::LoopVolume && arrivals[0].right == 0 && arrivals[0].front == 0,
		"the arrival plays once at the player's position with its own sound");
	memory.Visit(5, 0, 0); // production records a selected arrival immediately after Update
	ExplorationCueService::Update(nullptr, layout, {5, 5}, memory, true);
	Check(arrivals.size() == 1 && loops.size() == 1, "standing still after recording the visit repeats no arrival");
	ExplorationCueService::Update(nullptr, layout, {10, 5}, memory, true);
	Check(arrivals.size() == 2 && loops.size() == 1 && loops[0].right > 0,
		"entering the next room fires one arrival and advances the single loop");
	memory.Visit(5, 2, 0);
	ExplorationCueService::Silence();
	Check(loops.empty() && silences == 1, "Silence explicitly cancels a sounding loop");
	ExplorationCueService::Update(nullptr, layout, {10, 5}, memory, true);
	Check(arrivals.size() == 2 && loops.size() == 1, "resuming after Silence does not duplicate an arrival");
	ExploredRooms::Memory restored;
	restored.Merge(memory.Serialize());
	ExplorationCueService::Silence();
	ExplorationCueService::Update(nullptr, layout, {5, 5}, restored, true);
	Check(arrivals.size() == 2 && loops.size() == 1,
		"revisiting after a saved-memory reload does not duplicate an arrival");
}

void NonmazeAndNoCurrentRoom() {
	Reset();
	const auto layout = Layout({0, 0, 4, 2}, {
		{{0, 0, 2, 2}, true}, {{2, 0, 2, 2}, true}});
	ExploredRooms::Memory memory;
	ExplorationCueService::Update(nullptr, layout, {5, 5}, memory, true);
	Check(loops.size() == 1 && arrivals.size() == 1, "maze observation starts cues");
	ExplorationCueService::Update(nullptr, layout, {5, 5}, memory, false);
	Check(loops.empty() && arrivals.size() == 1, "nonmaze update removes the loop and causes no arrival");
	ExplorationCueService::Update(nullptr, layout, {50, 5}, memory, true);
	Check(loops.empty() && arrivals.size() == 1, "player outside active rooms leaves no stale output");
}
}

namespace D2RAccess::ItemAudio {
void UpdateExploration(std::vector<GroundItems::Voice> voices) { loops = std::move(voices); }
void PlayExplorationArrival(GroundItems::Voice voice) { arrivals.push_back(voice); }
void SilenceExploration() { loops.clear(); ++silences; }
}
namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) noexcept {}
void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {}
}

int main() {
	OneMovingTarget();
	ArrivalsAndPersistence();
	NonmazeAndNoCurrentRoom();
	Passed();
}
