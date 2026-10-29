#pragma once

#include "native_ui.h"

#include <array>
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
// one reached and left deletes the last one (or it walks a QWERTY keyboard,
// below). The keystrokes are sent as the
// keyboard's, so the text field reader echoes them. docs/multiplayer.md
namespace D2RAccess::PadTyping {
// The characters offered, in the order the stick steps up through them from "a", where it
// starts: down from there goes 1 to 0 at once, for number passwords (a tester, 2026-10-23).
inline constexpr std::string_view Charset = "abcdefghijklmnopqrstuvwxyz -_.:ABCDEFGHIJKLMNOPQRSTUVWXYZ0987654321";
// Pure: the index `delta` steps from `index`, wrapping at either end.
int Step(int index, int delta);

// The keyboard the stick walks instead with `pad-typing-qwerty` (the user, 2026-10-28): up and
// down change row, left and right key, A types. Caps Lock and Backspace are keys of the last row.
inline constexpr char CapsLockKey = '\x14', BackspaceKey = '\b';
inline constexpr std::array<std::string_view, 5> QwertyRows{
	"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", "\x14 -_.:\b"};
struct Key { int row = 1, column = 0; }; // starts on "q"
// Pure: the key `dx` keys and `dy` rows from `key`, wrapping; a shorter row takes its last key.
Key Move(Key key, int dx, int dy);
// Pure: the character at `key`, a letter in upper case with Caps Lock on.
char KeyAt(Key key, bool capsLock);

bool Enabled();
void SetEnabled(bool on);
bool Qwerty();
void SetQwerty(bool on);

// UI thread, every update the loader's form holds focus: `editing` is the game's
// own text-editing state (NativeSkillNavigation::TextEditing).
void Tick(const D2RL::PluginContext* context, NativeUi::Focus focus, std::optional<bool> editing, uint64_t now);
void Reset();
}
