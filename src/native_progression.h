#pragma once

#include "native_ui.h"

namespace D2RAccess::NativeProgression {
struct Details {
	UiText::Readout readout;
	bool skill = false;
	std::string points, value;
};
// Add companion labels/values to stat actions and identify skill button grids.
// Uses widget archetypes and their native containers, not a panel-name whitelist.
Details Read(NativeUi::Focus focus, const NativeUi::Type& type);
}
