#pragma once

#include "ui_text.h"

#include <string>
#include <optional>

namespace D2RAccess::FrontendNativeFallback {

struct Readout {
	std::string text;
	std::string heading;
	std::string context;
	std::string identity;
	UiText::Kind kind = UiText::Kind::Other;
	bool enabled = true;
	std::optional<bool> toggled;
	std::string value;
	std::string labelKey;
};

// Describe only the focused control in a live native panel snapshot.
Readout Describe(const UiText::Snapshot& snapshot);

}
