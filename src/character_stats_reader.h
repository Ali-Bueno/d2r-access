#pragma once

#include "input_direction.h"
#include "native_ui.h"

#include <D2RLPlugin/api.h>

// Character and hireling panel statistics, on the right stick. The hireling
// reader is active during its observed no-focus controller state and yields
// whenever the game reports a real focused widget. The character panel shows
// sixteen rows at once and the controller only ever stands on one of them, so
// reading it row by row means walking the focus across the whole screen. The
// stick walks the rows instead, without moving the game's own focus:
//
//   up    previous statistic     left   first statistic
//   down  next statistic         right  last statistic
//
// The stick is shared with the item/skill detail readout and the level
// scanner, so all four directions are claimed only while the character panel
// holds focus -- which is also the one panel where neither has anything to say.
namespace D2RAccess::CharacterStatsReader {

// Where a push moves a cursor over `count` lines. The cursor starts at -1,
// meaning nothing has been read yet; it never leaves the list, so a push past
// either end re-reads the line already there. Returns -1 when there is
// nothing to read.
int Advance(int cursor, InputDirection::StickPush push, int count);

// True when the gesture was claimed, so the callers further down the tick
// leave the stick alone. Also says the character's name once per opening of
// the panel, queued behind the panel's title. While a point button has focus
// the stick is the point readout's -- up for the stat's explanation, down for
// its controls -- except right, which reads the points left to spend; with
// nothing focused, right jumps to the last statistic, which is that same line.
bool Tick(const D2RL::PluginContext* context, NativeUi::Focus focus);

// True for the panel this reader speaks for, so the generic readers leave it.
bool Owns(std::string_view panel);

void Reset();

}
