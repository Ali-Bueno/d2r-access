#pragma once

#include <string>
#include <string_view>

namespace D2RL {
struct PluginContext;
}

// The controls the highlighted thing offers -- "A: Pick Up. B: Close Menu" --
// held for the asking instead of spoken on every move. The user, 2026-09-21:
// "buttons to press should always be last", and "with autodetail off, buttons
// to press should be read by pressing down on the right stick, never
// automatically". Every reader that finds controls offers them here and never
// speaks them itself; right stick down in a menu reads the latest
// (InteractionReader), and with autodetail and control tooltips both on
// (Ctrl+D, Ctrl+H) they are also spoken unasked, queued after everything else.
namespace D2RAccess::ControlHints {

// The controls for what was just described; empty clears them, so a readout
// with none never leaves the last one's standing.
void Offer(std::string_view text);
const std::string& Current();

// Whether controls are spoken unasked: autodetail and control tooltips on.
bool Automatic();

// Queues `text` behind whatever is being said, when Automatic(). Never
// interrupts: the controls always come last.
void SpeakAfter(const D2RL::PluginContext* context, std::string_view text);

void Reset();

}
