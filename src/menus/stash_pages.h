#pragma once

#include "native_ui.h"
#include <optional>
#include <string>

// Read-only shared-stash page controls. UI owner only; resolve the current
// visible hierarchy on every call, and return owned text/rectangles, not native
// pointers. The keyboard dispatcher uses the existing queued mouse route.
namespace D2RAccess::StashPages {
bool Available();
std::string Label();
std::optional<NativeUi::Rect> Arrow(int delta);
// Native page command with a copied 1/5/10 amount, independent of held keys.
bool Change(int delta, int pages);
}
