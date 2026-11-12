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
	bool slider = false; // the value is a slider's number, said after its role
	std::string labelKey;
	std::string contents; // a text box's text; masked boxes never expose theirs
};

// Describe only the focused control in a live native panel snapshot.
Readout Describe(const UiText::Snapshot& snapshot);
// Describe a control snapshotted on its own (a Chronicle reward's entry): every word it draws, its
// own buttons' captions included.
Readout DescribeRow(const UiText::Snapshot& row);

}
