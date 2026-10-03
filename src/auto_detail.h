#pragma once

namespace D2RL {
struct PluginContext;
}

// "Autodetail", in the Ctrl+S list: read the whole description on every highlight instead
// of holding everything but the name for right stick up. Covers what that
// gesture reads -- an item's tooltip, a skill's description, an options row's
// explanation -- and leaves the gesture working either way. Off by default,
// for the session only. The user, 2026-09-21: "build a control+d command
// (autodetail) that will read everything on the description reader on
// highlight when toggled on".
namespace D2RAccess::AutoDetail {

bool Register(const D2RL::PluginContext* context);
void Unregister();

bool Enabled();

// Ctrl+H, autodetail's subtoggle: whether the controls the highlighted thing
// offers ("A: Pick Up") are among what autodetail reads, always last
// (control_hints.h). On by default. Off, or with autodetail off, they wait for
// right stick down.
bool ControlTooltips();

// Says the new state after a toggle. UI thread.
void Tick(const D2RL::PluginContext* context);

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetEnabled(bool enabled);
void SetControlTooltips(bool enabled);
}
