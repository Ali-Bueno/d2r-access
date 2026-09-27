#pragma once

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>
#include <string>

// The controls of the options that hold a value and step through it: a slider
// (SettingsSliderWidget) and an option that goes through a list of states
// (CarouselWidget). Each has an arrow at either end, and a click on an arrow
// steps the value once, as the controller's left and right do. A click on a
// slider's track sets the value to where the click was, so the track is never
// clicked. docs/keyboard-play.md
namespace D2RAccess::NativeValueControls {
enum class Kind { None, Slider, Carousel };

bool Initialize(const D2RL::PluginContext* context);

Kind Of(uintptr_t widget);
// A slider's value, or the place of a carousel's state among its states.
std::optional<int> Value(uintptr_t widget);
// The value as the game shows it: a slider's number, a carousel's state.
std::string Shown(uintptr_t widget);
// The arrow that steps the value down or up, 0 when it is not there.
uintptr_t Arrow(uintptr_t widget, bool up);
}
