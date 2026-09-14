#pragma once

#include <D2RLPlugin/api.h>

// Writes the game's executable code section as it exists in memory. D2R.exe's
// .text is encrypted on disk (Arxan), so offline analysis needs this in-process
// image; .pdata/.rdata on disk stay valid for it. Written once next to the
// plugin (d2r-access/D2R.text.bin + D2R.text.json with base/RVA/size).
namespace D2RAccess::CodeDump {

// Dumps if no dump exists yet. Safe to call repeatedly; returns true when a
// dump file is present afterwards.
bool EnsureDumped(const D2RL::PluginContext* context);

}
