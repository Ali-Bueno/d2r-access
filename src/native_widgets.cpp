#include "native_widgets.h"
#include "native_memory.h"

#include <windows.h>

#include <cstring>
#include <deque>

namespace D2RAccess::NativeWidgets {

namespace {

// D2RCore.dll 1.2.1-beta: slots the loader fills at startup with the game's
// own pointers (offsets from the DLL base; see native_widgets.h).
constexpr const wchar_t* CoreModuleName        = L"D2RCore.dll";
constexpr uintptr_t      PanelManagerGlobalSlot = 0x5AA340; // -> address of the game's PanelManager* global
constexpr uintptr_t      FindPanelSlot          = 0x5AB880; // Panel*  (PanelManager*, const char* name)
constexpr uintptr_t      FindChildSlot          = 0x5AB610; // Widget* (Widget* parent, const char* name), searches below parent
constexpr uintptr_t      SetTextSlot            = 0x5ADA48; // void (TextBoxWidget*, const {const char*, size_t}*)

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

void RunProbe(const D2RL::PluginContext* context, const PendingProbe& probe) {
	std::string pathText = probe.panel;
	for (const std::string& step : probe.path) {
		pathText += "/" + step;
	}
	const uintptr_t widget = Find(probe.panel, probe.path);
	if (widget == 0) {
		D2RL::LogInfoF(context, "[native] probe %s: not found", pathText.c_str());
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
	D2RL::LogInfoF(context, "[native] probe %s at %p (flags +0x50=%u +0x51=%u):%s", pathText.c_str(), reinterpret_cast<void*>(widget), flags[0], flags[1], report.empty() ? " no strings" : report.c_str());
}

}

bool Initialize(const D2RL::PluginContext* context) {
	if (context == nullptr) {
		return false;
	}
	const HMODULE core = GetModuleHandleW(CoreModuleName);
	const HMODULE exe  = GetModuleHandleW(nullptr);
	if (core == nullptr || exe == nullptr) {
		context->LogWarn("[native] D2RCore.dll not found; native widget access disabled.");
		return false;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
	const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(exe) + dos->e_lfanew);
	g_exeBase       = reinterpret_cast<uintptr_t>(exe);
	g_exeEnd        = g_exeBase + nt->OptionalHeader.SizeOfImage;

	const uintptr_t coreBase = reinterpret_cast<uintptr_t>(core);
	uintptr_t       managerGlobal = 0, findPanel = 0, findChild = 0, setText = 0;
	if (!Peek(coreBase + PanelManagerGlobalSlot, managerGlobal) || !Peek(coreBase + FindPanelSlot, findPanel) || !Peek(coreBase + FindChildSlot, findChild) || !Peek(coreBase + SetTextSlot, setText)) {
		context->LogWarn("[native] D2RCore slots unreadable; native widget access disabled.");
		return false;
	}
	// Every slot must point into the game executable, or this is not the
	// D2RCore build these offsets were taken from.
	if (!InExe(managerGlobal) || !InExe(findPanel) || !InExe(findChild) || !InExe(setText)) {
		D2RL::LogWarnF(context, "[native] D2RCore slots do not point into D2R.exe (manager %p, findPanel %p, findChild %p, setText %p); native widget access disabled.",
			reinterpret_cast<void*>(managerGlobal), reinterpret_cast<void*>(findPanel), reinterpret_cast<void*>(findChild), reinterpret_cast<void*>(setText));
		return false;
	}
	g_managerGlobal = reinterpret_cast<void**>(managerGlobal);
	g_findPanel     = reinterpret_cast<FindPanelFn>(findPanel);
	g_findChild     = reinterpret_cast<FindChildFn>(findChild);
	// These resolved RVAs and the child-vector instructions were recovered from
	// this install's live code (docs/native-ui.md). A different build fails closed
	// for tree/focus traversal while retaining the existing name-based readers.
	constexpr uintptr_t VerifiedFindChildRva = 0x855C90;
	constexpr uintptr_t VerifiedSetTextRva = 0x86DD50;
	constexpr size_t ChildVectorCodeOffset = 0x95;
	constexpr unsigned char ChildVectorCode[] = { 0x48, 0x8B, 0x5F, 0x58, 0x48, 0x8B, 0x47, 0x60, 0x48, 0x8D, 0x2C, 0xC3 };
	unsigned char actual[sizeof(ChildVectorCode)]{};
	g_supportsTree = findChild - g_exeBase == VerifiedFindChildRva && setText - g_exeBase == VerifiedSetTextRva
		&& NativeMemory::Copy(findChild + ChildVectorCodeOffset, actual, sizeof(actual))
		&& std::memcmp(actual, ChildVectorCode, sizeof(actual)) == 0;
	if (!g_supportsTree) context->LogWarn("[text] Unrecognized native layout; in-game text traversal disabled.");
	D2RL::LogInfoF(context, "[native] D2R live RVAs: PanelManager global 0x%llX, FindPanel 0x%llX, FindChildWidget 0x%llX, TextBoxWidget::SetText 0x%llX",
		static_cast<unsigned long long>(managerGlobal - g_exeBase), static_cast<unsigned long long>(findPanel - g_exeBase), static_cast<unsigned long long>(findChild - g_exeBase), static_cast<unsigned long long>(setText - g_exeBase));
	return true;
}

bool IsAvailable() {
	return g_findPanel != nullptr;
}

bool SupportsTree() { return IsAvailable() && g_supportsTree; }

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
