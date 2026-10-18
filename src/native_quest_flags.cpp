#include "native_quest_flags.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"

#include <array>
#include <atomic>

namespace D2RAccess::NativeQuestFlags {
namespace {
// D2R 3.3.93787, read in the image, not yet measured live: a pointer to the client's
// quest record, whose first field points to 0x60 bytes of flags, bit quest*16+bit
// tested through a table of byte masks. Evidence: docs/quests.md.
constexpr uintptr_t RecordRva = 0x2A48778, MaskTableRva = 0x1D996D0;
constexpr int FlagWords = 0x60 / 2;
constexpr int MaskCount = 8;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x10F180, 8, 0x6BB58B35E90678D5ULL},  // the record's accessor: mov rax, [rip -> RecordRva]
	{0x325C50, 86, 0x2681A37A0942F67AULL}, // getter(record, quest, bit): flag bit quest*16+bit
	{0xA1BA50, 30, 0x2DB1A7098E3255A6ULL}, // the bit test: byte n/8, mask table[n%8]
};
const D2RL::PluginContext* g_context = nullptr;
std::atomic<uintptr_t> g_base{0};
// The mask table is filled at run time: 1 once it reads 1, 2, 4... (a word's own bit
// order), -1 when it reads anything else, 0 while it is still empty.
std::atomic<int> g_order{0};
std::array<std::atomic<int>, FlagWords> g_logged{}; // the last word logged per row, plus one; 0 for none

bool LowBitFirst() {
	if (const int order = g_order.load(); order) return order > 0;
	uint32_t masks[MaskCount]{};
	if (!NativeMemory::Copy(g_base.load() + MaskTableRva, masks, sizeof masks) || !masks[0]) return false;
	bool ordered = true;
	for (int i = 0; i < MaskCount; ++i) ordered &= masks[i] == 1u << i;
	g_order = ordered ? 1 : -1;
	if (ordered) AsyncLog::Info(g_context, "[quest] Flag bit order verified.");
	else AsyncLog::WarnF(g_context, "[quest] Flag masks read %X %X %X...: quest flags not read.", masks[0], masks[1], masks[2]);
	return ordered;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_base = 0;
	g_context = context;
	if (!context || !LayoutCheck::Verify(context, context->exeBase, Checks, "quest", "quest flags not read")) return false;
	g_base = context->exeBase;
	AsyncLog::Info(context, "[quest] Verified quest flag reader.");
	return true;
}

std::optional<uint16_t> Read(int quest) {
	if (!g_base.load() || quest < 0 || quest >= FlagWords) return std::nullopt;
	const NativeMemory::ReadScope scope;
	uintptr_t record = 0, flags = 0;
	uint16_t word = 0;
	if (!LowBitFirst() || !NativeMemory::Peek(g_base.load() + RecordRva, record) || !record
		|| !NativeMemory::Peek(record, flags) || !flags || !NativeMemory::Peek(flags + static_cast<uintptr_t>(quest) * 2, word))
		return std::nullopt;
	if (g_logged[static_cast<size_t>(quest)].exchange(word + 1) != word + 1)
		AsyncLog::InfoF(g_context, "[quest] flags row=%d word=%04X", quest, word);
	return word;
}
}
