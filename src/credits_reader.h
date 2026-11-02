#pragma once

#include <D2RLPlugin/api.h>

// The rolling credits (CreditsModalHD.json: a CreditsWidget that fills text boxes from
// CreditsROTW.txt as they scroll in). Each line is said, queued, as it comes onto the screen.
namespace D2RAccess::CreditsReader {

// UI thread.
void Tick(const D2RL::PluginContext* context);

}
