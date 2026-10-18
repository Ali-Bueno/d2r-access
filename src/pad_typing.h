#pragma once

#include "native_ui.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace D2RL {
struct PluginContext;
}

// Typing into a text box with the controller alone (a tester, 2026-10-15: the
// lobby's host and direct-join boxes took nothing from the pad). A on a box
// that does not start the game's own editing starts it the way the game's A
// does; and while a box is being edited the right stick picks characters: up
// and down step through them, each said as it is reached, A or right types the
// one reached and left deletes the last one. The keystrokes are sent as the
// keyboard's, so the text field reader echoes them. docs/multiplayer.md
namespace D2RAccess::PadTyping {
// The characters offered, in the order the stick steps through them.
inline constexpr std::string_view Charset = "abcdefghijklmnopqrstuvwxyz0123456789 -_.:ABCDEFGHIJKLMNOPQRSTUVWXYZ";
// Pure: the index `delta` steps from `index`, wrapping at either end.
int Step(int index, int delta);

bool Enabled();
void SetEnabled(bool on);

// UI thread, every update the loader's form holds focus: `editing` is the game's
// own text-editing state (NativeSkillNavigation::TextEditing).
void Tick(const D2RL::PluginContext* context, NativeUi::Focus focus, std::optional<bool> editing, uint64_t now);
void Reset();
}
