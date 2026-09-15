#pragma once

#include "ui_text.h"
#include <string>
#include <vector>

namespace D2RAccess::NativeUi {
struct Focus { uintptr_t panel = 0, widget = 0; };
struct Type {
	std::string name;
	UiText::Kind kind = UiText::Kind::Other;
	UiText::Surface surface = UiText::Surface::Other;
};

// UI thread only. All object addresses are re-resolved every poll.
Type Classify(uintptr_t widget);
bool Visible(uintptr_t widget);
Focus CurrentFocus();
std::vector<uintptr_t> Panels();
UiText::Snapshot Snapshot(uintptr_t panel, uintptr_t focus);
void Reset();
}
