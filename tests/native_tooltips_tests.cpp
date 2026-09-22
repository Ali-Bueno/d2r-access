#include "native_tooltips.h"
#include "native_memory.h"
#include "native_ui.h"
#include "test_support.h"

#include <windows.h>
#include <array>
#include <cstring>

namespace {
std::array<unsigned char, 0xB00> panel{};
std::array<unsigned char, 0x5D0> records{};
bool supported = true, installAllowed = true;
int draws = 0, installs = 0;
void (*hook)(uintptr_t) = nullptr;
D2RAccess::NativeUi::Focus focus;
uint64_t selection = UINT64_MAX;

template<class T> void Put(unsigned char* base, size_t offset, T value) { std::memcpy(base + offset, &value, sizeof(value)); }
void StringAt(size_t offset, const std::string& text) {
	Put(records.data(), offset, reinterpret_cast<uintptr_t>(text.data()));
	Put<uint64_t>(records.data(), offset + 8, text.size());
	Put<uint64_t>(records.data(), offset + 16, (uint64_t{1} << 63) | text.size());
}
void Draw(uintptr_t) { ++draws; }
void FaultingDraw(uintptr_t) { ++draws; RaiseException(0xE0421001, 0, 0, nullptr); }
bool DrawFaultPropagates(uintptr_t value) {
	__try {
		D2RAccess::NativeTooltips::DrawPanel(value, FaultingDraw);
	} __except (EXCEPTION_EXECUTE_HANDLER) { return true; }
	return false;
}
bool Install(const D2RL::PluginContext*, const D2RL::InlineHookRegistration* registration) noexcept {
	++installs;
	Check(registration->rva == 0x8DA7E0 && registration->expectedSize == 16, "hook is tied to the verified tooltip renderer and code bytes");
	if (!installAllowed) return false;
	hook = reinterpret_cast<void(*)(uintptr_t)>(registration->target);
	*registration->original = reinterpret_cast<void*>(&Draw);
	return true;
}
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .installInlineHook = Install};
const D2RL::PluginContext context{.contextSize = D2RL::PluginContextSize, .api = &api};
}
namespace D2RAccess::NativeWidgets { bool SupportsTree() { return supported; } }
namespace D2RAccess::AsyncLog {
void InfoF(const D2RL::PluginContext*, const char*, ...) {}
void WarnF(const D2RL::PluginContext*, const char*, ...) {}
}
namespace D2RAccess::NativeUi {
Focus CurrentFocus() { return focus; }
uint64_t FocusSelection(uintptr_t) { return selection; }
Type Classify(uintptr_t widget) {
	switch (widget) {
		case 200: return {.kind = UiText::Kind::InventoryGrid};
		case 201: return {.kind = UiText::Kind::InventorySlot};
		case 202: return {.kind = UiText::Kind::InventoryItem};
		default: return {.kind = UiText::Kind::Other};
	}
}
}

int main() {
	using namespace D2RAccess;
	Check(NativeTooltips::Initialize(&context), "verified hook installs through the SDK");
	Check(installs == 1, "a mismatched panel fingerprint leaves acceleration uninstalled");
	const auto root = reinterpret_cast<uintptr_t>(panel.data());
	const auto first = reinterpret_cast<uintptr_t>(records.data());
	Put(panel.data(), 0x168, first); Put<uint64_t>(panel.data(), 0x170, 2);
	std::string text = "Tu alijo personal", footer;
	StringAt(0, text); StringAt(0x28, footer);
	hook(first);
	Check(draws == 1 && NativeTooltips::Latest(GetTickCount64()).body.empty(), "disabled capture preserves drawing and stays silent");
	NativeTooltips::Track(root); hook(first);
	auto snapshot = NativeTooltips::Latest(GetTickCount64());
	Check(draws == 2 && snapshot.body == text && snapshot.widget == 0, "draw captures the real highlighted world label");
	Check(NativeTooltips::Latest(snapshot.seenAt + 181).body.empty(), "a label no longer rendered expires");
	hook(first + 0x2E8);
	Check(draws == 3 && NativeTooltips::Latest(GetTickCount64()).body == text, "comparison and button-image records never replace the primary tooltip");
	NativeTooltips::Reset(); NativeTooltips::Track(root);
	focus = {100, 200}; selection = (uint64_t{1} << 32) | 3;
	text = "Hacha de mano\nDaño: 3–6\nDurabilidad: 28 de 28";
	footer = "Soltar"; StringAt(0, text); StringAt(0x28, footer); hook(first);
	snapshot = NativeTooltips::Latest(GetTickCount64());
	Check(snapshot.body == text && snapshot.footer == footer, "full native item description and footer are copied without truncation");
	Check(snapshot.panel == 100 && snapshot.widget == 200 && snapshot.selection == selection, "capture records the controller focus and selected cell");
	selection += 1; text = "Amuleto"; StringAt(0, text); hook(first);
	snapshot = NativeTooltips::Latest(GetTickCount64());
	Check(snapshot.body == "Amuleto" && snapshot.selection == selection, "a new cell bypasses capture throttling immediately");
	focus.widget = 201; text = "Casco"; StringAt(0, text); hook(first);
	snapshot = NativeTooltips::Latest(GetTickCount64());
	Check(snapshot.body == "Casco" && snapshot.widget == 201, "equipment focus changes bypass the previous slot's capture throttle");
	NativeTooltips::Reset(); NativeTooltips::Track(root);
	Put<uint64_t>(panel.data(), 0x170, 99999); hook(first);
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "corrupt tooltip vector is rejected");
	Put<uint64_t>(panel.data(), 0x170, 0); hook(first);
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "cleared frame queue is not read");
	Put<uint64_t>(panel.data(), 0x170, 1); Put<uintptr_t>(records.data(), 0, 1); hook(first);
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "unreadable tooltip strings fail safely");
	StringAt(0, text); hook(first); NativeTooltips::Reset();
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "session reset clears captured item data");
	NativeTooltips::Track(1); hook(first);
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "dead tooltip panel cannot fault the render hook");
	NativeTooltips::Reset(); installAllowed = false;
	Check(!NativeTooltips::Initialize(&context), "rejected or mismatched hook is reported unavailable");
	supported = false;
	Check(!NativeTooltips::Initialize(&context), "unsupported native layout never attempts installation");
	supported = true;
	NativeTooltips::Track(root); focus.widget = 200;
	Put<int32_t>(panel.data(), 0xA44, 400);
	auto inspectDelay = +[](uintptr_t value) {
		++draws;
		int32_t delay = -1;
		std::memcpy(&delay, reinterpret_cast<void*>(value + 0xA44), sizeof(delay));
		Check(delay == 0, "inventory tooltip delay is zero only during drawing");
	};
	NativeTooltips::DrawPanel(root, inspectDelay);
	int32_t delay = 0;
	std::memcpy(&delay, panel.data() + 0xA44, sizeof(delay));
	Check(delay == 400, "inventory tooltip delay is restored after drawing");
	draws = 0;
	Check(DrawFaultPropagates(root) && draws == 1, "an original draw fault propagates after one call");
	std::memcpy(&delay, panel.data() + 0xA44, sizeof(delay));
	Check(delay == 400, "inventory tooltip delay is restored while a draw fault unwinds");
	for (const uintptr_t widget : {uintptr_t{201}, uintptr_t{202}}) {
		focus.widget = widget;
		Put<int32_t>(panel.data(), 0xA44, 400);
		NativeTooltips::DrawPanel(root, inspectDelay);
		std::memcpy(&delay, panel.data() + 0xA44, sizeof(delay));
		Check(delay == 400, "every inventory item archetype restores the stock delay");
	}
	Put<int32_t>(panel.data(), 0xA44, 0); NativeTooltips::DrawPanel(root, inspectDelay);
	std::memcpy(&delay, panel.data() + 0xA44, sizeof(delay));
	Check(delay == 0, "an existing zero delay remains zero");
	auto inspectStockDelay = +[](uintptr_t value) {
		++draws;
		int32_t stockDelay = 0;
		std::memcpy(&stockDelay, reinterpret_cast<void*>(value + 0xA44), sizeof(stockDelay));
		Check(stockDelay == 400, "excluded tooltips retain their delay during drawing");
	};
	Put<int32_t>(panel.data(), 0xA44, 400);
	focus.widget = 999; draws = 0; NativeTooltips::DrawPanel(root, inspectStockDelay);
	std::array<unsigned char, 0xB00> otherPanel{};
	Put<int32_t>(otherPanel.data(), 0xA44, 400);
	focus.widget = 200;
	NativeTooltips::DrawPanel(reinterpret_cast<uintptr_t>(otherPanel.data()), inspectStockDelay);
	NativeTooltips::Reset(); NativeTooltips::DrawPanel(root, inspectStockDelay);
	Check(draws == 3, "other widgets, panels, and disabled tracking draw unchanged");
	draws = 0; NativeTooltips::Track(1); focus.widget = 200; NativeTooltips::DrawPanel(1, Draw);
	Check(draws == 1, "an invalid panel cannot prevent its original draw");
	auto* readOnly = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0xB00, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	Check(readOnly != nullptr, "read-only panel fixture allocated");
	Put<int32_t>(readOnly, 0xA44, 400);
	DWORD oldProtect = 0;
	Check(VirtualProtect(readOnly, 0xB00, PAGE_READONLY, &oldProtect) != 0, "panel fixture made read-only");
	draws = 0; NativeTooltips::Track(reinterpret_cast<uintptr_t>(readOnly));
	NativeTooltips::DrawPanel(reinterpret_cast<uintptr_t>(readOnly), Draw);
	Check(draws == 1, "a read-only delay field cannot prevent its original draw");
	VirtualFree(readOnly, 0, MEM_RELEASE);
	Passed();
}
