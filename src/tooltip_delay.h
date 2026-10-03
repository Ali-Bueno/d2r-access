#pragma once

#include <cstdint>

namespace D2RL { struct PluginContext; }

// The controller layout holds every tooltip back 400 ms before it is drawn
// (global/ui/layouts/controller/tooltipspanelhd.json showDelayMilliseconds;
// the mouse layout's is 0), and an item's name comes from its tooltip, so each
// highlight was named 400 ms late. Setting the delay to 0 on the live panel
// makes the controller show tooltips the way the mouse does.
//
// The panel is only touched when it holds exactly the controller layout's
// values -- screenEdgePadding 50 and showDelayMilliseconds 400 -- so a changed
// layout or a moved field is left alone and logged instead.
namespace D2RAccess::TooltipDelay {

// UI thread, once per poll with the TooltipsPanel (0 when absent). The game
// may rebuild the panel from its layout, so the check runs every time.
void Apply(const D2RL::PluginContext* context, uintptr_t panel);

}
