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

// Writes the WHOLE mapped image instead of one section
// (d2r-access/D2R.image.bin + D2R.image.json).
//
// Why this exists: splicing only .text into the on-disk PE leaves every data
// reference pointing at nothing. The protector expands the image as it maps
// it, so the live .rdata/.data sit about 6.2 MB above where the on-disk
// section table puts them, and several megabytes of code live past the end of
// .text entirely. The measured result is a decompile with not one string
// literal and no RTTI in 138 MB -- which is why no function can be found by
// name. Capturing the mapping whole is what makes the strings and the type
// descriptors land under the addresses the code actually points at.
// Full evidence: docs/decompile.md.
//
// The .json beside it carries the region map, so which bytes came from the
// exe's own allocation is a fact recorded at dump time rather than a guess
// made at import time.
bool EnsureImageDumped(const D2RL::PluginContext* context);

}
