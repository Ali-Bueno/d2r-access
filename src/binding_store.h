#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// The keys D2RLoader saved for the plugin's actions in input-bindings.toml, and which actions
// differ from them. The loader writes that file only on Options' Apply (docs/keyboard-keys.md).
namespace D2RAccess::BindingStore {

struct Keys {
	uint32_t primaryKey = 0, primaryModifier = 0, secondaryKey = 0, secondaryModifier = 0;
	bool operator==(const Keys&) const = default;
};

using Saved = std::map<std::string, Keys, std::less<>>;

// The saved keys of this plugin's actions, by logical id; another plugin's entries are left out.
Saved Parse(std::string_view toml, std::string_view pluginId);

struct Action {
	std::string id;
	Keys defaults; // what the action has when the file does not name it
	Keys current;  // what the loader binds it to now
};

// How many actions are bound to keys that the file would not give them on the next start.
size_t Unsaved(const std::vector<Action>& actions, const Saved& saved);

}
