#include "native_status.h"
#include "ground_item_layout.h"
#include "entity_layout.h"
#include "layout_check.h"
#include "native_ground_items.h"
#include "native_memory.h"
#include "async_log.h"
#include "vitals.h"

#include <windows.h>

namespace D2RAccess::NativeStatus {
namespace {
// D2R 3.3.93787: AttributeBallWidget's poison-HSV branch at 0x14C9E25
// calls this state checker with id 2 (poison) for the local player's health
// orb; it takes any states.txt row (docs/combat-status-evidence.md).
constexpr uintptr_t StateChecker = 0x3351B0;
// HirelingInventoryPanel resolves relation 7 from player to a monster unit
// before drawing its equipment, experience and life statistics.
constexpr uintptr_t HirelingRelation = 0x13ABC0;
constexpr uintptr_t UnitById = 0x9A5D0;
constexpr uintptr_t HirelingLifePercent = 0x13AA50;
// HUDMonsterHealth's builder 0xCEA50 fills its bar with 0xD0640(unit, 100, 1):
// zero for a dead unit, else the life percent of 0x97490, which divides a
// non-local unit's life stat by 0x8000. docs/boss-life.md.
constexpr uintptr_t PlateFill = 0xD0640, UnitLifePercent = 0x97490;
constexpr int PlateScale = 100, PlateMinimum = 1;
constexpr int HirelingRelationType = 7;
// HirelingInventoryPanel 0x14F2B6A calls GetStat(unit, 12, 0) for its level.
constexpr int LevelStat = 12;
// Experience and the mercenary's next-level threshold: the panel 0x14F3400 asks
// GetStat for 13 (current) and 30 (next) beside 12. The HUD's experience bar
// 0x14CC8C0 takes the player's thresholds from experience.txt through
// 0x300920(expansion, class, level - 1 or level) below its cap 0x300C70.
constexpr int ExperienceStat = 13, NextExperienceStat = 30;
// ItemStatCost.txt rows 4 and 5, statpts and newskills: the attribute and skill points to spend.
constexpr int StatPointsStat = 4, NewSkillsStat = 5;
constexpr uintptr_t ExperienceThreshold = 0x300920, LevelCap = 0x300C70, UnitClassOf = 0x349860, UnitExpansion = 0x34A0E0;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; bool mayBeDetoured = false; };
constexpr Evidence PoisonChecks[] = {
	{0x14C9D5A, 1355, 0x171692F1DDA383FCULL}, // health-orb poison branch
	{StateChecker, 232, 0xE6077AA8B0D88FE4ULL},
	{0x2F6220, 172, 0x669DD90F27F01456ULL}, // active-state bitset reader
};
constexpr Evidence MercenaryChecks[] = {
	{0x14F2A60, 425, 0xCA792CA3357E8299ULL}, // hireling panel's unit binding
	{HirelingRelation, 123, 0x6B3D47A73702E114ULL},
	{UnitById, 33, 0x673F574CCEEDB47EULL},
	{HirelingLifePercent, 37, 0xDA9992529F17CB7BULL},
	{0x14F3400, 6346, 0x60012C081091103AULL}, // panel uses the getter for displayed life
	{GroundItemLayout::GetStat, 147, 0xEF955B08A436F777ULL, true},
};
constexpr Evidence ExperienceChecks[] = {
	{0x14CC8C0, 390, 0x5C18BEDE839C7CA2ULL}, // the HUD experience bar's own computation
	{ExperienceThreshold, 176, 0x22AD2A01FA40348DULL},
	{LevelCap, 167, 0x7625E9434149CB54ULL},
	{UnitClassOf, 46, 0xA487037EF27A3BC8ULL},
	{UnitExpansion, 47, 0x05F93D9EFDC375A1ULL},
	{GroundItemLayout::GetStat, 147, 0xEF955B08A436F777ULL, true},
};
constexpr Evidence MovementChecks[] = {
	{GroundItemLayout::GetStat, 147, 0xEF955B08A436F777ULL, true},
};
constexpr Evidence MonsterChecks[] = {
	{UnitById, 33, 0x673F574CCEEDB47EULL},
	{PlateFill, 304, 0x8A29F8B8A06D6F20ULL},
	{UnitLifePercent, 228, 0x76BB3F5F993669AAULL},
};
uintptr_t g_base = 0;
bool g_poisonSupported = false, g_mercenarySupported = false, g_monsterSupported = false, g_movementSupported = false;
bool g_experienceSupported = false;
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_poisonSupported = g_mercenarySupported = g_monsterSupported = g_movementSupported = g_experienceSupported = false;
	if (!context) return false;
	g_poisonSupported = LayoutCheck::Verify(context, context->exeBase, PoisonChecks,
		"status", "poison condition disabled");
	g_mercenarySupported = LayoutCheck::Verify(context, context->exeBase, MercenaryChecks,
		"status", "mercenary condition disabled");
	g_monsterSupported = LayoutCheck::Verify(context, context->exeBase, MonsterChecks,
		"status", "boss life disabled");
	g_movementSupported = LayoutCheck::Verify(context, context->exeBase, MovementChecks,
		"status", "gait and stamina disabled");
	g_experienceSupported = LayoutCheck::Verify(context, context->exeBase, ExperienceChecks,
		"status", "experience disabled");
	g_base = context->exeBase;
	AsyncLog::InfoF(context, "[status] Native condition readers: poison=%d mercenary=%d monster=%d movement=%d experience=%d.",
		g_poisonSupported, g_mercenarySupported, g_monsterSupported, g_movementSupported, g_experienceSupported);
	return g_poisonSupported || g_mercenarySupported || g_monsterSupported || g_movementSupported || g_experienceSupported;
}

namespace {
// The local player while alive and the state checker can be called, else 0.
uintptr_t StatePlayer() noexcept {
	if (!g_base || !g_poisonSupported) return 0;
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return 0;
	uint32_t type = 0, mode = 0;
	if (!NativeMemory::Peek(player + GroundItemLayout::UnitType, type)
		|| type != GroundItemLayout::PlayerType
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitMode, mode)
		|| mode == GroundItemLayout::Death || mode == GroundItemLayout::Dead
		|| !NativeMemory::Executable(g_base + StateChecker)) return 0;
	return player;
}

// 1 when the unit has the state, 0 when not, -1 when the call faulted.
int CheckState(uintptr_t unit, int state) noexcept {
	__try {
		return reinterpret_cast<int(*)(uintptr_t, int)>(g_base + StateChecker)(unit, state) != 0 ? 1 : 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
}

std::optional<std::vector<size_t>> PlayerStates(std::span<const int> states) {
	const auto player = StatePlayer();
	if (!player) return std::nullopt;
	std::vector<size_t> active;
	for (size_t i = 0; i < states.size(); ++i) {
		const int on = CheckState(player, states[i]);
		if (on < 0) return std::nullopt;
		if (on) active.push_back(i);
	}
	return active;
}

std::optional<Movement> PlayerMovement() noexcept {
	if (!g_base || !g_movementSupported) return std::nullopt;
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return std::nullopt;
	uint32_t type = 0, mode = 0;
	if (!NativeMemory::Peek(player + GroundItemLayout::UnitType, type)
		|| type != GroundItemLayout::PlayerType
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitMode, mode)
		|| mode == GroundItemLayout::Death || mode == GroundItemLayout::Dead) return std::nullopt;
	Movement movement;
	movement.walking = mode == GroundItemLayout::Walk || mode == GroundItemLayout::TownWalk;
	movement.running = mode == GroundItemLayout::Run;
	__try {
		const auto stat = reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GroundItemLayout::GetStat);
		movement.stamina = stat(player, GroundItemLayout::StaminaStat, 0);
		movement.maxStamina = stat(player, GroundItemLayout::MaxStaminaStat, 0);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	return movement;
}

namespace {
bool ReadStats(uintptr_t player, std::span<const int> stats, int* values) noexcept {
	__try {
		const auto stat = reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GroundItemLayout::GetStat);
		for (size_t i = 0; i < stats.size(); ++i) values[i] = stat(player, stats[i], 0);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
	return true;
}
}

std::optional<Stats> PlayerStats(std::span<const int> stats) {
	if (!g_base || !g_movementSupported || stats.size() > MaxStats) return std::nullopt;
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return std::nullopt;
	uint32_t type = 0, mode = 0;
	if (!NativeMemory::Peek(player + GroundItemLayout::UnitType, type)
		|| type != GroundItemLayout::PlayerType
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitMode, mode)
		|| mode == GroundItemLayout::Death || mode == GroundItemLayout::Dead) return std::nullopt;
	int values[MaxStats]{};
	if (!ReadStats(player, stats, values)) return std::nullopt;
	using namespace GroundItemLayout;
	Stats read{{values, values + stats.size()},
		mode == Attack1 || mode == Attack2 || mode == Throw || mode == Kick || mode == Sequence
			|| (mode >= Special1 && mode <= Special4)};
	return read;
}

std::optional<Orbs> PlayerOrbs() noexcept {
	if (!g_base || !g_movementSupported) return std::nullopt;
	uintptr_t player = 0, tables = 0;
	uint32_t type = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitType, type) || type != GroundItemLayout::PlayerType) return std::nullopt;
	constexpr int stats[] = {GroundItemLayout::LifeStat, GroundItemLayout::MaxLifeStat, GroundItemLayout::ManaStat, GroundItemLayout::MaxManaStat};
	int values[std::size(stats)]{};
	if (!ReadStats(player, stats, values)) return std::nullopt;
	return Orbs{values[0], values[1], values[2], values[3]};
}

std::optional<Experience> PlayerExperience() noexcept {
	if (!g_base || !g_experienceSupported) return std::nullopt;
	uintptr_t player = 0, tables = 0;
	uint32_t type = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitType, type) || type != GroundItemLayout::PlayerType) return std::nullopt;
	Experience experience;
	__try {
		const auto stat = reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GroundItemLayout::GetStat);
		const auto threshold = reinterpret_cast<uint32_t(*)(uint8_t, uint32_t, int)>(g_base + ExperienceThreshold);
		experience.level = stat(player, LevelStat, 0);
		experience.total = static_cast<uint32_t>(stat(player, ExperienceStat, 0));
		const uint32_t characterClass = reinterpret_cast<uint32_t(*)(uintptr_t)>(g_base + UnitClassOf)(player);
		const uint8_t expansion = reinterpret_cast<uint8_t(*)(uintptr_t)>(g_base + UnitExpansion)(player);
		const int cap = static_cast<int>(reinterpret_cast<uint32_t(*)(uint8_t, uint32_t)>(g_base + LevelCap)(expansion, characterClass));
		if (experience.level < 1) return std::nullopt;
		if (experience.level >= cap) { experience.maximum = true; return experience; }
		experience.levelStart = threshold(expansion, characterClass, experience.level - 1);
		experience.nextLevel = threshold(expansion, characterClass, experience.level);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	experience.percent = Vitals::ExperiencePercent(experience.total, experience.levelStart, experience.nextLevel);
	return experience;
}

std::optional<Unspent> PlayerUnspent() noexcept {
	if (!g_base || !g_experienceSupported) return std::nullopt;
	uintptr_t player = 0, tables = 0;
	uint32_t type = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player
		|| !NativeMemory::Peek(player + GroundItemLayout::UnitType, type) || type != GroundItemLayout::PlayerType) return std::nullopt;
	Unspent points{-1, -1};
	__try {
		const auto stat = reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GroundItemLayout::GetStat);
		points.attributes = stat(player, StatPointsStat, 0);
		points.skills = stat(player, NewSkillsStat, 0);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	if (points.attributes < 0 || points.skills < 0) return std::nullopt;
	return points;
}

std::optional<Mercenary> MercenaryLife() noexcept {
	if (!g_base || !g_mercenarySupported) return std::nullopt;
	uintptr_t player = 0, tables = 0, unit = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return std::nullopt;
	int id = -1;
	__try {
		id = reinterpret_cast<int(*)(uintptr_t, int, int)>(g_base + HirelingRelation)(player, HirelingRelationType, 0);
		if (id >= 0) unit = reinterpret_cast<uintptr_t(*)(uint32_t, int)>(g_base + UnitById)(id, EntityLayout::MonsterType);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	uint32_t type = 0, actualId = 0, mode = 0;
	if (!unit || !NativeMemory::Peek(unit + GroundItemLayout::UnitType, type)
		|| type != EntityLayout::MonsterType || !NativeMemory::Peek(unit + GroundItemLayout::UnitId, actualId)
		|| actualId != static_cast<uint32_t>(id)
		|| !NativeMemory::Peek(unit + GroundItemLayout::UnitMode, mode)) return std::nullopt;
	const bool dead = mode == EntityLayout::MonsterDeath || mode == EntityLayout::MonsterDead;
	int level = -1, lifePercent = -1;
	uint32_t total = 0, next = 0;
	__try {
		const auto stat = reinterpret_cast<int(*)(uintptr_t, int, uint32_t)>(g_base + GroundItemLayout::GetStat);
		level = stat(unit, LevelStat, 0);
		total = static_cast<uint32_t>(stat(unit, ExperienceStat, 0));
		next = static_cast<uint32_t>(stat(unit, NextExperienceStat, 0));
		if (!dead) lifePercent = reinterpret_cast<int(*)(uint32_t)>(g_base + HirelingLifePercent)(actualId);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	if (dead) lifePercent = 0;
	if (lifePercent < 0 || lifePercent > 100) lifePercent = -1;
	const uint64_t start = Vitals::MercenaryLevelStart(level, next);
	const int experiencePercent = start ? Vitals::ExperiencePercent(total, start, next) : -1;
	return Mercenary{actualId, lifePercent, level, experiencePercent};
}

std::optional<int> MonsterLife(uint32_t id) noexcept {
	if (!g_base || !g_monsterSupported) return std::nullopt;
	uintptr_t unit = 0;
	__try {
		unit = reinterpret_cast<uintptr_t(*)(uint32_t, int)>(g_base + UnitById)(id, EntityLayout::MonsterType);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	uint32_t type = 0, actualId = 0;
	if (!unit || !NativeMemory::Peek(unit + GroundItemLayout::UnitType, type) || type != EntityLayout::MonsterType
		|| !NativeMemory::Peek(unit + GroundItemLayout::UnitId, actualId) || actualId != id) return std::nullopt;
	int percent = -1;
	__try {
		percent = reinterpret_cast<int(*)(uintptr_t, int, int)>(g_base + PlateFill)(unit, PlateScale, PlateMinimum);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return std::nullopt; }
	if (percent < 0 || percent > PlateScale) return std::nullopt;
	return percent;
}

}
