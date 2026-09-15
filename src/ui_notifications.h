#pragma once

#include "ui_text.h"

namespace D2RAccess::UiNotifications {

// Read active notices only. Chat history and unrelated lists are excluded.
std::vector<std::string> Describe(const UiText::Snapshot& snapshot);

class Reader {
public:
	std::vector<std::string> Update(const std::vector<std::string>& visible, uint64_t now);
	void Reset();
	const std::string& Repeat() const { return last_; }
private:
	struct Entry { std::string text; uint64_t since; bool spoken; };
	std::vector<Entry> entries_;
	std::string last_;
};
}
