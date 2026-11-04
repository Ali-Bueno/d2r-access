#pragma once

#include <cstdint>
#include <string>

namespace D2RL {
struct PluginContext;
}

// Some panels are action prompts rather than things you navigate. The
// delete-character modal is the clearest case: in controller mode its two
// buttons carry ButtonLegendWidget children whose presets are the global
// "select" and "back" actions (game-data Controller/ConfirmationModalHD.json),
// so the game never gives anything focus and the player is meant to press a
// button, not move to one.
//
// Both readers in this mod are focus-driven, so such a panel is silent: there
// is no focus to report and nothing to match. The answer is NOT to put focus
// there -- moving the game's own cursor is what broke the controller focus
// handling before. It is to read the panel when it opens, from what it
// actually shows: its prompt and its legends.
namespace D2RAccess::ModalReader {

// Reads a panel that has just become active while nothing holds focus. Cheap:
// it does nothing at all unless the active panel changed.
void Tick(const D2RL::PluginContext* context);

void Reset();

// The delete-character modal's count, from five, as the tester saw it (2026-10-22; unmeasured).
inline constexpr int DeleteHoldSeconds = 5;
// The number to say after A has been held `heldMs` on that modal, 0 once the count is over.
int HoldCountdown(uint64_t heldMs);
// The keyboard's menu says when its A is held on Confirm, as a pad's A held counts. UI thread.
void SetKeyHold(bool held);
// The keyboard's menu says when its cursor has the menu: a panel of rows is then read a row at a time.
void SetKeyMenu(bool engaged);

// Text said once, queued behind the title screen's own announcement.
void SetTitleAnnouncement(std::string text);

// The main menu's link bar ("Reign of the Warlock available now!") read as it opens, on by default.
bool MenuNews();
void SetMenuNews(bool on);

}
