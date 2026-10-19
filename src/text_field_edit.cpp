#include "text_field_edit.h"

namespace D2RAccess::TextField {

Edit Classify(std::string_view before, std::string_view after) {
	if (before == after) return {};
	if (after.size() < before.size() && before.substr(0, after.size()) == after)
		return {EditKind::Removed, std::string(before.substr(after.size()))};
	if (after.size() > before.size() && after.substr(0, before.size()) == before)
		return {EditKind::Added, std::string(after.substr(before.size()))};
	return {EditKind::Replaced, std::string(after)};
}

}
