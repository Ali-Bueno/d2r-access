#pragma once

#include <D2RLPlugin/api.h>

// A diagnostic that dumps the raw bytes of the focused native widget, so an
// offset can be derived from live evidence instead of guessed.
//
// The SDK exposes no widget state at all (WidgetService offers only rect,
// visible, enabled and dispatch), so the values the reader is missing --
// whether a checkbox is checked, which row a dropdown has selected, the text
// typed into a field -- have to come from the widget object itself. Finding
// where they live means comparing the same widget in two states and looking
// for what changed, which is what this prints.
//
// Dumping happens when the focused widget CHANGES, so the player can simply
// navigate: focus a control, act on it, focus it again, and the log holds both
// states. Bounded to the head of the object and to one dump per focus change,
// so leaving it on does not flood the log.
//
// UI thread only. Every read is a guarded copy; an unreadable row prints as
// "??" rather than failing the dump.
namespace D2RAccess::WidgetProbe {

void SetEnabled(bool enabled) noexcept;
bool Enabled() noexcept;

// Dump the next focus change even while disabled, so a one-shot capture does
// not need the probe left running.
void RequestOnce() noexcept;

// Forget the last dumped widget, so the same control dumps again on the next
// tick. Use after acting on a control without moving focus off it.
void Rearm() noexcept;

void Tick(const D2RL::PluginContext* context);

}
