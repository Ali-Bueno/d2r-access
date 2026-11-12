#include "frontend_native_fallback.h"

#include <algorithm>

namespace D2RAccess::FrontendNativeFallback {
namespace {
// The game's key-binding string (keybinds.json) for the I/II tabs over the weapon slots, which have no text.
constexpr const char* SwapWeaponsKey = "Cfgswapweapons";
// The options slider. Its number is drawn outside the tree: the caller puts it in the node's text.
constexpr std::string_view SliderType = "SettingsSliderWidget";

bool Under(const UiText::Snapshot& snapshot, size_t child, size_t parent) {
	for (int at = static_cast<int>(child); at >= 0 && static_cast<size_t>(at) < snapshot.nodes.size();) {
		if (at == static_cast<int>(parent)) return true;
		const int next = snapshot.nodes[static_cast<size_t>(at)].parent;
		if (next >= at) break;
		at = next;
	}
	return false;
}

bool Control(UiText::Kind kind) {
	return kind == UiText::Kind::Button || kind == UiText::Kind::Toggle || kind == UiText::Kind::ListItem
		|| kind == UiText::Kind::List || kind == UiText::Kind::Focusable || kind == UiText::Kind::TextInput
		|| kind == UiText::Kind::ValueControl || kind == UiText::Kind::Tabs || kind == UiText::Kind::TabButton;
}

bool InsideNestedControl(const UiText::Snapshot& snapshot, size_t child, size_t focus) {
	for (int at = snapshot.nodes[child].parent; at >= 0 && static_cast<size_t>(at) < snapshot.nodes.size();) {
		if (at == static_cast<int>(focus)) return false;
		if (Control(snapshot.nodes[static_cast<size_t>(at)].kind)) return true;
		const int next = snapshot.nodes[static_cast<size_t>(at)].parent;
		if (next >= at) break;
		at = next;
	}
	return true;
}

void Append(std::string& target, std::string_view text) {
	const std::string cleaned = UiText::Clean(text);
	if (cleaned.empty() || target == cleaned) return;
	if (!target.empty()) target += ", ";
	target += cleaned;
}

std::string LabelBefore(const UiText::Snapshot& snapshot, size_t focus) {
	const int parent = snapshot.nodes[focus].parent;
	if (parent < 0) return {};
	// A field is usually beside its visible label, under the same group.
	for (int i = static_cast<int>(focus) - 1; i >= 0; --i) {
		const auto& node = snapshot.nodes[static_cast<size_t>(i)];
		if (node.parent == parent && node.kind == UiText::Kind::Text) {
			const auto label = UiText::Clean(node.text);
			if (!label.empty()) return label;
		}
		if (!Under(snapshot, focus, static_cast<size_t>(i)) && node.parent < parent) break;
	}
	// Some layouts group the field under an image while its caption is a sibling.
	const int group = snapshot.nodes[static_cast<size_t>(parent)].parent;
	if (group >= 0) {
		for (int i = parent - 1; i >= 0; --i) {
			const auto& node = snapshot.nodes[static_cast<size_t>(i)];
			if (node.parent == group && node.kind == UiText::Kind::Text) {
				const auto label = UiText::Clean(node.text);
				if (!label.empty()) return label;
			}
		}
	}
	return {};
}

std::string Heading(const UiText::Snapshot& snapshot, std::optional<size_t> modal) {
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		const auto& node = snapshot.nodes[i];
		if (modal && !Under(snapshot, i, *modal)) continue;
		if (node.kind != UiText::Kind::Text || !(node.name == "Title" || node.name == "Heading"
			|| node.name == "OptionsText" || node.name.ends_with("Title"))) continue;
		if (const auto text = UiText::Clean(node.text); !text.empty()) return text;
	}
	return {};
}

std::string CounterPrefix(std::string_view name) {
	for (const std::string_view suffix : {std::string_view{"IncrementButton"}, std::string_view{"DecrementButton"}}) {
		if (name.ends_with(suffix) && name.size() > suffix.size()) return std::string(name.substr(0, name.size() - suffix.size()));
	}
	return {};
}

std::string TextNamed(const UiText::Snapshot& snapshot, std::string_view name) {
	for (const auto& node : snapshot.nodes) {
		if (node.kind == UiText::Kind::Text && node.name == name) {
			const auto text = UiText::Clean(node.text);
			if (!text.empty()) return text;
		}
	}
	return {};
}

}

Readout DescribeRow(const UiText::Snapshot& row) {
	Readout result;
	if (row.nodes.empty()) return result;
	for (const auto& node : row.nodes) {
		if (node.kind == UiText::Kind::Text || node.kind == UiText::Kind::LinkedText) Append(result.text, node.text);
	}
	const auto& self = row.nodes.front();
	result.kind = self.kind;
	result.enabled = self.enabled;
	result.context = row.name;
	result.identity = row.name + "/" + std::to_string(self.id) + "/" + result.text;
	return result;
}

Readout Describe(const UiText::Snapshot& snapshot) {
	Readout result;
	if (snapshot.nodes.empty()) return result;
	std::optional<size_t> modal;
	for (size_t i = 1; i < snapshot.nodes.size(); ++i) {
		if (snapshot.nodes[i].name.ends_with("Modal")) modal = i;
	}
	auto it = std::find_if(snapshot.nodes.begin(), snapshot.nodes.end(), [&](const UiText::Node& node) { return node.id == snapshot.focus; });
	size_t focus = it == snapshot.nodes.end() ? snapshot.nodes.size() : static_cast<size_t>(it - snapshot.nodes.begin());
	if (modal && (focus == snapshot.nodes.size() || !Under(snapshot, focus, *modal))) {
		focus = snapshot.nodes.size();
		for (size_t i = *modal + 1; i < snapshot.nodes.size() && Under(snapshot, i, *modal); ++i) {
			if (snapshot.nodes[i].enabled && snapshot.nodes[i].kind == UiText::Kind::TextInput) { focus = i; break; }
		}
		if (focus == snapshot.nodes.size()) {
			for (size_t i = *modal + 1; i < snapshot.nodes.size() && Under(snapshot, i, *modal); ++i) {
				if (snapshot.nodes[i].enabled && snapshot.nodes[i].kind == UiText::Kind::Button) { focus = i; break; }
			}
		}
	}
	if (focus == snapshot.nodes.size()) return result;
	if (!Control(snapshot.nodes[focus].kind) && snapshot.nodes[focus].parent >= 0
		&& Control(snapshot.nodes[static_cast<size_t>(snapshot.nodes[focus].parent)].kind)) {
		focus = static_cast<size_t>(snapshot.nodes[focus].parent);
	}
	result.context = snapshot.name;
	if (modal) {
		result.context += "/" + std::to_string(snapshot.nodes[*modal].id);
		result.heading = Heading(snapshot, modal);
	} else {
		for (const auto& tab : snapshot.nodes) {
			if (tab.kind == UiText::Kind::Tabs && !tab.text.empty()) {
				result.heading = UiText::Clean(tab.text);
				result.context += "/" + result.heading;
				break;
			}
		}
		if (result.heading.empty()) result.heading = Heading(snapshot, std::nullopt);
	}
	const auto& node = snapshot.nodes[focus];
	result.kind = node.kind;
	result.enabled = node.enabled;
	result.toggled = node.toggled;
	std::string controlText;

	if (node.kind == UiText::Kind::TextInput) {
		Append(controlText, LabelBefore(snapshot, focus));
		if (controlText.empty()) Append(controlText, node.name);
		if (node.type != "MaskedInputTextBoxWidget") result.contents = UiText::Clean(node.text);
	} else if (const std::string prefix = CounterPrefix(node.name); !prefix.empty()) {
		std::string label = TextNamed(snapshot, prefix + "Label");
		if (label.empty()) label = TextNamed(snapshot, prefix + "HostLabel");
		Append(controlText, label);
		result.value = TextNamed(snapshot, prefix + "Value");
		Append(controlText, result.value);
		Append(controlText, node.text);
		if (controlText.empty()) Append(controlText, node.name);
	} else if (node.type == SliderType) {
		// Captioned as a legacy option row (XText), else by the caption beside it in its row.
		std::string label = node.name.empty() ? std::string{} : TextNamed(snapshot, node.name + "Text");
		if (label.empty()) label = LabelBefore(snapshot, focus);
		Append(controlText, label);
		if (controlText.empty()) Append(controlText, node.name);
		result.value = UiText::Clean(node.text);
		result.slider = true;
	} else if (node.type == "DropdownListWidget") {
		Append(controlText, LabelBefore(snapshot, focus));
		result.value = UiText::Clean(node.text);
		Append(controlText, result.value);
		Append(controlText, node.hint);
		if (controlText.empty()) Append(controlText, node.name);
	} else {
		std::string childText;
		for (size_t i = focus + 1; i < snapshot.nodes.size() && Under(snapshot, i, focus); ++i) {
			const auto& child = snapshot.nodes[i];
			if (Control(child.kind) || child.kind == UiText::Kind::Legend || InsideNestedControl(snapshot, i, focus)) continue;
			if (child.kind == UiText::Kind::Text || child.kind == UiText::Kind::LinkedText) Append(childText, child.text);
		}
		// The legacy option rows caption a control X with a box beside it called XText (VideoOptionsPanel.json).
		if (!node.name.empty()) Append(controlText, TextNamed(snapshot, node.name + "Text"));
		Append(controlText, childText);
		Append(controlText, node.text);
		if (controlText.empty() && node.kind == UiText::Kind::Tabs) Append(controlText, node.text);
		if (controlText.empty()) Append(controlText, node.name);
	}
	if (controlText.empty()) return {};
	// Loader icon buttons have no caption, but ship localized action strings.
	if (snapshot.name == "D2RLoaderMultiplayerPanel" && controlText == node.name) {
		if (node.name == "ExternalAddressCopyButton") result.labelKey = "d2rloader:D2RLoaderMultiplayerCopyIpAddress";
		else if (node.name == "ExternalAddressEyeButton") result.labelKey = "d2rloader:D2RLoaderPartyShowIpTooltip";
		else if (node.name == "RefreshButton") result.labelKey = "d2rloader:D2RLoaderMultiplayerRefreshGames";
		else if (node.name == "HostingStatusRecheckButton") result.labelKey = "d2rloader:D2RLoaderMultiplayerHostingRetryTooltip";
		else if (node.name == "HostingInfoButton") {
			std::string status;
			Append(status, TextNamed(snapshot, "HostingStatusLabel"));
			Append(status, TextNamed(snapshot, "HostingStatusText"));
			if (!status.empty()) controlText = status;
		}
	}
	// A hit area with no words of its own: the weapon-swap tabs by the game's string for the swap, any
	// other by what it covers (the skill tree's footer covers the choices left), else by its tooltip.
	if (controlText == node.name) {
		const auto* covered = node.parent >= 0 ? &snapshot.nodes[static_cast<size_t>(node.parent)] : nullptr;
		if (node.name == "weaponswap_left_arm" || node.name == "weaponswap_right_arm") result.labelKey = SwapWeaponsKey;
		else if (covered && !UiText::Clean(covered->text).empty()) {
			controlText.clear();
			Append(controlText, covered->text);
			Append(controlText, node.hint);
		} else if (!UiText::Clean(node.hint).empty()) {
			controlText.clear();
			Append(controlText, node.hint);
		}
	}
	result.text = controlText;
	result.identity = result.context + "/" + std::to_string(node.id) + "/" + controlText;
	if (result.slider) result.identity += "/" + result.value;
	if (result.toggled) result.identity += *result.toggled ? "/1" : "/0";
	result.identity += result.enabled ? "/enabled" : "/disabled";
	return result;
}

}
