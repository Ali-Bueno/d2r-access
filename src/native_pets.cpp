#include "native_pets.h"

#include "async_log.h"
#include "entity_layout.h"
#include "ground_item_layout.h"
#include "layout_check.h"
#include "localization.h"
#include "native_ground_items.h"
#include "native_memory.h"
#include "ui_text.h"

#include <windows.h>
#include <array>

namespace D2RAccess::NativePets {
namespace {
using NativeMemory::Peek;
using namespace EntityLayout;

// The list head, a qword global; node by id 0x13B270 loads it. docs/summons-and-corpses-re.md §1.
constexpr uintptr_t ListHead = 0x2A4DC10;
constexpr size_t NodeClass = 0x00, NodeType = 0x04, NodeId = 0x08, NodeOwner = 0x0C, NodeLife = 0x1C,
	NodeDeadHireling = 0x20, NodeNext = 0x30;
// The game has no cap; a cycle guard over eight players' large armies.
constexpr size_t MaxPets = 1024;
// PetType rows (accessor 0x13A4F0): pointer and count in the table bank, stride 200.
constexpr size_t PetTypes = 0x12D8, PetTypeCount = 0x12E0, PetTypeStride = 200;
constexpr size_t KindFlags = 0x04, KindName = 0x10, KindIcon = 0x12;
constexpr uint8_t DrawHp = 1u << 5;
constexpr uint16_t NoName = 5382; // ui.json "dummy", the rows with no name of their own
constexpr uint32_t MaxPetTypes = 256;
// Hostility 0x971E0: Unit+0x128 & 0x400 with owner type 0 (a player) at +0xE8.
constexpr uint32_t OwnerFlag = 0x400;
constexpr size_t UnitOwnerType = 0xE8;
constexpr uint32_t PlayerOwnerType = 0;

struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence ListChecks[] = {
	{0x13B270, 33, 0x17DFFFA847B8AD90ULL},  // node by id: the head, id +8, next +0x30
	{0x13ABA0, 32, 0xEF297C9F25E10636ULL},  // owner +0xC
	{0x13AB80, 27, 0x3F52C1F982EDB58AULL},  // type +4
	{0x13AA80, 27, 0xC210D535C7CBDBE5ULL},  // class +0
	{0x13AA50, 37, 0xDA9992529F17CB7BULL},  // life +0x1C
	{0x13A5A0, 179, 0x4AC276CB45359F2EULL}, // node create: 0x38 bytes, life 100, pushed at the head
	{0x13AEC0, 173, 0x640B7385EF906B52ULL}, // remove; the own dead hireling flagged +0x20
};
constexpr Evidence KindChecks[] = {
	{0x13A4F0, 169, 0x41641BC29FB5E290ULL}, // PetType rows
	{0x1E75F0, 697, 0x719F3C566D8354C4ULL}, // portraits by icontype +0x12
	{0x1E6560, 341, 0x65A33E3907E2FF09ULL}, // drawhp
	{0x1E66C0, 141, 0x76D2ECDB0E4C5E6BULL}, // name +0x10, 5382 skipped
	{0x976E0, 169, 0xFF5A28122B22038AULL},  // MonStats row
};
// The unit by id and type, as HirelingInventoryPanel finds its mercenary (native_status.cpp).
constexpr uintptr_t UnitById = 0x9A5D0;
constexpr Evidence PositionChecks[] = {
	{UnitById, 33, 0x673F574CCEEDB47EULL},
};
constexpr Evidence OwnerChecks[] = {
	{0x971E0, 675, 0x702AD84E5A3174ACULL},
	{0x34FB00, 54, 0x94781FF2A3108387ULL}, // Unit+0x128 mask test
};
uintptr_t g_base = 0;
bool g_list = false, g_kinds = false, g_owners = false, g_positions = false;

uintptr_t Tables() {
	uintptr_t player = 0, tables = 0;
	return NativeGroundItems::RuntimeRoots(player, tables) ? tables : 0;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	if (context && g_base && g_base == context->exeBase) return g_list; // several readers ask
	g_base = 0;
	g_list = g_kinds = g_owners = g_positions = false;
	if (!context) return false;
	g_list = LayoutCheck::Verify(context, context->exeBase, ListChecks, "pets", "summons not listed or told from NPCs");
	g_kinds = LayoutCheck::Verify(context, context->exeBase, KindChecks, "pets", "summons not grouped");
	g_owners = LayoutCheck::Verify(context, context->exeBase, OwnerChecks, "pets", "player-owned monsters told by the list alone");
	g_positions = LayoutCheck::Verify(context, context->exeBase, PositionChecks, "pets", "where summons and party members stand not said");
	g_base = context->exeBase;
	AsyncLog::InfoF(context, "[pets] Native readers: list=%d kinds=%d owners=%d positions=%d.", g_list, g_kinds, g_owners, g_positions);
	return g_list;
}

std::optional<std::vector<Pet>> All() {
	if (!g_base || !g_list) return std::nullopt;
	const NativeMemory::ReadScope scope;
	std::vector<Pet> pets;
	uintptr_t node = 0;
	if (!Peek(g_base + ListHead, node)) return std::nullopt;
	for (size_t i = 0; node; ++i) {
		if (i >= MaxPets) return std::nullopt;
		Pet pet;
		int32_t dead = 0;
		if (!Peek(node + NodeClass, pet.classId) || !Peek(node + NodeType, pet.type) || !Peek(node + NodeId, pet.id)
			|| !Peek(node + NodeOwner, pet.owner) || !Peek(node + NodeLife, pet.life) || !Peek(node + NodeDeadHireling, dead)
			|| !Peek(node + NodeNext, node)) return std::nullopt;
		pet.deadHireling = dead != 0;
		pets.push_back(pet);
	}
	return pets;
}

bool PlayerOwned(uintptr_t unit) {
	if (!g_owners || !unit) return false;
	uint32_t flags = 0, ownerType = 0;
	return Peek(unit + UnitFlags2, flags) && (flags & OwnerFlag) && Peek(unit + UnitOwnerType, ownerType) && ownerType == PlayerOwnerType;
}

std::optional<Kind> KindOf(int type) {
	if (!g_kinds || type < 0) return std::nullopt;
	const NativeMemory::ReadScope scope;
	const uintptr_t tables = Tables();
	uintptr_t rows = 0;
	uint32_t count = 0;
	if (!tables || !Peek(tables + PetTypes, rows) || !rows || !Peek(tables + PetTypeCount, count)
		|| count > MaxPetTypes || static_cast<uint32_t>(type) >= count) return std::nullopt;
	const uintptr_t row = rows + static_cast<uintptr_t>(type) * PetTypeStride;
	uint8_t flags = 0, icon = 0;
	uint16_t name = 0;
	if (!Peek(row + KindFlags, flags) || !Peek(row + KindName, name) || !Peek(row + KindIcon, icon)) return std::nullopt;
	Kind kind{icon, (flags & DrawHp) != 0, {}};
	if (name && name != NoName) kind.name = UiText::Clean(Localization::Text(uint32_t{name}));
	return kind;
}

std::string ClassName(uint16_t classId) {
	if (!g_kinds) return {};
	const NativeMemory::ReadScope scope;
	const uintptr_t tables = Tables();
	uintptr_t rows = 0;
	uint64_t count = 0;
	uint16_t name = 0;
	if (!tables || !Peek(tables + MonStats, rows) || !rows || !Peek(tables + MonStatCount, count) || classId >= count
		|| !Peek(rows + static_cast<uintptr_t>(classId) * MonStatStride + MonNameId, name) || !name || name == NoName) return {};
	return UiText::Clean(Localization::Text(uint32_t{name}));
}

namespace {
uintptr_t UnitOf(uint32_t id, uint32_t type) noexcept {
	__try { return reinterpret_cast<uintptr_t(*)(uint32_t, uint32_t)>(g_base + UnitById)(id, type); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
}

std::optional<Radar::Position> PositionOf(uint32_t id, uint32_t type) {
	if (!g_base || !g_positions || !NativeMemory::Executable(g_base + UnitById)) return std::nullopt;
	const NativeMemory::ReadScope scope;
	const uintptr_t unit = UnitOf(id, type);
	uintptr_t path = 0;
	std::array<uint32_t, 2> packed{}; // 16.16 X and Y at the dynamic path's start
	if (!unit || !Peek(unit + GroundItemLayout::UnitPath, path) || !path || !Peek(path, packed)) return std::nullopt;
	return Radar::Position{packed[0] / GroundItemLayout::PositionScale, packed[1] / GroundItemLayout::PositionScale};
}

}
