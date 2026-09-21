#pragma once

namespace D2RL {
struct PluginContext;
}

// Ctrl+D, "autodetail": read the whole description on every highlight instead
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

// Says the new state after a toggle. UI thread.
void Tick(const D2RL::PluginContext* context);

}
