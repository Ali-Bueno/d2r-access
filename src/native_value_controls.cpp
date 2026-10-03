#include "native_value_controls.h"

#include "async_log.h"
#include "layout_check.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "ui_text.h"

#include <string_view>

namespace D2RAccess::NativeValueControls {
namespace {
using NativeMemory::Peek;
// D2R 3.3.93787, READ in the image and not yet measured in the game; the
// working files are in the workspace, re/actions/widgets (notes.txt, A and C).
//
// SettingsSliderWidget (constructor 0x883670, init 0x884380): the value is
// the i32 at +0x6C4, between the ones at +0x6BC and +0x6C0. Its two arrows
// are widgets inside the slider's own object, "LeftArrow" at +0x970 and
// "RightArrow" at +0x1500, and the number it shows is the text box
// "ValueText" at +0x2090. None of the three is among its children. A click on
// an arrow sends the slider a message (0x884B60) that steps the value by one
// step through SetValue (0x8DC0C0), which tells the options panel.
// CarouselWidget (init 0x898860): the state shown is the i32 at +0x7EC of the
// count at +0x7E8. "CarouselText", "CarouselLeftArrow" and "CarouselRightArrow"
// are children, and a click on an arrow steps to the next state that is
// allowed (0x8994D0, 0x897E50).
constexpr size_t SliderMinimum = 0x6BC, SliderMaximum = 0x6C0, SliderValue = 0x6C4;
constexpr size_t SliderLeft = 0x970, SliderRight = 0x1500, SliderShown = 0x2090;
constexpr size_t CarouselCount = 0x7E8, CarouselState = 0x7EC;
constexpr int MostStates = 16;
struct Evidence { uintptr_t rva; size_t size; uint64_t hash; };
constexpr Evidence Checks[] = {
	{0x883670, 802, 0x69246CFD33446770ULL},  // the slider is made, with its arrows and its number
	{0x884380, 2007, 0x576A0E44C246CB22ULL}, // and set up
	{0x884B60, 560, 0x9D3F06E21DB3BEAFULL},  // an arrow's click steps it
	{0x884E00, 359, 0xE35FFC1571E04F74ULL},  // the number shown
	{0x8DB840, 1839, 0xECFAFC72DABDA758ULL}, // a click on the track, and the arrow keys
	{0x8DC0C0, 430, 0xB1EF44D8BD083BE0ULL},  // the value is set
	{0x898860, 2941, 0x5AFEC4788BE4378EULL}, // the carousel is set up, with its children
	{0x8994D0, 1536, 0xEB1B3C27CE1CD0E0ULL}, // an arrow's click steps it
	{0x897E50, 446, 0xB48CC4D1009F7CA5ULL},  // to the next state that is allowed
	{0x899B10, 578, 0x0A4E9255AB485B51ULL},  // the state is set
};
constexpr std::string_view Slider = "SettingsSliderWidget", Carousel = "CarouselWidget";
bool g_verified = false;

// A widget inside another's object, by where it is and what it is called.
uintptr_t Inside(uintptr_t owner, size_t offset, std::string_view name) {
	const uintptr_t widget = owner + offset;
	return NativeWidgets::ReadName(widget) == name && NativeUi::Parent(widget) == owner ? widget : 0;
}
}

bool Initialize(const D2RL::PluginContext* context) {
	g_verified = context && LayoutCheck::Verify(context, context->exeBase, Checks, "values", "sliders and options that step are not changed by the arrows");
	if (g_verified) AsyncLog::Info(context, "[values] Verified.");
	return g_verified;
}

Kind Of(uintptr_t widget) {
	if (!g_verified || !widget) return Kind::None;
	const auto name = NativeUi::Classify(widget).name;
	return name == Slider ? Kind::Slider : name == Carousel ? Kind::Carousel : Kind::None;
}

std::optional<int> Value(uintptr_t widget) {
	int32_t value = 0;
	switch (Of(widget)) {
	case Kind::Slider: {
		int32_t least = 0, most = 0;
		if (!Peek(widget + SliderValue, value) || !Peek(widget + SliderMinimum, least) || !Peek(widget + SliderMaximum, most)
			|| least > most || value < least || value > most) return std::nullopt;
		return value;
	}
	case Kind::Carousel: {
		int32_t count = 0;
		if (!Peek(widget + CarouselState, value) || !Peek(widget + CarouselCount, count)
			|| count < 1 || count > MostStates || value < 0 || value >= count) return std::nullopt;
		return value;
	}
	default: return std::nullopt;
	}
}

std::string Shown(uintptr_t widget) {
	switch (Of(widget)) {
	case Kind::Slider: {
		const uintptr_t shown = Inside(widget, SliderShown, "ValueText");
		auto text = shown ? UiText::Clean(NativeWidgets::ReadText(shown)) : std::string{};
		if (const auto value = Value(widget); text.empty() && value) text = std::to_string(*value);
		return text;
	}
	case Kind::Carousel: return UiText::Clean(NativeWidgets::ReadText(NativeWidgets::FindChild(widget, "CarouselText")));
	default: return {};
	}
}

uintptr_t Arrow(uintptr_t widget, bool up) {
	switch (Of(widget)) {
	case Kind::Slider: return up ? Inside(widget, SliderRight, "RightArrow") : Inside(widget, SliderLeft, "LeftArrow");
	case Kind::Carousel: return NativeWidgets::FindChild(widget, up ? "CarouselRightArrow" : "CarouselLeftArrow");
	default: return 0;
	}
}
}
