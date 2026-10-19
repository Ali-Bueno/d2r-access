#pragma once

#include <D2RLPlugin/api.h>

#include <string_view>

// Controller focus reader built only on the SDK: the game's FocusManager panel
// owns a "FocusIndicator" widget (the selection border, see
// Controller/FocusManagerHD.json). Its screen rect is polled every UI update;
// when it moves, the focusable widget of an open panel whose rect it covers is
// the focused one, and its label is spoken.
namespace D2RAccess::FocusTracker {

// One poll; must run on the UI thread (driven by UiThreadPump).
void Tick(const D2RL::PluginContext* context);

// Speaks "<label>: <value>" for a UI message that is some catalog widget's
// update message (its layout onUpdateMessage / onClickMessage). Returns false
// when no open panel owns such a widget. Any thread.
bool AnnounceValue(const D2RL::PluginContext* context, std::string_view target, std::string_view command, std::string_view value);

// Widget rects are cached while the UI stays still; call when panels open,
// close or scroll so the next poll re-resolves them.
// Cached widget rects are stale: a list scrolled. Rows moved under a still
// indicator, so the next tick re-checks even if it did not move.
void InvalidateRects();

// The panel set changed: rects and widget handles are stale.
void PanelsChanged();

// The game reported a scroll input (ScrollController:ManualScrollInput):
// the press count, not the sampled positions, drives popup scroll tracking.
void NoteScrollInput();

// Whether the character-select and character-create hints are spoken.
bool ScreenHints();
void SetScreenHints(bool enabled);

// Forgets the last announced widget so the next poll re-announces.
void Reset();

// Per-panel resolution diagnostics after a persistent miss (one expensive
// pass over every open panel); off unless the console trace is on.
void SetDiagnosticsEnabled(bool enabled);

}
