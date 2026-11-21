#include "native_class_choice.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "ui_text.h"

#include <mutex>
#include <string>
#include <string_view>

namespace D2RAccess::NativeClassChoice {
namespace {
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/charcreate.
//
// HDCharacterSelectorWidget keeps its state in the i32 at +0x13FC: 0 not
// chosen, 1 stepping forward, 2 chosen, 3 stepping back (0x156EBB0 writes it,
// 0x156F250 ends 1 and 3 when the animation has). A left click on a figure the
// pointer is over (0x156D110) chooses it in state 0 and puts it back in state
// 2; in states 1 and 3 it does nothing. The panel writes the class's name and
// description to InfoContainer > ClassTitle and ClassDescription (0x15559A0).
constexpr size_t StateOffset = 0x13FC;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x156D110, 4408, 0x4CED529366246564ULL}, // a figure's messages, the click among them
	{0x156EBB0, 664, 0x7B8F72F7085490BFULL},  // the state is set
	{0x156E7B0, 1016, 0xCC955C16B428A18DULL}, // a class is chosen
	{0x156EE50, 1014, 0x489984C5429362CBULL}, // and put back
	{0x156F250, 1943, 0xAF9144F1EA126BD1ULL}, // a step forward or back ends
	{0x15559A0, 517, 0x0D996FDF82CCE4C8ULL},  // the name and the description are written
};
constexpr std::string_view Figure = "HDCharacterSelectorWidget", LegacyFigure = "CharacterCreateWidget";
bool g_verified = false;
// A legacy figure keeps its state where the HD one's offset is not checked: the class
// the game last said chosen (CharacterCreate:Select), by its key, the figure's layout name.
std::mutex g_legacyMutex;
std::string g_legacyChosen;
bool IsLegacy(uintptr_t widget) { return widget && NativeUi::Classify(widget).name == LegacyFigure; }
}

bool Initialize(const D2RL::PluginContext* context) {
	g_verified = context && LayoutCheck::Verify(context, context->exeBase, Checks, "classes", "whether a class is chosen is not said");
	if (g_verified) AsyncLog::Info(context, "[classes] Verified.");
	return g_verified;
}

bool Is(uintptr_t widget) { return widget && NativeUi::Classify(widget).name == Figure; }
bool IsFigure(uintptr_t widget) { return Is(widget) || IsLegacy(widget); }

// A legacy figure is drawn up and to the left of its rectangle's corner, by its
// animation frame's offset; its rectangle's middle is off the figure. A quarter of
// its size up and left of the corner is on all seven (MEASURED 2026-11-21 by
// hovering, docs/legacy-graphics.md#class-figures-do-not-take-the-keyboards-click--2026-11-21):
// the figure is taken as its size round that point, for the pointer and the arrows alike.
std::optional<NativeUi::Rect> LegacyTarget(uintptr_t widget, const NativeUi::Rect& layout) {
	if (!IsLegacy(widget)) return std::nullopt;
	const int x = layout.x - layout.width / 4, y = layout.y - layout.height / 4;
	return NativeUi::Rect{x - layout.width / 2, y - layout.height / 2, layout.width, layout.height};
}

void Note(std::string_view target, std::string_view command, std::string_view text) {
	std::lock_guard lock(g_legacyMutex);
	if (target == "FrontEndNavigation" && command == "ToCharacterCreate") g_legacyChosen.clear();
	else if (target != "CharacterCreate") return;
	else if (command == "Select") g_legacyChosen = text;
	else if (command == "Unselect" && g_legacyChosen == text) g_legacyChosen.clear();
}

State Of(uintptr_t widget) {
	if (IsLegacy(widget)) {
		const std::string name = NativeWidgets::ReadName(widget);
		std::lock_guard lock(g_legacyMutex);
		return !name.empty() && name == g_legacyChosen ? State::Chosen : State::NotChosen;
	}
	int32_t state = -1;
	if (!g_verified || !Is(widget) || !NativeMemory::Peek(widget + StateOffset, state)) return State::Unknown;
	switch (state) {
	case 0:  return State::NotChosen;
	case 1:  return State::Coming;
	case 2:  return State::Chosen;
	case 3:  return State::Going;
	default: return State::Unknown;
	}
}

std::string Description(uintptr_t panel) {
	const uintptr_t info = NativeWidgets::FindChild(panel, "InfoContainer");
	const uintptr_t text = info ? NativeWidgets::FindChild(info, "ClassDescription") : 0;
	return text && NativeUi::Visible(text) ? UiText::Clean(NativeWidgets::ReadText(text)) : std::string{};
}
}
