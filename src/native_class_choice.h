#pragma once

#include "native_ui.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <optional>
#include <string>

// The classes of the character creation screen. Each is a figure that takes a
// click anywhere in its rectangle: a click on one that is not chosen chooses
// it, in place of any other, and a click on the one that is chosen puts it
// back. While a figure steps forward or back a click on it is lost. The name
// and the description of the class chosen are two text boxes of the panel.
// A legacy graphics figure's state is the game's own Select and Unselect messages.
// docs/keyboard-play.md
namespace D2RAccess::NativeClassChoice {
enum class State { Unknown, NotChosen, Coming, Chosen, Going };

bool Initialize(const D2RL::PluginContext* context);
// A class's figure; its name in the layout is the class's key in the string table.
bool Is(uintptr_t widget);
// A figure of either graphics: legacy graphics' figures have no state read (Of says Unknown).
bool IsFigure(uintptr_t widget);
// Where a legacy figure is drawn, from its layout rectangle: its middle takes the pointer; nullopt for any other widget.
std::optional<NativeUi::Rect> LegacyTarget(uintptr_t widget, const NativeUi::Rect& layout);
// The game's UI messages, from which a legacy figure's state is known: CharacterCreate
// Select and Unselect with the class key, and the screen being entered afresh.
void Note(std::string_view target, std::string_view command, std::string_view text);
State Of(uintptr_t widget);
// The description the panel shows of the class chosen, or of the one the
// pointer is over when none is; "" when it shows none.
std::string Description(uintptr_t panel);
}
