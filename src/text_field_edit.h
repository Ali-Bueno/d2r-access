#pragma once

#include <string>
#include <string_view>

namespace D2RAccess::TextField {

enum class EditKind { None, Removed, Added, Replaced };
struct Edit {
	EditKind    kind = EditKind::None;
	std::string text; // the characters removed or added, or the whole new contents
};

// Classifies the change from `before` to `after` in an edited box.
Edit Classify(std::string_view before, std::string_view after);

}
