#include "async_log.h"
#include "native_widgets.h"
#include "core_slots.h"
#include "native_memory.h"
#include "layout_check.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

namespace D2RAccess::NativeWidgets {

namespace {

// The game pointers this reader needs, named by the RVA their D2RCore
// descriptor carries (core_slots.h).
constexpr uint64_t PanelManagerGlobalRva = 0x3440170; // the game's PanelManager* global
constexpr uint64_t FindPanelRva          = 0x89F760;  // Panel*  (PanelManager*, const char* name)
constexpr uint64_t FindChildRva          = 0x855C90;  // Widget* (Widget* parent, const char* name), searches below parent
constexpr uint64_t SetTextRva            = 0x86DD50;  // void (TextBoxWidget*, const {const char*, size_t}*)

// Engine string type (0x28 bytes).
constexpr size_t   EngineStringSize   = 0x28;
// Measured with the probe (2026-09-15, D2R 3.3.93787): every widget keeps
// its name at +0x8 (CharacterTemplate, Name, TextBox, Prompt) and a text box
// keeps the text it displays at +0x88 ("yarkey", "Bárbaro de niv. 1",
// "1280x720", the modal prompt), UTF-8.
constexpr size_t NameOffset    = 0x8;
constexpr size_t TextBoxTextOffset = 0x88;
// Reflection offsets seen for widget members reach 0xAD0, so a scan spans this much.
constexpr size_t   WidgetScanSpan     = 0xB80;
constexpr size_t   ScanStep           = 8; // strings are pointer-aligned members

using FindPanelFn = void* (*)(void* manager, const char* name);
using FindChildFn = void* (*)(void* parent, const char* name);

uintptr_t   g_exeBase    = 0;
uintptr_t   g_exeEnd     = 0;
void**      g_managerGlobal = nullptr; // the game's PanelManager* global
FindPanelFn g_findPanel  = nullptr;
FindChildFn g_findChild  = nullptr;
bool g_supportsTree = false;
bool g_supportsNotifications = false;
bool g_supportsTombSymbols = false;
constexpr uintptr_t NotificationCountRva = 0x84CC70;
constexpr uintptr_t NotificationEntryRva = 0x84D0B0;
constexpr size_t NotificationCountSlot = 0xC8;
constexpr uint64_t NotificationCapacity = 19; // one unused ring slot; 0x850060
// An entry without text draws as an empty row (a tester's "black square", 2026-10-23): its
// start and its string's first bytes are logged once, to tell what the game pushed.
constexpr size_t UnreadEntryBytes = 0x40;
constexpr size_t UnreadTextBytes  = 0x20;
uintptr_t g_unreadEntry = 0;
const D2RL::PluginContext* g_context = nullptr;

struct PendingProbe {
	std::string              panel;
	std::vector<std::string> path;
	unsigned long long       dueTick = 0;
};
std::deque<PendingProbe> g_probes;

using NativeMemory::Peek;
using NativeMemory::Readable;
using NativeMemory::ReadEngineString;
using NativeMemory::LooksLikeText;

bool InExe(uintptr_t address) {
	return address >= g_exeBase && address < g_exeEnd;
}

// The native calls run under structured exception handling so a wrong
// signature or a dead object cannot take the game down; these helpers hold
// no C++ objects, as the compiler requires for __try.
uintptr_t CallFindPanel(void* manager, const char* name) noexcept {
	__try {
		return reinterpret_cast<uintptr_t>(g_findPanel(manager, name));
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

uintptr_t CallFindChild(void* parent, const char* name) noexcept {
	__try {
		return reinterpret_cast<uintptr_t>(g_findChild(parent, name));
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return 0;
	}
}

uintptr_t CallLatestNotification(uintptr_t list, uintptr_t countFunction, uintptr_t entryFunction) noexcept {
	__try {
		const auto count = reinterpret_cast<uint64_t(*)(uintptr_t)>(countFunction)(list);
		return count > 0 && count < NotificationCapacity
			? reinterpret_cast<uintptr_t(*)(uintptr_t, int)>(entryFunction)(list, static_cast<int>(count - 1)) : 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

std::string Hex(const unsigned char* bytes, size_t size) {
	std::string text;
	char pair[4];
	for (size_t i = 0; i < size; ++i) {
		std::snprintf(pair, sizeof(pair), i % 8 == 7 && i + 1 < size ? "%02X " : "%02X", bytes[i]);
		text += pair;
	}
	return text;
}

void LogUnreadEntry(uintptr_t entry) {
	if (entry == 0 || entry == g_unreadEntry) return;
	g_unreadEntry = entry;
	unsigned char head[UnreadEntryBytes]{}, body[UnreadTextBytes]{};
	if (!NativeMemory::Copy(entry, head, sizeof(head))) return;
	uint64_t data = 0, size = 0;
	std::memcpy(&data, head, sizeof(data));
	std::memcpy(&size, head + sizeof(data), sizeof(size));
	const size_t shown = static_cast<size_t>(size < sizeof(body) ? size : sizeof(body));
	const bool read = shown != 0 && NativeMemory::Copy(static_cast<uintptr_t>(data), body, shown);
	D2RAccess::AsyncLog::InfoF(g_context, "[notice] row without text: entry %p, string size %llu, text [%s], entry [%s]",
		reinterpret_cast<void*>(entry), static_cast<unsigned long long>(size), read ? Hex(body, shown).c_str() : "",
		Hex(head, sizeof(head)).c_str());
}

void RunProbe(const D2RL::PluginContext* context, const PendingProbe& probe) {
	std::string pathText = probe.panel;
	for (const std::string& step : probe.path) {
		pathText += "/" + step;
	}
	const uintptr_t widget = Find(probe.panel, probe.path);
	if (widget == 0) {
		D2RAccess::AsyncLog::InfoF(context, "[native] probe %s: not found", pathText.c_str());
		return;
	}
	std::string report;
	for (const FoundString& found : ScanStrings(widget, WidgetScanSpan)) {
		char item[240] {};
		std::snprintf(item, sizeof(item), " +0x%zX \"%.200s\"", found.offset, found.value.c_str());
		report += item;
	}
	uint8_t flags[2] {};
	(void)Peek(widget + 0x50, flags);
	D2RAccess::AsyncLog::InfoF(context, "[native] probe %s at %p (flags +0x50=%u +0x51=%u):%s", pathText.c_str(), reinterpret_cast<void*>(widget), flags[0], flags[1], report.empty() ? " no strings" : report.c_str());
}

}

bool Initialize(const D2RL::PluginContext* context) {
	if (context == nullptr) {
		return false;
	}
	g_context = context;
	const HMODULE core = GetModuleHandleW(CoreSlots::ModuleName);
	const HMODULE exe  = GetModuleHandleW(nullptr);
	if (core == nullptr || exe == nullptr) {
		D2RAccess::AsyncLog::Warn(context, "[native] D2RCore.dll not found; native widget access disabled.");
		return false;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
	const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(exe) + dos->e_lfanew);
	g_exeBase       = reinterpret_cast<uintptr_t>(exe);
	g_exeEnd        = g_exeBase + nt->OptionalHeader.SizeOfImage;

	const char*     build    = CoreSlots::LoaderVersion();
	const uintptr_t coreBase = reinterpret_cast<uintptr_t>(core);
	CoreSlots::Anchor anchors[]{{PanelManagerGlobalRva}, {FindPanelRva}, {FindChildRva}, {SetTextRva}};
	CoreSlots::Resolve(coreBase, anchors);
	if (!CoreSlots::Resolved(anchors)) {
		D2RAccess::AsyncLog::WarnF(context, "[native] D2RCore (loader %s) carries no single descriptor for the game pointers (matches: manager %u, findPanel %u, findChild %u, setText %u); native widget access disabled.",
			build != nullptr ? build : "unknown", anchors[0].hits, anchors[1].hits, anchors[2].hits, anchors[3].hits);
		return false;
	}
	uintptr_t managerGlobal = 0, findPanel = 0, findChild = 0, setText = 0;
	if (!Peek(coreBase + anchors[0].slot, managerGlobal) || !Peek(coreBase + anchors[1].slot, findPanel) || !Peek(coreBase + anchors[2].slot, findChild) || !Peek(coreBase + anchors[3].slot, setText)) {
		D2RAccess::AsyncLog::Warn(context, "[native] D2RCore slots unreadable; native widget access disabled.");
		return false;
	}
	// Every slot must point into the game executable. A zero means the loader
	// had not resolved that descriptor yet; anything else means this D2RCore
	// resolves these RVAs to something other than the game's own code.
	if (!InExe(managerGlobal) || !InExe(findPanel) || !InExe(findChild) || !InExe(setText)) {
		D2RAccess::AsyncLog::WarnF(context, "[native] D2RCore slots (loader %s) do not point into D2R.exe (manager %p, findPanel %p, findChild %p, setText %p); native widget access disabled.",
			build != nullptr ? build : "unknown",
			reinterpret_cast<void*>(managerGlobal), reinterpret_cast<void*>(findPanel), reinterpret_cast<void*>(findChild), reinterpret_cast<void*>(setText));
		return false;
	}
	D2RAccess::AsyncLog::InfoF(context, "[native] D2RCore descriptors (loader %s): manager 0x%llX, findPanel 0x%llX, findChild 0x%llX, setText 0x%llX",
		build != nullptr ? build : "unknown",
		static_cast<unsigned long long>(anchors[0].slot), static_cast<unsigned long long>(anchors[1].slot),
		static_cast<unsigned long long>(anchors[2].slot), static_cast<unsigned long long>(anchors[3].slot));
	g_managerGlobal = reinterpret_cast<void**>(managerGlobal);
	g_findPanel     = reinterpret_cast<FindPanelFn>(findPanel);
	g_findChild     = reinterpret_cast<FindChildFn>(findChild);
	// The child-vector instructions were recovered from this install's live code
	// (docs/native-ui.md). Requiring the loader to have resolved each descriptor
	// to the game's own code, undetoured, keeps tree/focus traversal failing
	// closed on a different build while the name-based readers carry on.
	constexpr size_t ChildVectorCodeOffset = 0x95;
	constexpr unsigned char ChildVectorCode[] = { 0x48, 0x8B, 0x5F, 0x58, 0x48, 0x8B, 0x47, 0x60, 0x48, 0x8D, 0x2C, 0xC3 };
	unsigned char actual[sizeof(ChildVectorCode)]{};
	g_supportsTree = findChild - g_exeBase == FindChildRva && setText - g_exeBase == SetTextRva
		&& NativeMemory::Copy(findChild + ChildVectorCodeOffset, actual, sizeof(actual))
		&& std::memcmp(actual, ChildVectorCode, sizeof(actual)) == 0;
	if (!g_supportsTree) D2RAccess::AsyncLog::Warn(context, "[text] Unrecognized native layout; in-game text traversal disabled.");
	D2RAccess::AsyncLog::InfoF(context, "[native] D2R live RVAs: PanelManager global 0x%llX, FindPanel 0x%llX, FindChildWidget 0x%llX, TextBoxWidget::SetText 0x%llX",
		static_cast<unsigned long long>(managerGlobal - g_exeBase), static_cast<unsigned long long>(findPanel - g_exeBase), static_cast<unsigned long long>(findChild - g_exeBase), static_cast<unsigned long long>(setText - g_exeBase));
	constexpr unsigned char EntryCode[] = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57 };
	unsigned char entryCode[sizeof(EntryCode)]{};
	g_supportsNotifications = g_supportsTree
		&& NativeMemory::Copy(g_exeBase + NotificationEntryRva, entryCode, sizeof(entryCode))
		&& std::memcmp(entryCode, EntryCode, sizeof(entryCode)) == 0;
	if (g_supportsTree && !g_supportsNotifications) D2RAccess::AsyncLog::Warn(context, "[notice] Unrecognized message accessor; repeated-delivery detection disabled.");
	// ImageWidget::SetFrame and the journal population routine prove +0x90
	// and the visible TalRashaSymbol semantics. See docs/tal-rasha-symbols.md.
	struct SymbolCheck { uintptr_t rva; size_t size; uint64_t hash; };
	constexpr SymbolCheck symbolChecks[] {
		{0x859700, 7, 0xEAB4DB47D06D0BC4ULL},
		{0x14DE610, 1572, 0x10E3E6169FE18826ULL},
	};
	g_supportsTombSymbols = g_supportsTree && LayoutCheck::Verify(context, g_exeBase, symbolChecks,
		"tomb", "journal symbol reading disabled");
	if (g_supportsTombSymbols) AsyncLog::Info(context, "[tomb] Verified visible journal symbol reader.");
	return true;
}

bool IsAvailable() {
	return g_findPanel != nullptr;
}

bool SupportsTree() { return IsAvailable() && g_supportsTree; }
bool SupportsTombSymbols() { return SupportsTree() && g_supportsTombSymbols; }

Notification ReadLatestNotification(uintptr_t list) {
	uintptr_t vtable = 0, count = 0;
	if (!g_supportsNotifications || !Peek(list, vtable) || !Peek(vtable + NotificationCountSlot, count)
		|| count != g_exeBase + NotificationCountRva) return {};
	const uintptr_t entry = CallLatestNotification(list, count, g_exeBase + NotificationEntryRva);
	std::string text;
	// The producer copies the localized message into the entry's first string.
	if (!entry || !ReadEngineString(entry, text) || !LooksLikeText(text)) { LogUnreadEntry(entry); return {}; }
	return {entry, std::move(text)};
}

uintptr_t PanelManager() {
	uintptr_t manager = 0;
	return IsAvailable() && Peek(reinterpret_cast<uintptr_t>(g_managerGlobal), manager) ? manager : 0;
}

uintptr_t FindPanel(std::string_view panel) {
	void* manager = nullptr;
	if (!IsAvailable() || !Peek(reinterpret_cast<uintptr_t>(g_managerGlobal), manager) || manager == nullptr) {
		return 0;
	}
	const std::string name(panel);
	return CallFindPanel(manager, name.c_str());
}

uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	if (!IsAvailable() || parent == 0) {
		return 0;
	}
	const std::string owned(name);
	return CallFindChild(reinterpret_cast<void*>(parent), owned.c_str());
}

uintptr_t Find(std::string_view panel, std::span<const std::string> path) {
	uintptr_t current = FindPanel(panel);
	for (const std::string& step : path) {
		if (current == 0) {
			break;
		}
		current = FindChild(current, step);
	}
	return current;
}

std::vector<FoundString> ScanStrings(uintptr_t widget, size_t span) {
	std::vector<FoundString> found;
	if (widget == 0 || (widget & 7) != 0 || !Readable(reinterpret_cast<const void*>(widget), ScanStep)) {
		return found;
	}
	for (size_t offset = ScanStep; offset + EngineStringSize <= span; offset += ScanStep) { // +0 is the vtable
		std::string value;
		if (ReadEngineString(widget + offset, value) && LooksLikeText(value)) {
			found.push_back({ offset, std::move(value) });
		}
	}
	return found;
}

std::string ReadName(uintptr_t widget) {
	std::string text;
	return widget != 0 && ReadEngineString(widget + NameOffset, text) ? text : std::string {};
}

std::string ReadText(uintptr_t widget) {
	std::string text;
	return widget != 0 && ReadEngineString(widget + TextBoxTextOffset, text) && LooksLikeText(text) ? text : std::string {};
}

std::string TextAt(std::string_view panel, std::span<const std::string> path) {
	return ReadText(Find(panel, path));
}

bool HasGameVtable(uintptr_t object) {
	uintptr_t vtable = 0;
	return object != 0 && (object & 7) == 0 && Peek(object, vtable) && InExe(vtable);
}

void LogStringsDeep(const D2RL::PluginContext* context, uintptr_t widget, std::string_view label) {
	constexpr size_t BlockSpan = 0x400; // the start of a block a member points at
	constexpr int    MaxBlocks = 24;    // log lines, once
	const std::string name(label);
	std::string own;
	for (const FoundString& found : ScanStrings(widget, WidgetScanSpan)) {
		char item[120] {};
		std::snprintf(item, sizeof(item), " +0x%zX \"%.60s\"", found.offset, found.value.c_str());
		own += item;
	}
	D2RAccess::AsyncLog::InfoF(context, "[native] deep probe %s at %p:%s", name.c_str(), reinterpret_cast<void*>(widget), own.empty() ? " no strings of its own" : own.c_str());
	int blocks = 0;
	for (size_t offset = ScanStep; offset + sizeof(uintptr_t) <= WidgetScanSpan && blocks < MaxBlocks; offset += ScanStep) {
		uintptr_t target = 0;
		if (!Peek(widget + offset, target) || target == 0 || (target & 7) != 0 || target == widget
			|| (target >= g_exeBase && target < g_exeEnd)) continue;
		const auto strings = ScanStrings(target, BlockSpan);
		if (strings.empty()) continue;
		++blocks;
		std::string report;
		for (size_t i = 0; i < strings.size() && i < 4; ++i) {
			char item[120] {};
			std::snprintf(item, sizeof(item), " +0x%zX \"%.60s\"", strings[i].offset, strings[i].value.c_str());
			report += item;
		}
		D2RAccess::AsyncLog::InfoF(context, "[native] deep probe %s +0x%zX -> %p (%zu strings):%s", name.c_str(), offset,
			reinterpret_cast<void*>(target), strings.size(), report.c_str());
	}
}

void RequestProbe(std::string_view panel, std::span<const std::string> path, unsigned delayMs) {
	if (!IsAvailable()) {
		return;
	}
	g_probes.push_back({ std::string(panel), std::vector<std::string>(path.begin(), path.end()), GetTickCount64() + delayMs });
}

void Tick(const D2RL::PluginContext* context) {
	const unsigned long long now = GetTickCount64();
	while (!g_probes.empty() && g_probes.front().dueTick <= now) {
		const PendingProbe probe = std::move(g_probes.front());
		g_probes.pop_front();
		RunProbe(context, probe);
	}
}

}
