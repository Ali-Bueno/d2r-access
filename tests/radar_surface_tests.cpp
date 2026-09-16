#include "radar.h"
#include "test_support.h"
#include <cmath>
#include <string_view>

using namespace D2RAccess::Radar;
namespace {
World Field() {
	World world;
	world.session = world.level = 1;
	world.player = {100, 100}; world.bodyPattern = 1; world.walkingMask = 0x1C09;
	world.rooms.push_back({0, 0, 240, 240, std::vector<uint16_t>(240 * 240)});
	return world;
}
void Put(World& w, int x, int y, uint16_t flags = 1) { w.rooms[0].cells[y*240+x] = flags; }
void WallAtAngle() {
	// D2's grid staircases are not AHC's exact tile-face intersections. A
	// straight wall at an arbitrary slope must not turn into a row of doors.
	for (const double slope : {0.0, 0.5, 2.0}) for (int stride : {1, 3}) {
		auto world = Field();
		for (int x = 0; x < 240; ++x) for (int y = 0; y < 240; ++y)
			if (y <= 35 + slope*(x-80)) Put(world, x, y);
		world.player = {100, 45 + std::floor(slope*20)};
		Engine engine; (void)engine.Tick(world, 1000);
		unsigned events = 0;
		for (uint64_t step = 1; step <= 18; ++step) {
			world.player.x += stride;
			world.player.y = 45 + std::floor(slope*(world.player.x-80));
			const auto result = engine.Tick(world, 1000 + step*stride*50);
			for (auto event : result) events += event != State::Unknown;
		}
		Check(events == 0, "a continuous rasterized wall has no fictitious openings at any tested slope or stride");
	}
}
void ClippedSamples() {
	// The old intermediate sweep stopped at 21, below its requested 21.414
	// release range, while labelling that result Open. Arrival at the final
	// cell could therefore be compared with an invented intermediate exit.
	auto world = Field();
	for (int x = 0; x < 240; ++x) Put(world, x, 84);
	Engine engine; (void)engine.Tick(world, 1000);
	unsigned events = 0;
	for (uint64_t step = 1; step <= 15; ++step) {
		world.player.x += 3;
		for (auto event : engine.Tick(world, 1000 + step*150)) events += event != State::Unknown;
	}
	Check(events == 0, "an unchanged wall at the sweep limit cannot create duplicate openings from interpolated samples");
}
void CornerPass() {
	// Static solid rectangle, native five-cell footprint, diagonal screen
	// motion. Each side of the single corner must be consumed only once.
	for (int stride : {1, 2, 3}) {
		auto world = Field();
		for (int x = 70; x <= 90; ++x) for (int y = 80; y <= 110; ++y) Put(world, x, y, 0x400);
		world.player = {95, 100};
		Engine engine; (void)engine.Tick(world, 1000);
		unsigned openings = 0;
		for (uint64_t step = 1; step <= 24 / stride; ++step) {
			world.player.x += stride; world.player.y += stride;
			openings += engine.Tick(world, 1000+step*stride*50)[1] == State::Open;
		}
		Check(openings <= 1, "running past one solid corner does not announce its opening twice");
	}
}
void SlidePastObject() {
	// Collision response can slide around a post between polls. The straight
	// interpolation is blocked, but the far wall above remains one surface.
	auto world = Field();
	for (int x = 0; x < 240; ++x) Put(world, x, 86);
	Put(world, 103, 99, 0x400);
	Engine engine; (void)engine.Tick(world, 1000);
	world.player = {106, 98};
	(void)engine.Tick(world, 1150);
	Check(engine.Tick(world, 1200)[0] == State::Unknown,
		"sliding around a post cannot turn travel along the same farther wall into a geometric edge");
}
void PartialPlayback() {
	auto world = Field(); Put(world, 101, 101, 0x800);
	Engine engine; (void)engine.Tick(world, 1000);
	Put(world, 101, 101, 0); (void)engine.Tick(world, 1050);
	Check(engine.Tick(world, 1100)[3] == State::Open, "the rear ray announces a newly opened door");
	AudioLoad failure; failure.failed[3] = {State::Open, 1100, true};
	unsigned repeated = 0;
	for (uint64_t now = 1150; now <= 2000; now += 50) {
		repeated += engine.Tick(world, now, failure)[3] == State::Open;
		failure = {};
	}
	Check(repeated == 0, "an opening already submitted to audio is not replayed after its tail was interrupted");
	Put(world, 101, 101, 0x800); (void)engine.Tick(world, 2050);
	Check(engine.Tick(world, 2100)[3] == State::Blocked, "a real rear closure is still reported after interrupted playback");
	Put(world, 101, 101, 0); (void)engine.Tick(world, 2150);
	Check(engine.Tick(world, 2200)[3] == State::Open, "reopening the rear door is a fresh event, not suppressed as a duplicate");
}
void AdjacentFacets() {
	// Two steps of one widening corner within the player's three-cell span.
	// A second opening must not be queued behind the first for that same spot.
	auto world = Field();
	const auto wall = [&](int sum, uint16_t flags) {
		for (int y=0; y<240; ++y) if (sum-y >= 0 && sum-y < 240) Put(world, sum-y, y, flags);
	};
	wall(216, 1); wall(222, 1); wall(228, 1);
	Engine engine; (void)engine.Tick(world, 1000);
	wall(216, 0); (void)engine.Tick(world, 1050);
	Check(engine.Tick(world, 1100)[3] == State::Open, "the first widening is announced");
	world.player = {101, 99}; wall(222, 0); (void)engine.Tick(world, 1150);
	Check(engine.Tick(world, 1200)[3] == State::Unknown, "adjacent facets of the same widening produce one cue");
	wall(216, 1); (void)engine.Tick(world, 1250);
	Check(engine.Tick(world, 1300)[3] == State::Blocked, "coalescing consecutive openings does not delay an opposite closure");
	wall(216, 0); (void)engine.Tick(world, 1350);
	Check(engine.Tick(world, 1400)[3] == State::Open, "a confirmed closure rearms the next opening even without leaving the cell");
}
void RecordedCampPost() {
	// Exact Ctrl+T grid, 2026-09-15 21:06:53, centered at (4472,4622).
	// '#' stores terrain classification, not the unrecoverable original bits.
	// Nothing beyond this 25x25 capture is inferred to be open.
	constexpr std::array<std::string_view, 25> rows{
		"...........##############", "............#############", "............#############",
		"...............##########", "...............##########", "............#############",
		"............###.......###", ".............##........##", "........................#",
		"........................#", "########.................", "########.................",
		"............@............", ".........................", ".........................",
		"####.#...................", "####.#...................", "####.....................",
		"####.....................", "......#..................", ".........................",
		".........................", ".........................", ".........................",
		"........................."};
	auto world = Field(); world.rooms = {{4460,4610,25,25,std::vector<uint16_t>(625)}};
	for (size_t y=0; y<rows.size(); ++y) for (size_t x=0; x<rows[y].size(); ++x)
		world.rooms[0].cells[y*25+x] = rows[y][x] == '#' ? 1 : 0;
	const Position left{-std::sqrt(0.5), std::sqrt(0.5)};
	world.player = {4473,4621}; const auto post = Scan(world, left, {}, Layer::Terrain);
	Check(post.clearDepth == 9 && post.hit.x == 4466 && post.hit.y == 4628,
		"the captured camp post reproduces the exact nine-subtile obstruction in the 08:01:13.708 event");
	Check(post.surfaceNormal.x == 0 && post.surfaceNormal.y == 0,
		"an isolated real camp post cannot acquire a fictitious supporting wall through its neighboring gaps");
	world.player = {4472,4621}; const auto gap = Scan(world, left, {}, Layer::Terrain);
	Check(gap.end == State::Unknown && gap.clearDepth > post.clearDepth,
		"one native cell bypasses the post, but the uncaptured far map remains unknown");
	Check(world.Body(4466,4628) == State::Blocked && world.Cell(4466,4628) == State::Open,
		"the real obstacle strikes the player's side footprint even though the central ray cell is empty");
}
}
int main(int argc, char** argv) {
	const std::string_view test = argc > 1 ? argv[1] : "all";
	if (test == "all" || test == "wall") WallAtAngle();
	if (test == "all" || test == "clip") ClippedSamples();
	if (test == "all" || test == "corner") CornerPass();
	if (test == "all" || test == "slide") SlidePastObject();
	if (test == "all" || test == "partial") PartialPlayback();
	if (test == "all" || test == "facets") AdjacentFacets();
	if (test == "all" || test == "camp") RecordedCampPost();
	Passed();
}
