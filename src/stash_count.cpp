#include "stash_count.h"

#include <algorithm>
#include <cctype>

namespace D2RAccess::StashCount {
std::string WithCount(const std::string& name, const std::string& shownText, bool shown, const char* separator) {
	const bool number = !shownText.empty()
		&& std::all_of(shownText.begin(), shownText.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
	return shown && number ? name + separator + shownText : name;
}
}
