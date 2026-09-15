#include "native_world.h"
#include "native_memory.h"
#include "async_log.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace D2RAccess::NativeWorld {
namespace {
using NativeMemory::Peek;
// Installed D2R 3.3.93787 / D2RCore 1.2.1-beta disassembly, docs/sonar-radar.md.
// Slots are used by the SDK getLocalPlayer wrapper at Core RVA 0x372C44/4D.
constexpr uintptr_t PlayerIndexSlot = 0x5A8CF8, PlayerLookupSlot = 0x5A8D10;
constexpr uintptr_t PlayerLookupRva = 0x9A480;
constexpr size_t UnitType = 0, UnitId = 8, UnitPath = 0x38;
// Collision getters 0x341A20/30 read unsigned words from the high halves of
// the fixed-point coordinates. Path+8/+C instead holds projected coordinates.
constexpr size_t PathPosition = 0, PathRoom = 0x20, PathPattern = 0x5C;
// 0x382AA0 forms (subtile << 16) + 0x8000, then 0x3818E0 stores the
// packed X/Y qword at Path+0. Retain low words for continuous motion.
constexpr unsigned FractionBits = std::numeric_limits<uint16_t>::digits;
constexpr uint32_t FractionMask = std::numeric_limits<uint16_t>::max();
constexpr double PositionScale = static_cast<double>(FractionMask) + 1;
constexpr size_t PathFootprintMask = 0x60; // getter 0x341860; setter 0x342670
constexpr size_t PathMovementMask = 0x64; // movement wrapper 0x3C5610; setter 0x342740
constexpr size_t RoomNear = 0, RoomNearCount = 0x40, RoomGrid = 0x38, RoomExtended = 0x18;
constexpr size_t ExtendedLevel = 0x90, LevelId = 0x1F8;
constexpr int MaxCoordinate = std::numeric_limits<uint16_t>::max(); // native movzx word getters
constexpr int MaxRoomSide = 1024, MaxNeighbors = 64, MaxRooms = 256; // corruption/work bounds
constexpr size_t MaxCopiedCells = 512 * 1024; // <= 1 MiB per local snapshot
constexpr double SnapshotRadius = 152; // default 150-subtile radar plus footprint
uintptr_t g_indexFunction = 0, g_playerFunction = 0;

struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
// FNV-1a of complete recovered accessors/readers. Verify before consuming their
// layout: a new executable must fail quiet until its structures are rechecked.
constexpr Evidence Checks[] = {
	{0x9A480, 165, 0xD7C8BF425C24EF8FULL},
	{0x2EFB30, 5, 0x71FA801FF8C90D42ULL},
	{0x2EFDE0, 13, 0x0746FEFCB398B135ULL},
	{0x2F0680, 62, 0xF0ADAC690E656CD0ULL},
	{0x341A20, 5, 0xCE32643327F7AF15ULL},
	{0x341A30, 5, 0xCE264C3327EEAEF1ULL},
	{0x382AA0, 374, 0x91726DCDA3AFD43EULL}, // subtile centre to 16.16 position
	{0x3818E0, 310, 0x260F7E3A027B8F70ULL}, // packed position setter
	{0x341870, 17, 0x850E898DB8A253A0ULL},
	{0x341860, 14, 0xF2D1DCF8A918C439ULL},
	{0x341D70, 255, 0xA39B91A111FA8DD3ULL}, // restores live monster footprint 0x100
	{0x341600, 182, 0x8C1C53B07AF3DF19ULL}, // replaces monster footprint with corpse 0x8000
	{0x363CF0, 344, 0x652C884296D3F7EDULL},
	{0x3636D0, 353, 0x07A933B5F15EAE54ULL},
	{0x3C5610, 54, 0x49DB70EEE3861E43ULL},
	{0x342740, 4, 0x139702E42DDC56F6ULL},
	{0x366250, 148, 0x92BE7DBD292A006FULL},
	{0x366D40, 895, 0x4A0CFAC2C8F9877CULL},
	{0x364C70, 197, 0x17506F77209FE3D8ULL},
};
bool Verify(uintptr_t base, Evidence check) {
	std::vector<uint8_t> bytes(check.size);
	if (!NativeMemory::Copy(base + check.rva, bytes.data(), bytes.size())) return false;
	constexpr uint64_t FnvBasis = 14695981039346656037ULL, FnvPrime = 1099511628211ULL;
	uint64_t hash = FnvBasis;
	for (auto byte : bytes) hash = (hash ^ byte) * FnvPrime;
	return hash == check.hash;
}
uintptr_t LocalPlayer() noexcept {
	__try {
		const unsigned index = reinterpret_cast<unsigned(*)()>(g_indexFunction)();
		// 0x9A480 explicitly supports eight local-player indices.
		return index < 8 ? reinterpret_cast<uintptr_t(*)(unsigned)>(g_playerFunction)(index) : 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool ReadRoom(uintptr_t room, Radar::Position position, Radar::RoomGrid& output) {
	uintptr_t grid = 0;
	struct GridHeader { int x, y, width, height, tileX, tileY, tileWidth, tileHeight; uintptr_t data; } header{};
	if (!Peek(room + RoomGrid, grid) || !grid || !Peek(grid, header)) return false;
	if (header.x < 0 || header.y < 0 || header.x > MaxCoordinate || header.y > MaxCoordinate
		|| header.width <= 0 || header.height <= 0 || header.width > MaxRoomSide || header.height > MaxRoomSide) return false;
	if (position.x + SnapshotRadius < header.x || position.y + SnapshotRadius < header.y
		|| position.x - SnapshotRadius >= header.x + header.width || position.y - SnapshotRadius >= header.y + header.height) return false;
	output = {header.x, header.y, header.width, header.height, {}};
	const size_t count = static_cast<size_t>(header.width) * header.height;
	if (count > MaxCopiedCells) return false;
	output.cells.resize(count);
	return NativeMemory::Copy(header.data, output.cells.data(), output.cells.size() * sizeof(uint16_t));
}
void RemovePlayerPresence(Radar::World& world, Diagnostics& detail) {
	// Native set/reset pattern routines 0x363CF0 / 0x3636D0 add/remove a
	// separate presence marker: centre for patterns 1/3, cross for 2/4.
	// Remove the footprint and this local marker in our copies, matching the
	// native reset before movement testing. Never strip NO_PATH
	// globally, or clear wall/door/object flags that share the player's cell.
	constexpr uint16_t NoPath = 0x1000, PetPresence = 0x2000;
	const bool cross = world.bodyPattern == 2 || world.bodyPattern == 4;
	const uint16_t marker = world.bodyPattern == 1 || world.bodyPattern == 2 ? NoPath
		: world.bodyPattern == 3 || world.bodyPattern == 4 ? PetPresence : 0;
	for (auto& room : world.rooms) {
		for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
			const int x = static_cast<int>(world.player.x) + dx - room.x;
			const int y = static_cast<int>(world.player.y) + dy - room.y;
			if (x < 0 || y < 0 || x >= room.width || y >= room.height) continue;
			auto& cell = room.cells[static_cast<size_t>(y) * room.width + x];
			const bool centre = dx == 0 && dy == 0;
			if (centre) detail.rawPlayerCell = cell;
			const bool inFootprint = world.bodyPattern != 0
				&& (cross || dx == 0 || dy == 0);
			if (!inFootprint || !detail.footprintMask || (cell & detail.footprintMask) != detail.footprintMask) continue;
			const bool inPresence = centre || (cross && (dx == 0 || dy == 0));
			const uint16_t remove = static_cast<uint16_t>(detail.footprintMask | (inPresence ? marker : 0));
			if (cell & marker & remove) ++detail.removedPresenceCells;
			cell &= static_cast<uint16_t>(~remove);
		}
	}
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_indexFunction = g_playerFunction = 0;
	if (!context) return false;
	const auto base = context->exeBase;
	IMAGE_DOS_HEADER dos{};
	IMAGE_NT_HEADERS64 nt{};
	if (!Peek(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0
		|| !Peek(base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE) return false;
	const uintptr_t end = base + nt.OptionalHeader.SizeOfImage;
	for (auto check : Checks) {
		if (!Verify(base, check)) {
			AsyncLog::WarnF(context, "[radar] Unsupported world layout at RVA %llX; sonar disabled.", static_cast<unsigned long long>(check.rva));
			return false;
		}
	}
	const auto core = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"D2RCore.dll"));
	uintptr_t index = 0, player = 0;
	if (!core || !Peek(core + PlayerIndexSlot, index) || !Peek(core + PlayerLookupSlot, player)
		|| player != base + PlayerLookupRva || !NativeMemory::Executable(index)) return false;
	// Loader-resolved index accessor: mov eax,[rip+disp32]; ret. Its target
	// belongs to this executable; the data address is derived, never guessed.
	std::array<uint8_t, 7> code{};
	int32_t displacement = 0;
	if (!NativeMemory::Copy(index, code.data(), code.size()) || code[0] != 0x8B || code[1] != 0x05 || code[6] != 0xC3
		|| !Peek(index + 2, displacement) || index < base || index >= end) return false;
	const uintptr_t target = index + 6 + displacement;
	if (target < base || target >= end) return false;
	g_indexFunction = index;
	g_playerFunction = player;
	AsyncLog::Info(context, "[radar] Verified live room collision reader; no map generation required.");
	return true;
}

bool ReadPlayer(uintptr_t player, uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics) {
	output = {};
	Diagnostics local;
	auto& detail = diagnostics ? *diagnostics : local;
	detail = {};
	uint32_t type = ~0U, id = 0;
	uintptr_t path = 0, room = 0, extended = 0, nativeLevel = 0;
	auto& x = detail.x; auto& y = detail.y; auto& pattern = detail.pattern; auto& actualLevel = detail.level;
	detail.stage = "player";
	if (!player || !Peek(player + UnitType, type) || type != 0 || !Peek(player + UnitId, id)) return false;
	detail.playerId = id;
	detail.stage = "player-identity";
	if (id != playerId) return false;
	detail.stage = "path";
	if (!Peek(player + UnitPath, path) || !path) return false;
	detail.stage = "position";
	struct FixedPosition { uint32_t x, y; } position{};
	if (!Peek(path + PathPosition, position)) return false;
	x = position.x >> FractionBits;
	y = position.y >> FractionBits;
	detail.stage = "footprint";
	if (!Peek(path + PathPattern, pattern) || pattern < 0 || pattern > 5) return false;
	detail.stage = "footprint-mask";
	if (!Peek(path + PathFootprintMask, detail.footprintMask) || detail.footprintMask > std::numeric_limits<uint16_t>::max()) return false;
	detail.stage = "movement-mask";
	if (!Peek(path + PathMovementMask, detail.movementMask) || detail.movementMask > std::numeric_limits<uint16_t>::max()) return false;
	detail.stage = "room";
	if (!Peek(path + PathRoom, room) || !room || !Peek(room + RoomExtended, extended) || !extended) return false;
	detail.stage = "level";
	if (!Peek(extended + ExtendedLevel, nativeLevel) || !nativeLevel || !Peek(nativeLevel + LevelId, actualLevel) || actualLevel <= 0) return false;
	detail.stage = "level-mismatch";
	if (level < 0 || (level > 0 && actualLevel != level)) return false;
	output.player = {static_cast<double>(x), static_cast<double>(y)};
	output.subtileFraction = {(position.x & FractionMask) / PositionScale, (position.y & FractionMask) / PositionScale};
	output.session = session;
	output.level = actualLevel;
	output.bodyPattern = pattern;
	output.walkingMask = static_cast<uint16_t>(detail.movementMask);
	std::vector<uintptr_t> pending{room};
	size_t copied = 0;
	detail.stage = "collision-grid";
	for (size_t i = 0; i < pending.size(); ++i) {
		Radar::RoomGrid grid;
		if (!ReadRoom(pending[i], output.player, grid)) {
			if (i == 0) { output = {}; return false; }
			continue;
		}
		if (copied + grid.cells.size() > MaxCopiedCells) break;
		copied += grid.cells.size();
		output.rooms.push_back(std::move(grid));
		uintptr_t neighbors = 0;
		uint32_t count = 0;
		if (!Peek(pending[i] + RoomNear, neighbors) || !Peek(pending[i] + RoomNearCount, count) || count > MaxNeighbors) continue;
		std::array<uintptr_t, MaxNeighbors> rooms{};
		if (count && !NativeMemory::Copy(neighbors, rooms.data(), count * sizeof(uintptr_t))) continue;
		for (size_t n = 0; n < count && pending.size() < MaxRooms; ++n) {
			if (rooms[n] && std::find(pending.begin(), pending.end(), rooms[n]) == pending.end()) pending.push_back(rooms[n]);
		}
	}
	RemovePlayerPresence(output, detail);
	detail.stage = "player-cell";
	if (output.Cell(x, y) == Radar::State::Unknown) { output = {}; return false; }
	detail.stage = "ready";
	return true;
}

bool Read(uint32_t playerId, uint64_t session, int level, Radar::World& output, Diagnostics* diagnostics) {
	if (!g_indexFunction || !g_playerFunction) {
		output = {};
		if (diagnostics) { *diagnostics = {}; diagnostics->stage = "not-initialized"; }
		return false;
	}
	return ReadPlayer(LocalPlayer(), playerId, session, level, output, diagnostics);
}
}
