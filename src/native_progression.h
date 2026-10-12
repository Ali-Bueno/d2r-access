#pragma once

#include "native_ui.h"

namespace D2RAccess::NativeProgression {
struct Details {
	UiText::Readout readout;
	bool skill = false;
	uintptr_t tab = 0; // a skill's branch (its TabWidget)
	std::string name; // table name available before the skill tooltip renders
	std::string points, value;
	std::string help;     // part of the readout: the skill's current binding
	std::string controls; // what the buttons do here -- a control hint (control_hints.h)
	std::string description; // a stat's own explanation, for right stick up or autodetail
};
// Add companion labels/values to stat actions and identify skill button grids.
// Uses widget archetypes and their native containers, not a panel-name whitelist.
Details Read(NativeUi::Focus focus, const NativeUi::Type& type);
}
