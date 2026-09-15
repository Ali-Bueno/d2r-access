#include "control_state.h"

#include "input_direction.h"
#include "localization.h"
#include "settings_file.h"
#include "speech_strings.h"

#include <cmath>
#include <cstring>
#include <iterator>
#include <unordered_map>

namespace D2RAccess::ControlState {

namespace {

constexpr const char* ToggleType = "ToggleButtonWidget";

// A value message follows the D-pad press that caused it within one UI update
// or so; anything older is not the cause.
constexpr unsigned DirectionMaxAgeMs = 400;

// Controls whose stored value is not a position in their layout `states` but
// the game's own enumeration. The text language is stored as the locale's
// index in the game's string tables (game-data/local/lng/strings/ui.json lists
// the locales in this order: enUS, zhTW, deDE, esES, frFR, itIT, koKR, plPL,
// esMX, jaJP, ptBR, ruRU, zhCN), named here with the layout's state names.
struct StoredValueOrder {
	const char*        updateCommand;
	const char* const* stateNames;
	int                count;
};
constexpr const char* const TextLanguageStoredOrder[] = { "English", "ChineseTraditional", "German", "Spanish", "French", "Italian", "Korean", "Polish", "SpanishMexican", "Japanese", "Portuguese", "Russian", "ChineseSimplified" };
constexpr StoredValueOrder StoredValueOrders[] = {
	{ "SetTextLanguage", TextLanguageStoredOrder, static_cast<int>(std::size(TextLanguageStoredOrder)) },
};

std::unordered_map<std::string, int> g_index;

bool IsToggle(const Generated::WidgetEntry& widget) {
	return std::strcmp(widget.type, ToggleType) == 0;
}

int Count(const Generated::WidgetEntry& widget) {
	return IsToggle(widget) ? 2 : static_cast<int>(widget.stateCount);
}

std::optional<int> Baseline(const Generated::WidgetEntry& widget) {
	if (widget.updateCommand == nullptr) {
		return std::nullopt;
	}
	(void)SettingsFile::Reload();
	const auto value = SettingsFile::NumberForCommand(widget.updateCommand);
	if (!value) {
		return std::nullopt;
	}
	int       index = static_cast<int>(std::lround(*value));
	const int count = Count(widget);
	for (const StoredValueOrder& order : StoredValueOrders) {
		if (std::strcmp(order.updateCommand, widget.updateCommand) != 0) {
			continue;
		}
		if (index < 0 || index >= order.count || widget.states == nullptr) {
			return std::nullopt;
		}
		const char* stateName = order.stateNames[index];
		index                 = -1;
		for (int i = 0; i < count; ++i) {
			if (std::strcmp(widget.states[i], stateName) == 0) {
				index = i;
			}
		}
	}
	if (index < 0 || (count > 0 && index >= count)) {
		return std::nullopt;
	}
	return index;
}

}

std::optional<int> Index(const Generated::WidgetEntry& widget, std::string_view widgetId) {
	const std::string id(widgetId);
	if (const auto it = g_index.find(id); it != g_index.end()) {
		return it->second;
	}
	const auto baseline = Baseline(widget);
	if (baseline) {
		g_index[id] = *baseline;
	}
	return baseline;
}

std::string Describe(const Generated::WidgetEntry& widget, std::string_view widgetId) {
	const auto index = Index(widget, widgetId);
	if (!index) {
		return {};
	}
	if (IsToggle(widget)) {
		return *index != 0 ? Strings::Current().checked : Strings::Current().unchecked;
	}
	if (widget.states != nullptr && *index < static_cast<int>(widget.stateCount)) {
		const std::string localized = Localization::Text(widget.states[*index]);
		return localized.empty() ? std::string(widget.states[*index]) : localized;
	}
	return {};
}

void OnUpdateMessage(const Generated::WidgetEntry& widget, std::string_view widgetId, std::optional<int> before) {
	const auto current = before ? before : Index(widget, widgetId);
	if (!current) {
		return;
	}
	const std::string id(widgetId);
	if (IsToggle(widget)) {
		g_index[id] = *current != 0 ? 0 : 1;
		return;
	}
	const int count = Count(widget);
	if (count <= 0) {
		return;
	}
	switch (InputDirection::LastHorizontal(DirectionMaxAgeMs)) {
	case InputDirection::Horizontal::Right:
		g_index[id] = (*current + 1) % count;
		break;
	case InputDirection::Horizontal::Left:
		g_index[id] = (*current + count - 1) % count;
		break;
	case InputDirection::Horizontal::None:
		break;
	}
}

void SetIndex(std::string_view widgetId, int index) {
	g_index[std::string(widgetId)] = index;
}

void Invalidate() {
	g_index.clear();
}

}
