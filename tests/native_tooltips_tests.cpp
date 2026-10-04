#include "native_tooltips.h"
#include "native_memory.h"
#include "native_ui.h"
#include "test_support.h"

#include <windows.h>
#include <array>
#include <cstring>

namespace {
std::array<unsigned char, 0x180> panel{};
std::array<unsigned char, 0x5D0> records{};
bool supported = true, installAllowed = true;
int draws = 0;
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
bool Install(const D2RL::PluginContext*, const D2RL::InlineHookRegistration* registration) noexcept {
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
namespace D2RAccess::NativeUi {
Focus CurrentFocus() { return focus; }
uint64_t FocusSelection(uintptr_t) { return selection; }
}

int main() {
	using namespace D2RAccess;
	Check(NativeTooltips::Initialize(&context), "verified hook installs through the SDK");
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
	NativeTooltips::Discard(GetTickCount64());
	Check(NativeTooltips::Latest(GetTickCount64()).body.empty(), "moving the keyboard cursor clears the previous capture");
	text = "Runa Tal"; StringAt(0, text); hook(first);
	Check(NativeTooltips::Latest(GetTickCount64()).body == text, "the next draw after cursor movement is captured immediately without a settle window");
	text = "Casco"; StringAt(0, text);
	NativeTooltips::Discard(GetTickCount64());
	std::string equipped = "Casco\nCurrently Equipped";
	StringAt(0x2E8, equipped); focus.widget = 202; hook(first);
	snapshot = NativeTooltips::Latest(GetTickCount64());
	Check(snapshot.body == "Casco" && snapshot.extras == std::vector<std::string>{equipped}, "the records drawn after the primary, a comparison among them, are copied beside it");
	std::string none; StringAt(0x2E8, none);
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
	Passed();
}
