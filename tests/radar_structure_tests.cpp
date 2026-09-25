#include "radar.h"
#include "radar_passage.h"
#include "recorded_alcove_grid.h"
#include "recorded_return_grid.h"
#include "test_support.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace D2RAccess;
using namespace Radar;
namespace {
constexpr int Size = 240;
// Native PLAYER_PATH bits: WALL | NO_PLAYER | OBJECT | DOOR | NO_PATH.
constexpr uint16_t PlayerPath = 0x1C09, Wall = 1, Object = 0x400, Door = 0x800;
// The rest of the native collision word, named where these tests need it.
constexpr uint16_t Blank = 0x20, Missile = 0x40, Player = 0x80, Monster = 0x100, NoPath = 0x1000, Pet = 0x2000;
constexpr uint16_t Water = Missile | Player; // native water marker: two shared bits, not its own
constexpr size_t Index(Direction direction) { return static_cast<size_t>(direction); }
constexpr size_t Down = Index(Direction::Down);

World Field(int pattern = 1) {
	World world;
	world.session = world.level = 1;
	world.player = {100, 100};
	world.bodyPattern = pattern;
	world.walkingMask = PlayerPath;
	world.rooms.push_back({0, 0, Size, Size, std::vector<uint16_t>(Size * Size)});
	return world;
}
void Put(World& world, int x, int y, uint16_t flags) {
	world.rooms[0].cells[static_cast<size_t>(y) * Size + x] = flags;
}
void Rectangle(World& world, int x, int y, int width, int height, uint16_t flags) {
	for (int dy = 0; dy < height; ++dy) for (int dx = 0; dx < width; ++dx) Put(world, x + dx, y + dy, flags);
}
bool Silent(const Events& events) {
	return std::all_of(events.begin(), events.end(), [](State state) { return state == State::Unknown; });
}
Navigation::Grid Analyze(const World& world) {
	const double reach = std::max(Settings{}.openDepth, Settings{}.reachDepth);
	return Navigation::Build(world, PassageRadius(reach, world.bodyPattern));
}

void IsolatedObjects() {
	for (int pattern : {0, 1, 2, 3, 4, 5}) for (uint16_t flags : {Wall, Object, uint16_t(Wall | Object)}) {
		auto world = Field(pattern);
		Rectangle(world, 108, 108, 3, 2, flags);
		Check(StructuralWorld(world, {}).Cell(108, 108) == State::Open,
			"bypassable small furniture is absent from automatic structural geometry for every native body pattern");
		Check(world.Body(108, 108) == State::Blocked, "structural filtering never changes the physical source map");
		world.slides = false;
		Check(StructuralWorld(world, {}).Cell(108, 108) == State::Blocked,
			"when a direct move stops at obstacles instead of sliding round them, small furniture is geometry again");
	}
	// One native map tile is the whole classification scale now: anything wider
	// is geometry, whatever collision bit it carries.
	constexpr int Tile = 5;
	auto world = Field();
	Rectangle(world, 108, 104, Tile, Tile, Object);
	Check(StructuralWorld(world, {}).Cell(108, 104) == State::Open,
		"an object up to one map tile across is bypassable clutter");
	Rectangle(world, 108, 104, Tile, Tile, 0);
	Rectangle(world, 108, 104, Tile + 1, Tile + 1, Object);
	Check(StructuralWorld(world, {}).Cell(108, 104) == State::Blocked,
		"one cell wider than a map tile is already too big to ignore");
	Rectangle(world, 108, 104, Tile + 1, Tile + 1, 0);
	Rectangle(world, 108, 104, 20, 20, Object);
	Check(StructuralWorld(world, {}).Cell(108, 104) == State::Blocked,
		"a large block of pure object collision is retained like any other structure");
	Rectangle(world, 108, 104, 20, 20, 0);
	Rectangle(world, 108, 104, 7, 12, Wall);
	Check(StructuralWorld(world, {}).Cell(108, 104) == State::Blocked,
		"a building larger than one map tile is preserved despite an exterior route around it");
	Rectangle(world, 108, 104, 7, 12, 0);
	Rectangle(world, 105, 105, 21, 2, Object);
	Check(StructuralWorld(world, {}).Cell(105, 105) == State::Blocked,
		"objects extending beyond the local classification scale remain structural");
}

void Barriers() {
	for (uint16_t flags : {Door, uint16_t(0x8), uint16_t(0x1000), uint16_t(Object | Door), uint16_t(Wall | 0x8)}) {
		auto world = Field();
		Put(world, 105, 105, flags);
		Check(StructuralWorld(world, {}).Cell(105, 105) == State::Blocked,
			"doors, no-player barriers and independent no-path cells are never treated as furniture");
	}
	for (int pattern : {1, 2}) {
		auto world = Field(pattern);
		Rectangle(world, 0, 100, Size, 1, Wall);
		Rectangle(world, 0, 106, Size, 1, Wall);
		Put(world, 108, 103, Object);
		Check(StructuralWorld(world, {}).Cell(108, 103) == State::Blocked,
			"furniture plugging a corridor stays structural when the apparent gaps do not fit the body");
		world.bodyPattern = 0;
		Check(StructuralWorld(world, {}).Cell(108, 103) == State::Open,
			"a real smaller body may fit around that same obstacle");
	}
	auto world = Field();
	for (int n = 70; n <= 130; ++n) Put(world, n, n, Wall);
	Check(StructuralWorld(world, {}).Cell(105, 105) == State::Blocked, "diagonally connected wall cells remain one barrier");
	Rectangle(world, 0, 100, Size, 1, Wall);
	Put(world, 105, 101, Object);
	Check(StructuralWorld(world, {}).Cell(105, 101) == State::Blocked,
		"clutter attached to an extended wall is retained conservatively");
	world = Field();
	Rectangle(world, 104, 104, 5, 5, Wall);
	Rectangle(world, 105, 105, 3, 3, 0);
	Check(world.Body(106, 106) == State::Open && StructuralWorld(world, {}).Cell(104, 104) == State::Blocked,
		"an enclosed walkable pocket is preserved even when its entire exterior can be bypassed");
}

void CoverageAndActors() {
	auto world = Field();
	Put(world, 108, 108, Object);
	Put(world, 111, 108, 0x20);
	Check(StructuralWorld(world, {}).Cell(108, 108) == State::Blocked,
		"unknown detour coverage cannot justify ignoring a physical obstacle");
	Put(world, 111, 108, 0);
	const auto before = StructuralWorld(world, {});
	// MONSTER | NO_PATH | PET is a native actor stamp, not a new wall.
	Rectangle(world, 106, 106, 5, 1, 0x3100);
	Put(world, 108, 108, Object | 0x3100);
	const auto after = StructuralWorld(world, {});
	Check(before.Cell(108, 108) == State::Open && after.Cell(108, 108) == State::Open,
		"creatures crossing a detour do not toggle the structural classification of its furniture");
	Check(world.Cell(108, 108) == State::Blocked && world.Cell(108, 106) == State::Blocked,
		"creatures and furniture both remain in the original physical movement map");
	Put(world, 108, 108, Object | 0xC0);
	world.walkingMask |= 0xC0;
	Check(StructuralWorld(world, {}).Cell(108, 108) == State::Blocked,
		"water is never mistaken for a removable player stamp or furniture");
	// A unit's marker belongs to the unit wherever it stands. Sharing water, or a
	// cell a missile is crossing, must not promote it to terrain - and water's own
	// bits must survive the strip.
	world = Field();
	Put(world, 108, 108, static_cast<uint16_t>(Water | NoPath));
	Check(world.Cell(108, 108) == State::Blocked && world.Cell(108, 108, Layer::Terrain) == State::Open,
		"a unit marker standing in water blocks native movement but is not structural terrain");
	world.walkingMask |= Water;
	Check(world.Cell(108, 108, Layer::Terrain) == State::Blocked, "stripping that marker leaves the water itself in place");
	// The two occupancy bits the player's native mask never contains cannot close
	// a cell on either layer; NO_PATH is the one that does.
	world = Field();
	Check(!(world.walkingMask & (Monster | Pet)), "the native movement mask names neither monsters nor pets");
	for (const uint16_t occupancy : {Monster, Pet, static_cast<uint16_t>(Monster | Pet | NoPath)}) {
		Put(world, 108, 108, occupancy);
		Check(world.Cell(108, 108, Layer::Terrain) == State::Open, "no combination of occupancy bits is structural terrain");
		Check((world.Cell(108, 108) == State::Blocked) == ((occupancy & PlayerPath) != 0),
			"on the movement layer only the bits the native mask names block the player");
	}
	// Blank subtiles and unstreamed ground are unknown on both layers: never a
	// wall, never an opening, whatever else the invalid word carries.
	world = Field();
	Put(world, 108, 108, static_cast<uint16_t>(Blank | Wall));
	for (const auto layer : {Layer::Movement, Layer::Terrain}) {
		Check(world.Cell(108, 108, layer) == State::Unknown, "a blank subtile is unknown, not a wall");
		Check(world.Cell(-1, -1, layer) == State::Unknown, "ground outside every streamed room is unknown too");
	}
}

void RoomSeams() {
	auto world = Field();
	Rectangle(world, 118, 108, 4, 2, Object);
	const auto source = world.rooms.front();
	world.rooms.clear();
	for (int left : {0, Size / 2}) {
		RoomGrid room{left, 0, Size / 2, Size, std::vector<uint16_t>(static_cast<size_t>(Size) * Size / 2)};
		for (int y = 0; y < Size; ++y) for (int x = 0; x < Size / 2; ++x)
			room.cells[static_cast<size_t>(y) * (Size / 2) + x] = source.cells[static_cast<size_t>(y) * Size + left + x];
		world.rooms.push_back(std::move(room));
	}
	Check(StructuralWorld(world, {}).Cell(119, 108) == State::Open,
		"a known neighboring room completes the same object's shape and detour across a room seam");
	world.rooms.pop_back();
	Check(StructuralWorld(world, {}).Cell(119, 108) == State::Blocked,
		"unloading that neighboring room cannot create a fictitious detour at the seam");
}

void PhysicalGuardAndDoors() {
	// The screen-down ray leaves the player's cell through (101,101).
	auto world = Field();
	Put(world, 101, 101, Object);
	Check(Analyze(world).At(101, 101)->body == State::Blocked,
		"the physical map still reports the object against the native footprint");
	Check(Analyze(StructuralWorld(world, {})).At(101, 101)->body == State::Open,
		"automatic geometry sees past an isolated object the player can walk around");
	Engine engine;
	Check(Silent(engine.Tick(world, 1000)), "the structural baseline is silent");
	Check(engine.Sensors()[Down].state == State::Open, "the bypassable object never becomes a wall in that direction");
	Put(world, 101, 101, 0);
	for (uint64_t now = 1050; now <= 1200; now += PollMs)
		Check(Silent(engine.Tick(world, now)), "removing ignored furniture does not announce a new room opening");
	Put(world, 101, 101, Door);
	(void)engine.Tick(world, 1250);
	(void)engine.Tick(world, 1300);
	Check(engine.Tick(world, 1350)[Down] == State::Door, "a real door closing still sounds once it is confirmed, as a door");
	Put(world, 101, 101, 0);
	(void)engine.Tick(world, 1400);
	(void)engine.Tick(world, 1450);
	Check(engine.Tick(world, 1500)[Down] == State::Open,
		"the real doorway reopening sounds once confirmed: the cue interval spaces openings, not a door and its opening");
}

void Travel() {
	for (uint16_t flags : {Wall, Object, uint16_t(Wall | Object)}) for (int stride : {1, 3, 4}) {
		auto world = Field();
		world.player.x = 70;
		for (int x = 50; x <= 180; x += 12) {
			Rectangle(world, x, 89, 3, 2, flags);
			Rectangle(world, x + 5, 109, 2, 3, flags);
		}
		Engine engine;
		(void)engine.Tick(world, 1000);
		unsigned events = 0;
		for (int step = 1; step <= 72 / stride; ++step) {
			world.player.x += stride;
			for (const auto event : engine.Tick(world, 1000 + static_cast<uint64_t>(step) * stride * PollMs))
				events += event != State::Unknown;
		}
		Check(events == 0,
			"walking or running between bypassable objects produces no opening or closing chatter in any direction");
		for (int step = 1; step <= 72 / stride; ++step) {
			world.player.x -= stride;
			for (const auto event : engine.Tick(world, 4600 + static_cast<uint64_t>(step) * stride * PollMs))
				events += event != State::Unknown;
		}
		Check(events == 0, "returning along the same cluttered route stays silent, including behind the player");
	}
}

void RecordedReturn() {
	// A real Blood Moor capture. Nothing in it changes, so every cue the walk
	// produces must be a genuine alternation of that direction's state, and
	// standing still at either end must settle into silence.
	auto world = RecordedReturnGrid();
	const Position mouth = world.player;
	Engine engine;
	uint64_t now = 1000;
	bool consistent = true, legal = true;
	const auto step = [&](double offset) {
		world.player = {mouth.x + offset, mouth.y + offset};
		legal &= world.Body(static_cast<int>(world.player.x), static_cast<int>(world.player.y)) == State::Open;
		const auto events = engine.Tick(world, now += PollMs);
		for (size_t i = 0; i < DirectionCount; ++i) {
			if (events[i] == State::Unknown) continue;
			// Every cue is the state that was just confirmed for a direction the
			// scan could actually see; nothing is announced out of thin air.
			consistent &= engine.Sensors()[i].state == events[i] && engine.Sensors()[i].passage.available;
		}
		return events;
	};
	for (const double offset : {0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 3.0, 2.0, 1.0, 0.0}) (void)step(offset);
	Check(legal, "the recorded walk only visits cells the native body really fits in");
	Check(consistent, "every cue on the recorded walk is the confirmed state of a direction that could be seen");
	// An opening confirmed during the walk may still be waiting for its cue
	// interval, so give the engine one whole interval to flush anything pending
	// before requiring that an unchanging map says nothing more.
	const int flush = static_cast<int>(Settings{}.cueIntervalMs / PollMs) + 1;
	for (int poll = 0; poll < flush; ++poll) (void)step(0);
	bool settled = true;
	for (int poll = 0; poll < 8; ++poll) settled &= Silent(step(0));
	Check(settled, "standing still back at the start of the recorded walk falls silent and stays silent");
	for (size_t i = 0; i < DirectionCount; ++i) {
		const auto& sensor = engine.Sensors()[i];
		if (!sensor.passage.available || sensor.passage.observed == State::Unknown) continue;
		Check(sensor.state == sensor.passage.observed && sensor.candidate == State::Unknown,
			"after the map has stopped changing every confirmed state matches what is actually observed");
	}
}

void RecordedStone() {
	// The Blood Moor capture has a four-cell stone (WALL|PRESET) a few cells down
	// and right of the player, alone on open grass: clutter while the game steers
	// round it, the wall a direct move stops against when it does not.
	auto world = RecordedReturnGrid();
	const PassageQuery query{.openDepth = Settings{}.openDepth, .closeDepth = Engine::CloseDepth({}, world.bodyPattern)};
	const auto down = [&] { return ScanPassage(Analyze(StructuralWorld(world, {})), Axes[Down], query); };
	Check(down().depth >= query.openDepth, "while the game slides round it, the stone leaves the way down open");
	world.slides = false;
	Check(down().available && down().depth <= query.closeDepth,
		"when the move stops at it instead, the stone ahead is inside the warning distance");
}

void RecordedAlcove() {
	// The stone alcove of the 2026-09-20 07:56:54 live run. The game refused
	// three down inputs there while the radar reported an open route 33.9 cells
	// deep, because the wall pieces hemming the player in had been filtered out
	// as clutter on the strength of a long way round inside the window.
	auto world = RecordedAlcoveGrid();
	const Settings settings;
	const double close = Engine::CloseDepth(settings, world.bodyPattern);
	const auto ahead = [&](Position player, Direction direction) {
		world.player = player;
		return ScanPassage(Analyze(StructuralWorld(world, settings)), Axes[Index(direction)], {.openDepth = settings.openDepth, .closeDepth = close});
	};
	Check(world.Body(4465, 4625) == State::Blocked && world.Body(4461, 4625) == State::Blocked,
		"the cells the game refused to walk into really are blocked on the recorded map");
	const auto down = ahead({4464, 4624}, Direction::Down);
	Check(down.available && down.observed == State::Blocked && down.depth < close && down.depth == 0,
		"a wall piece the player cannot step around is never filtered into an opening");
	const auto left = ahead({4463, 4623}, Direction::Left);
	Check(left.available && left.observed == State::Blocked && std::abs(left.depth - Diagonal) < 1e-9,
		"the alcove's left wall stops the route at its own cells, not eight cells further on");
	// The same filter still ignores a lone prop standing in open ground.
	auto& room = world.rooms.front();
	room.cells[static_cast<size_t>(4623 - room.y) * room.width + (4472 - room.x)] = Object;
	world.player = {4468, 4623};
	Check(StructuralWorld(world, settings).Cell(4472, 4623) == State::Open,
		"clutter with a clear body reach all round it is still absent from structural geometry");
}

void WindowAndCost() {
	auto world = Field();
	Put(world, 100, 110, Object);
	bool stable = true;
	for (int x = 80; x <= 120; ++x) {
		world.player.x = x;
		stable &= StructuralWorld(world, {}).Cell(100, 110) == State::Open;
	}
	Check(stable, "a nearby obstacle stays consistently ignored as the local classification window follows the player");
	for (int y = 65; y <= 140; y += 6) for (int x = 65; x <= 140; x += 6) Put(world, x, y, Object);
	world.player = {100, 100};
	Engine engine;
	constexpr int Ticks = 200;
	const auto start = std::chrono::steady_clock::now();
	for (int tick = 0; tick < Ticks; ++tick) (void)engine.Tick(world, 1000 + tick * PollMs);
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / Ticks;
	// Reported for comparison between runs; never asserted, because a shared
	// build machine's wall clock is not a property of the radar.
	std::cout << "dense clutter average tick ms=" << ms << '\n';
}
}
int main() {
	IsolatedObjects();
	Barriers();
	CoverageAndActors();
	RoomSeams();
	PhysicalGuardAndDoors();
	Travel();
	RecordedReturn();
	RecordedStone();
	RecordedAlcove();
	WindowAndCost();
	Passed();
}
