#pragma once

#include <cstdint>
#include <vector>

namespace D2RL {
struct PluginContext;
}

// Keeps the keyboard and the mouse's buttons from the game while the tutorial's
// question is open (docs/tutorial.md): a WH_GETMESSAGE hook on the game window's
// thread turns those messages into WM_NULL and keeps each new press for the
// question. A key pressed meanwhile stays kept to its release; Alt+F4 still closes.
namespace D2RAccess::MessageHold {
// Call on every tick the question is open; it lapses HoldMs after the last call,
// so a question that stops ticking can never leave the keyboard held.
inline constexpr uint64_t HoldMs = 500;
void Hold(const D2RL::PluginContext* context, uint64_t now);
// Lets go at once; a key still down stays kept to its release.
void Release();
// The keys newly pressed while held, in order, as virtual keys; a mouse button
// as its own (VK_LBUTTON...), a turn of the wheel as 0.
std::vector<uint32_t> TakePresses();
void Remove();
}
