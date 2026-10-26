#include "native_hit_chance.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include "ui_text.h"

#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace D2RAccess::NativeHitChance {
namespace {
// D2R 3.3.93787, READ in re/D2R.rebuilt.exe (docs/character-panel.md#hit-chance-tooltips-2026-10-25).
// The sheet's hover handler 0x14E8300 builds its text with AttackText(panel, out, player, hand) for each
// skill block's Rating and with DefenseText's pair; the panel argument is never read.
constexpr uintptr_t AttackText = 0x14E7FE0, DefenseChance = 0x15149F0, DefenseText = 0x1514CA0;
constexpr uintptr_t LocalIndex = 0x8B2D0, LocalPlayer = 0x9A480, Allocator = 0x12063C0;
constexpr uint32_t LocalPlayers = 8; // the bound LocalPlayer puts on the index
// The child name the hover handler tests with hand 1 (its second block, hand 0, is named at +0x10).
constexpr uintptr_t HandOneBlock = 0x1FDD080;
constexpr std::string_view LeftBlock = "LeftSkillStats", RightBlock = "RightSkillStats";
// The engine's string as AttackText leaves it: the top bit of `capacity` set while `data` is the
// inline buffer, else the data came from the allocator and goes back through its vtable +0x20 (0x14E852D).
struct EngineString { char* data; uint64_t size; uint64_t capacity; char inlined[16]; };
constexpr uint64_t InlineFlag = 0x8000000000000000ULL;
constexpr size_t FreeSlot = 0x20;
// DefenseText writes at most 0x80 bytes (its snprintf bound); the panel gives it 0x100.
constexpr size_t DefenseBytes = 0x100, MaxTextBytes = 512;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x14E8300, 2149, 0xCC21893231598228ULL},    // the sheet's hover: the builders, hands 1 and 0, the free
	{AttackText, 566, 0xDB7C39187976DAF3ULL},
	{0x1514700, 749, 0xE556344C97C425D1ULL},     // chance to hit, inside AttackText
	{DefenseChance, 687, 0x5265C26DBD44DD92ULL},
	{DefenseText, 343, 0x67BA149E4C0ADB47ULL},
	{LocalIndex, 7, 0x512D0A09BEF62F41ULL},
	{LocalPlayer, 165, 0x754EA91CD47DAFC1ULL},
	{Allocator, 110, 0x5E29724DA34B440AULL},
};
uintptr_t g_base = 0;
bool g_leftIsHandOne = true;

uintptr_t Player() noexcept {
	__try {
		const auto index = reinterpret_cast<uint32_t (*)()>(g_base + LocalIndex)();
		return index < LocalPlayers ? reinterpret_cast<uintptr_t (*)(uint32_t)>(g_base + LocalPlayer)(index) : 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

void Release(char* data) noexcept {
	__try {
		const auto allocator = reinterpret_cast<uintptr_t (*)()>(g_base + Allocator)();
		uintptr_t vtable = 0, release = 0;
		if (!allocator || !NativeMemory::Peek(allocator, vtable) || !NativeMemory::Peek(vtable + FreeSlot, release) || !release) return;
		reinterpret_cast<void (*)(uintptr_t, char*)>(release)(allocator, data);
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Copies the attack text into `text` (NUL-terminated); false when the call failed.
bool BuildAttack(uintptr_t player, uint8_t hand, char* text, size_t capacity) noexcept {
	EngineString built{};
	__try {
		reinterpret_cast<EngineString* (*)(uintptr_t, EngineString*, uintptr_t, uint8_t)>(g_base + AttackText)(0, &built, player, hand);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
	const size_t size = built.data ? std::min<size_t>(static_cast<size_t>(built.size), capacity - 1) : 0;
	const bool copied = !size || NativeMemory::Copy(reinterpret_cast<uintptr_t>(built.data), text, size);
	text[copied ? size : 0] = '\0';
	if (built.data && !(built.capacity & InlineFlag)) Release(built.data);
	return copied;
}

bool BuildDefense(char* text) noexcept {
	__try {
		const int chance = reinterpret_cast<int (*)()>(g_base + DefenseChance)();
		reinterpret_cast<void (*)(int, char*)>(g_base + DefenseText)(chance, text);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// The builders' lines, as one: the game's own break between them.
std::string Spoken(const char* text) {
	std::string joined(text);
	std::replace(joined.begin(), joined.end(), '\n', ' ');
	return UiText::Clean(joined);
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "hit-chance", "the character sheet's hit chances are not read"))
		return false;
	g_base = context->exeBase;
	// Which block is hand 1, read from the name the game itself tests; the left one unless it says otherwise.
	char name[32]{};
	const bool read = NativeMemory::Copy(g_base + HandOneBlock, name, sizeof name - 1);
	const std::string_view block = read ? std::string_view(name) : std::string_view{};
	g_leftIsHandOne = block != RightBlock;
	AsyncLog::InfoF(context, "[hit-chance] Verified; hand 1 is %s (\"%.15s\").", g_leftIsHandOne ? "left" : "right", name);
	if (block != LeftBlock && block != RightBlock) AsyncLog::Warn(context, "[hit-chance] hand 1's block name unread; left assumed.");
	return true;
}

std::string Attack(bool left) {
	const auto player = g_base ? Player() : 0;
	char text[MaxTextBytes]{};
	if (!player || !BuildAttack(player, left == g_leftIsHandOne ? 1 : 0, text, sizeof text)) return {};
	return Spoken(text);
}

std::string Defense() {
	char text[DefenseBytes]{};
	if (!g_base || !Player() || !BuildDefense(text)) return {};
	return Spoken(text);
}
}
