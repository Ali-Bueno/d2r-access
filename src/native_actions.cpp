#include "native_actions.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_ground_items.h"
#include "native_memory.h"

#include <windows.h>
#include <intrin.h>

#include <atomic>
#include <cmath>

namespace D2RAccess::NativeActions {
namespace {
// D2R 3.3.93787, read from the image; docs/keyboard-actions.md has the
// evidence. Command(op, player, a, b): a and b are a cell's x and y for a
// command aimed at a place, a unit's type and id for one aimed at a unit.
constexpr uintptr_t CommandRva = 0xFABE0, HaltRva = 0xFE080;
// What a click on the ground calls: MoveToPoint(command, player, x, y) has the
// game find its own path to the cell, then issues the command to where that ends.
constexpr uintptr_t MoveToPointRva = 0x101EF0;
// The player's skills (0x34A540, 0x34B400): the list at unit+0x100 holds the
// left hand's skill at +8 and the right hand's at +0x10; a skill's first field
// points at its skills.txt record, which begins with its 16-bit id (0x33E080).
constexpr size_t UnitSkills = 0x100, SkillsLeft = 0x08, SkillsRight = 0x10;
// The command numbers are the game's own client-to-server message numbers.
constexpr uint32_t Walk = 0x01, Run = 0x03;
constexpr uint32_t LeftAt = 0x05, LeftOn = 0x06, RightAt = 0x0C, RightOn = 0x0D;
constexpr uint32_t TakeItem = 0x16, TalkTo = 0x2F, UseTile = 0x40, UseObject = 0x41;
// A player unit in reach: the game's click (0x140101E29) sends 0x13, whose
// server handler is in PlrMsg.cpp, to loot a corpse, and 0x2B (PlrTrade.cpp),
// a trade, to a living player. READ 2026-09-28; the game calls this same
// command function with 0x13 itself (0x140101E83).
constexpr uint32_t LootCorpse = 0x13;
constexpr uint8_t CommandPrefix[] = {0x40, 0x55, 0x53, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0x6C, 0x24, 0xC9};
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{CommandRva, 1509, 0x2EEC5517E47C3C96ULL}, // the command itself
	{0xFC000, 638, 0xF59785F70C614A47ULL},     // what it sends for each command number
	{0xFFAC0, 232, 0x7CB43AF827187F53ULL},     // its refusal while the character cannot act
	{0xFB1D0, 264, 0xEDEF46B84410DA93ULL},     // the local start of the action
	{0xFA180, 1956, 0xED1391B34A9FBF58ULL},    // what an interaction sends, by unit type
	{HaltRva, 367, 0x256CD056ADB87DC3ULL},     // the stop
	{MoveToPointRva, 168, 0x8F1B71BB9A57CE71ULL}, // a click's walk to a point
};
constexpr Evidence SkillChecks[] = {
	{0x34A540, 50, 0x3330034D07C56C5BULL}, // unit -> skills, for the left hand
	{0x33D1A0, 48, 0xF250096169552F56ULL}, // skills -> left skill
	{0x33DB60, 48, 0xE190B1066334A726ULL}, // skills -> right skill
	{0x33E080, 15, 0x912940B532D9CE76ULL}, // skill -> id
};

// Whatever the game returns is handed back untouched: the hook stands between it and its callers.
using CommandFn = uintptr_t(*)(uint32_t, uintptr_t, int32_t, int32_t);
const D2RL::PluginContext* g_context = nullptr;
uintptr_t g_base = 0;
CommandFn g_command = nullptr;  // the game's function, past our own hook when there is one
CommandFn g_original = nullptr; // the hook's way on
bool g_skills = false;
std::atomic<bool> g_logging{true};
std::atomic<uint32_t> g_uiThread{0};
uint32_t g_lastOp = ~0U;
uint64_t g_repeats = 0; // commands of the same number since the last line
uint32_t g_ownOp = ~0U;  // our own last command, and when it was last logged
uint64_t g_ownLog = 0;
constexpr uint64_t OwnLogMs = 1000;

// Our own commands, one line a second for as long as the command stays the
// same: a walk gives its orders several times a second.
bool LogOwn(uint32_t op) {
	if (!g_logging.load()) return false;
	const uint64_t now = GetTickCount64();
	if (op == g_ownOp && now - g_ownLog < OwnLogMs) return false;
	g_ownOp = op;
	g_ownLog = now;
	return true;
}

// Every command the game issues passes here first, ours excepted: those go
// straight to g_command. Observes only; the call goes on unchanged.
uintptr_t Observe(uint32_t op, uintptr_t player, int32_t a, int32_t b) {
	// One line when the command number changes, not one per command: a held
	// stick issues its move many times a second.
	if (g_logging.load()) {
		if (op != g_lastOp) {
			AsyncLog::InfoF(g_context, "[actions] game command 0x%02X a=%d b=%d from RVA %llX thread=%lu ui_thread=%lu after %llu of 0x%02X",
				op, a, b, static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(_ReturnAddress()) - g_base),
				GetCurrentThreadId(), static_cast<unsigned long>(g_uiThread.load()),
				static_cast<unsigned long long>(g_repeats), g_lastOp == ~0U ? 0 : g_lastOp);
			g_lastOp = op;
			g_repeats = 0;
		}
		++g_repeats;
	}
	return g_original(op, player, a, b);
}

bool Call(uint32_t op, int32_t a, int32_t b, const char* what) noexcept {
	if (!g_command) return false;
	g_uiThread = GetCurrentThreadId();
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return false;
	__try {
		g_command(op, player, a, b);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		AsyncLog::ErrorF(g_context, "[actions] %s faulted; commands are off for this session.", what);
		g_command = nullptr;
		return false;
	}
	if (LogOwn(op)) AsyncLog::InfoF(g_context, "[actions] %s: command 0x%02X a=%d b=%d", what, op, a, b);
	return true;
}

int32_t Cell(double value) { return static_cast<int32_t>(std::lround(value)); }
}

bool Initialize(const D2RL::PluginContext* context, bool hooks) {
	g_context = context;
	g_command = g_original = nullptr;
	g_skills = false;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "actions", "keyboard walking and fighting are off")) return false;
	g_base = context->exeBase;
	g_command = reinterpret_cast<CommandFn>(g_base + CommandRva);
	g_skills = LayoutCheck::Verify(context, context->exeBase, SkillChecks, "actions", "the selected skill cannot be read");
	if (hooks && context->InstallInlineHook(CommandRva, CommandPrefix, sizeof(CommandPrefix), &Observe, &g_original) && g_original) {
		g_command = g_original;
		AsyncLog::Info(context, "[actions] Verified; the game's own commands are logged.");
	} else {
		AsyncLog::Info(context, "[actions] Verified; the game's own commands are not logged.");
	}
	return true;
}

bool Available() { return g_command != nullptr; }

bool MoveTo(Radar::Position cell, bool run, bool plain) {
	if (plain) return Call(run ? Run : Walk, Cell(cell.x), Cell(cell.y), run ? "run to (plain)" : "walk to (plain)");
	if (!g_command) return false;
	g_uiThread = GetCurrentThreadId();
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return false;
	const uint32_t op = run ? Run : Walk;
	__try {
		// Its own call to the command passes our hook, which logs it as the game's.
		reinterpret_cast<int(*)(uint32_t, uintptr_t, int32_t, int32_t)>(g_base + MoveToPointRva)(op, player, Cell(cell.x), Cell(cell.y));
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		AsyncLog::Error(g_context, "[actions] move to point faulted; commands are off for this session.");
		g_command = nullptr;
		return false;
	}
	if (LogOwn(op)) AsyncLog::InfoF(g_context, "[actions] %s: %d,%d", run ? "run to" : "walk to", Cell(cell.x), Cell(cell.y));
	return true;
}
bool SkillAt(Hand hand, Radar::Position cell) {
	return Call(hand == Hand::Left ? LeftAt : RightAt, Cell(cell.x), Cell(cell.y), hand == Hand::Left ? "left skill at" : "right skill at");
}
bool SkillOn(Hand hand, Unit type, uint32_t id) {
	return Call(hand == Hand::Left ? LeftOn : RightOn, static_cast<int32_t>(type), static_cast<int32_t>(id),
		hand == Hand::Left ? "left skill on" : "right skill on");
}
bool Interact(Unit type, uint32_t id) {
	uint32_t op = 0;
	switch (type) {
	case Unit::Item:    op = TakeItem; break;
	case Unit::Monster: op = TalkTo; break;
	case Unit::Tile:    op = UseTile; break;
	case Unit::Object:  op = UseObject; break;
	case Unit::Player:  op = LootCorpse; break; // only ever a corpse here: D never offers a living player
	default: return false; // a missile: nothing to open or take
	}
	return Call(op, static_cast<int32_t>(type), static_cast<int32_t>(id), "interact with");
}
bool Halt() {
	if (!g_command) return false;
	uintptr_t player = 0, tables = 0;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player) return false;
	__try {
		reinterpret_cast<void(*)(uintptr_t)>(g_base + HaltRva)(player);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int SelectedSkill(Hand hand) {
	using NativeMemory::Peek;
	if (!g_skills) return -1;
	uintptr_t player = 0, tables = 0, skills = 0, skill = 0, record = 0;
	int16_t id = -1;
	const NativeMemory::ReadScope scope;
	if (!NativeGroundItems::RuntimeRoots(player, tables) || !player
		|| !Peek(player + UnitSkills, skills) || !skills
		|| !Peek(skills + (hand == Hand::Left ? SkillsLeft : SkillsRight), skill) || !skill
		|| !Peek(skill, record) || !record || !Peek(record, id)) return -1;
	return id;
}

void SetLogging(bool enabled) { g_logging = enabled; }
bool Logging() { return g_logging.load(); }
}
