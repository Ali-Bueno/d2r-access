#pragma once

#include "generated/panel_widgets.h"

#include <optional>
#include <string>
#include <string_view>

// Tracks the value of toggles, carousels and dropdowns, which the game exposes
// neither through the SDK nor in its update messages. The baseline comes from
// the game's own Settings.json (written whenever the player applies options);
// afterwards each update message flips a toggle or steps a carousel in the
// direction of the player's last horizontal input, and a dropdown takes the
// index picked in its popup list.
namespace D2RAccess::ControlState {

// Current state index (0/1 for toggles, position in `states` otherwise), or
// nothing when unknown.
std::optional<int> Index(const Generated::WidgetEntry& widget, std::string_view widgetId);

// Spoken state: "checked"/"unchecked" for toggles, the localized state name
// for carousels and dropdowns; empty when unknown.
std::string Describe(const Generated::WidgetEntry& widget, std::string_view widgetId);

// The control reported a change through its update message. `before` is the
// index captured when the message arrived: the game may rewrite Settings.json
// (and the cache be invalidated) before the message is processed, so stepping
// from a re-read baseline would count the change twice.
void OnUpdateMessage(const Generated::WidgetEntry& widget, std::string_view widgetId, std::optional<int> before);

// A popup list item was chosen for a dropdown.
void SetIndex(std::string_view widgetId, int index);

// Drops cached values whose baseline may have changed (settings applied or
// discarded); the next query re-reads Settings.json.
void Invalidate();

}
