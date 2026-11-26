#include "binding_store.h"

#include <charconv>

namespace D2RAccess::BindingStore {
namespace {
std::string_view Trim(std::string_view text) {
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
	return text;
}
}

Saved Parse(std::string_view toml, std::string_view pluginId) {
	Saved saved;
	Keys* entry = nullptr;
	const std::string prefix = "[bindings.\"" + std::string(pluginId) + "/";
	while (!toml.empty()) {
		const auto end = toml.find('\n');
		const auto line = Trim(toml.substr(0, end));
		toml.remove_prefix(end == std::string_view::npos ? toml.size() : end + 1);
		if (line.starts_with('[')) {
			entry = nullptr;
			if (line.starts_with(prefix) && line.ends_with("\"]"))
				entry = &saved[std::string(line.substr(prefix.size(), line.size() - prefix.size() - 2))];
			continue;
		}
		const auto equals = line.find('=');
		if (!entry || equals == std::string_view::npos) continue;
		const auto name = Trim(line.substr(0, equals));
		const auto value = Trim(line.substr(equals + 1));
		uint32_t number = 0;
		if (std::from_chars(value.data(), value.data() + value.size(), number).ec != std::errc{}) continue;
		if (name == "primary_key") entry->primaryKey = number;
		else if (name == "primary_modifier") entry->primaryModifier = number;
		else if (name == "secondary_key") entry->secondaryKey = number;
		else if (name == "secondary_modifier") entry->secondaryModifier = number;
	}
	return saved;
}

size_t Unsaved(const std::vector<Action>& actions, const Saved& saved) {
	size_t count = 0;
	for (const auto& action : actions) {
		const auto found = saved.find(action.id);
		if (action.current != (found == saved.end() ? action.defaults : found->second)) ++count;
	}
	return count;
}

}
