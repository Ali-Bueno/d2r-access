#include "focus_tracker.h"

#include "generated/panel_widgets.h"
#include "control_state.h"
#include "input_direction.h"
#include "localization.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace D2RAccess::FocusTracker {

namespace {

// Layout facts (game-data/global/ui/Layouts/Controller/FocusManagerHD.json):
// full-screen root, unanchored, one child "FocusIndicator".
constexpr const char*   FocusManagerPanelNames[] = { "FocusManager", "Controller/FocusManager" };
constexpr Widgets::Step FocusManagerRoot {};
constexpr Widgets::Step FocusIndicatorPath[] = { { "FocusIndicator", 0.0f, 0.0f } };

// Minimum share of a candidate's area the indicator must cover to count as a match.
constexpr double MinimumCoverage = 0.5;

// Value controls declare no children in their layout, but the engine creates
// some at runtime. Measured 2026-09-14: a SettingsSliderWidget owns a child
// "Indicator" (the handle) whose x runs from the slider's left edge (0%) to
// slider.width - indicator.width (100%): Music at 187/417 = 45%, the advanced
// volumes at 417/417 = 100%. Toggles exposed no child.
constexpr const char* SliderWidgetType     = "SettingsSliderWidget";
constexpr const char* SliderIndicatorChild = "Indicator";

// Still probed once per control on its first update message, for the types
// whose state is not readable yet (toggles, dropdowns).
constexpr const char* ValueChildProbes[] = { "Indicator", "Handle", "Knob", "ValueText", "Value", "Text", "Label", "Toggle", "Checkbox", "Icon", "Image", "Selected", "Current", "Option", "Item", "List" };

// A value message is handled one UI update later: sliders so the layout has
// moved the handle, toggles/carousels so InputDirection::Poll has seen the
// press that caused the message (it happens in the same frame, before our tick).
struct PendingValue {
	const Generated::PanelWidgets* panel  = nullptr;
	const Generated::WidgetEntry*  widget = nullptr;
};

// A dropdown opens a popup panel whose rows are clones of one template, all
// with the same name (DropdownListContentsHD.json). The highlighted option is
// the row under the focus indicator: its index is the indicator's distance
// from the first row in row heights.
constexpr const char* DropdownPopupPanel  = "DropdownListContents";
// A dropdown whose options are built at runtime declares this placeholder.
constexpr const char* DynamicStatesMarker = "?";
constexpr const char* DropdownPopupItem   = "ItemTemplate";
constexpr const char* DropdownWidgetType  = "DropdownListWidget";

struct DropdownSession {
	const Generated::PanelWidgets* panel      = nullptr;
	const Generated::WidgetEntry*  widget     = nullptr;
	std::string                    id;
	std::optional<int>             popupIndex; // last row highlighted in the popup
};
DropdownSession g_dropdown;

// Widget type (from the layout JSON) -> spoken control role. Types not listed
// are spoken by label alone.
struct RoleMapping {
	const char* type;
	const char* Strings::Table::*role;
};
constexpr RoleMapping RoleMappings[] = {
	{ "ButtonWidget", &Strings::Table::roleButton },
	{ "ToggleButtonWidget", &Strings::Table::roleCheckbox },
	{ "DropdownListWidget", &Strings::Table::roleDropdown },
	{ "CarouselWidget", &Strings::Table::roleDropdown },
	{ "SettingsSliderWidget", &Strings::Table::roleSlider },
	{ "SliderWidget", &Strings::Table::roleSlider },
	{ "InputTextBoxWidget", &Strings::Table::roleTextField },
	{ "TabBarWidget", &Strings::Table::roleTabs },
};

struct Match {
	const Generated::PanelWidgets* panel  = nullptr;
	const Generated::WidgetEntry*  widget = nullptr;
	double                         score  = 0.0;
};

const char*                  g_focusManagerName = nullptr;
std::optional<Widgets::Rect> g_lastIndicator;
std::string                  g_lastAnnounced;
PendingValue                 g_pendingValue; // read on the next UI tick, after the layout updated

std::string WidgetId(const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	return std::string(catalog.panel) + "/" + widget.name;
}

std::optional<Widgets::Rect> IndicatorRect() {
	if (g_focusManagerName != nullptr) {
		return Widgets::AbsoluteRect(g_focusManagerName, FocusManagerRoot, FocusIndicatorPath);
	}
	for (const char* name : FocusManagerPanelNames) {
		auto rect = Widgets::AbsoluteRect(name, FocusManagerRoot, FocusIndicatorPath);
		if (rect) {
			g_focusManagerName = name;
			return rect;
		}
	}
	return std::nullopt;
}

double Coverage(const Widgets::Rect& indicator, const Widgets::Rect& candidate) {
	if (candidate.width <= 0 || candidate.height <= 0) {
		return 0.0;
	}
	const int32_t left   = std::max(indicator.x, candidate.x);
	const int32_t top    = std::max(indicator.y, candidate.y);
	const int32_t right  = std::min(indicator.x + indicator.width, candidate.x + candidate.width);
	const int32_t bottom = std::min(indicator.y + indicator.height, candidate.y + candidate.height);
	if (right <= left || bottom <= top) {
		return 0.0;
	}
	const double overlap = static_cast<double>(right - left) * static_cast<double>(bottom - top);
	return overlap / (static_cast<double>(candidate.width) * static_cast<double>(candidate.height));
}

const Generated::PanelWidgets* Catalog(std::string_view panel) {
	for (const auto& entry : Generated::PanelCatalog) {
		if (panel == entry.panel) {
			return &entry;
		}
	}
	return nullptr;
}

// Screen-space rect of a catalog widget, applying the panel root's and each
// step's layout anchor.
std::optional<Widgets::Rect> WidgetRect(const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	std::vector<Widgets::Step> steps;
	steps.reserve(widget.pathLength);
	for (uint32_t i = 0; i < widget.pathLength; ++i) {
		steps.push_back({ widget.path[i].name, widget.path[i].anchorX, widget.path[i].anchorY });
	}
	const Widgets::Step root { nullptr, catalog.rootAnchorX, catalog.rootAnchorY };
	return Widgets::AbsoluteRect(catalog.panel, root, steps);
}

std::string Describe(const std::optional<Widgets::Rect>& rect) {
	char text[64] {};
	if (rect) {
		std::snprintf(text, sizeof(text), "%d,%d %dx%d", rect->x, rect->y, rect->width, rect->height);
	} else {
		std::snprintf(text, sizeof(text), "unresolved (%s)", Widgets::LastError());
	}
	return text;
}

int64_t Distance(const Widgets::Rect& a, const Widgets::Rect& b) {
	const int64_t dx = (a.x + a.width / 2) - (b.x + b.width / 2);
	const int64_t dy = (a.y + a.height / 2) - (b.y + b.height / 2);
	return dx * dx + dy * dy;
}

// Best-covered focusable widget across the open panels, most recent panel
// first. With `diagnostics`, logs per panel how many candidates resolved, the
// nearest one, and the first resolution error.
Match Resolve(const D2RL::PluginContext* context, const Widgets::Rect& indicator, bool diagnostics) {
	Match best;
	const auto& open = PanelState::OpenPanels();
	for (auto it = open.rbegin(); it != open.rend(); ++it) {
		const Generated::PanelWidgets* catalog = Catalog(*it);
		if (catalog == nullptr) {
			continue;
		}
		uint32_t                      resolved        = 0;
		const Generated::WidgetEntry* nearest         = nullptr;
		Widgets::Rect                 nearestRect {};
		int64_t                       nearestDistance = INT64_MAX;
		char                          firstError[160] {};
		for (uint32_t i = 0; i < catalog->count; ++i) {
			const Generated::WidgetEntry& widget = catalog->widgets[i];
			const auto                    rect   = WidgetRect(*catalog, widget);
			if (!rect) {
				if (firstError[0] == '\0') {
					std::snprintf(firstError, sizeof(firstError), "%s", Widgets::LastError());
				}
				continue;
			}
			++resolved;
			const int64_t distance = Distance(indicator, *rect);
			if (distance < nearestDistance) {
				nearestDistance = distance;
				nearest         = &widget;
				nearestRect     = *rect;
			}
			const double score = Coverage(indicator, *rect);
			if (score > best.score) {
				best = { catalog, &widget, score };
			}
		}
		if (diagnostics) {
			D2RL::LogInfoF(context, "[focus]   %s (root %s, anchor %.1f,%.1f): %u/%u resolved; nearest %s at %d,%d %dx%d; first error: %s", catalog->panel,
				Describe(Widgets::PanelRect(catalog->panel)).c_str(), catalog->rootAnchorX, catalog->rootAnchorY, resolved, catalog->count,
				nearest != nullptr ? nearest->name : "-", nearestRect.x, nearestRect.y, nearestRect.width, nearestRect.height, firstError[0] != '\0' ? firstError : "none");
		}
		if (best.score >= MinimumCoverage) {
			break;
		}
	}
	return best;
}

std::vector<Widgets::Step> Steps(const Generated::WidgetEntry& widget) {
	std::vector<Widgets::Step> steps;
	steps.reserve(widget.pathLength + 1);
	for (uint32_t i = 0; i < widget.pathLength; ++i) {
		steps.push_back({ widget.path[i].name, widget.path[i].anchorX, widget.path[i].anchorY });
	}
	return steps;
}

// 0-100 from the handle position, or nothing when the control is not a slider
// or its handle cannot be resolved.
std::optional<int> SliderPercent(const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	if (std::strcmp(widget.type, SliderWidgetType) != 0) {
		return std::nullopt;
	}
	const Widgets::Step        root { nullptr, catalog.rootAnchorX, catalog.rootAnchorY };
	std::vector<Widgets::Step> steps = Steps(widget);
	const auto                 track = Widgets::AbsoluteRect(catalog.panel, root, steps);
	steps.push_back({ SliderIndicatorChild, 0.0f, 0.0f });
	const auto handle = Widgets::AbsoluteRect(catalog.panel, root, steps);
	if (!track || !handle) {
		return std::nullopt;
	}
	const int32_t range = track->width - handle->width;
	if (range <= 0) {
		return std::nullopt;
	}
	const int32_t offset  = std::clamp(handle->x - track->x, 0, range);
	const double  percent = 100.0 * static_cast<double>(offset) / static_cast<double>(range);
	return static_cast<int>(percent + 0.5);
}

std::string PercentText(int percent) {
	char text[16] {};
	std::snprintf(text, sizeof(text), "%d%%", percent);
	return text;
}

std::string TextForKey(const char* key) {
	return key != nullptr ? Localization::Text(key) : std::string {};
}

// Own text, else the row label next to it, else a literal, else the name.
std::string Label(const Generated::WidgetEntry& widget) {
	std::string text = TextForKey(widget.textKey);
	if (text.empty()) {
		text = TextForKey(widget.labelKey);
	}
	if (text.empty() && widget.text != nullptr && widget.text[0] != '\0') {
		text = widget.text;
	}
	if (text.empty()) {
		text = widget.name;
	}
	return text;
}

const char* Role(const Generated::WidgetEntry& widget) {
	for (const RoleMapping& mapping : RoleMappings) {
		if (std::strcmp(widget.type, mapping.type) == 0) {
			return Strings::Current().*mapping.role;
		}
	}
	return nullptr;
}

// A dropdown/carousel value arrives as an index into its layout `states`; a
// slider or free value arrives as text. Either is localized when it is a key.
std::string ValueText(const Generated::WidgetEntry& widget, std::string_view raw) {
	const bool numeric = !raw.empty() && std::all_of(raw.begin(), raw.end(), [](char c) { return c >= '0' && c <= '9'; });
	if (widget.states != nullptr && numeric) {
		const unsigned long index = std::strtoul(std::string(raw).c_str(), nullptr, 10);
		if (index < widget.stateCount) {
			const std::string localized = Localization::Text(widget.states[index]);
			return localized.empty() ? std::string(widget.states[index]) : localized;
		}
	}
	const std::string localized = Localization::Text(raw);
	return localized.empty() ? std::string(raw) : localized;
}

void ProbeValueChildren(const D2RL::PluginContext* context, const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	static std::vector<std::string> probed;
	const std::string               id = std::string(catalog.panel) + "/" + widget.name;
	if (std::find(probed.begin(), probed.end(), id) != probed.end()) {
		return;
	}
	probed.push_back(id);

	std::vector<Widgets::Step> steps = Steps(widget);
	const Widgets::Step        root { nullptr, catalog.rootAnchorX, catalog.rootAnchorY };
	const auto                 own = Widgets::AbsoluteRect(catalog.panel, root, steps);
	D2RL::LogInfoF(context, "[probe] %s (%s) rect %s", id.c_str(), widget.type, Describe(own).c_str());
	for (const char* child : ValueChildProbes) {
		steps.push_back({ child, 0.0f, 0.0f });
		const auto rect = Widgets::AbsoluteRect(catalog.panel, root, steps);
		steps.pop_back();
		if (rect) {
			D2RL::LogInfoF(context, "[probe]   child '%s' at %s", child, Describe(rect).c_str());
		}
	}
}

// Row highlighted in a dropdown popup: spoken as the owning dropdown's state
// name when its options are static, else as "option N of M".
bool HasStaticStates(const Generated::WidgetEntry* widget) {
	return widget != nullptr && widget->states != nullptr && widget->stateCount > 0 && std::strcmp(widget->states[0], DynamicStatesMarker) != 0;
}

// The popup captures focus while open: the highlighted row is the indicator's
// distance from the first row (all rows share the template's name and size).
void AnnouncePopupRow(const D2RL::PluginContext* context, const Generated::PanelWidgets& popup, const Widgets::Rect& indicator) {
	const Generated::WidgetEntry* rowTemplate = nullptr;
	for (uint32_t i = 0; i < popup.count; ++i) {
		if (std::strcmp(popup.widgets[i].name, DropdownPopupItem) == 0) {
			rowTemplate = &popup.widgets[i];
		}
	}
	if (rowTemplate == nullptr) {
		return;
	}
	const auto row = WidgetRect(popup, *rowTemplate);
	if (!row || row->height <= 0) {
		return;
	}
	const int index = static_cast<int>(std::lround(static_cast<double>(indicator.y - row->y) / static_cast<double>(row->height)));
	if (index < 0 || (g_dropdown.popupIndex && *g_dropdown.popupIndex == index)) {
		return;
	}
	g_dropdown.popupIndex = index;

	std::string phrase;
	if (HasStaticStates(g_dropdown.widget) && index < static_cast<int>(g_dropdown.widget->stateCount)) {
		phrase = Localization::Text(g_dropdown.widget->states[index]);
		if (phrase.empty()) {
			phrase = g_dropdown.widget->states[index];
		}
	} else {
		const int count = g_dropdown.widget != nullptr ? static_cast<int>(g_dropdown.widget->stateCount) : 0;
		char      text[64] {};
		std::snprintf(text, sizeof(text), Strings::Current().optionOfCount, index + 1, count);
		phrase = text;
	}
	D2RL::LogInfoF(context, "[focus] popup row %d -> \"%s\"", index, phrase.c_str());
	ScreenReader::Speak(phrase, true);
}

void Announce(const D2RL::PluginContext* context, const Match& match) {
	std::string id = std::string(match.panel->panel) + "/" + match.widget->name;
	if (id == g_lastAnnounced) {
		return;
	}
	g_lastAnnounced = std::move(id);
	if (std::strcmp(match.widget->type, DropdownWidgetType) == 0) {
		g_dropdown = { match.panel, match.widget, g_lastAnnounced, std::nullopt };
	}

	std::string phrase = Label(*match.widget);
	if (const char* role = Role(*match.widget); role != nullptr) {
		phrase += ", ";
		phrase += role;
	}
	if (const auto percent = SliderPercent(*match.panel, *match.widget); percent) {
		phrase += ", ";
		phrase += PercentText(*percent);
	}
	if (const std::string state = ControlState::Describe(*match.widget, g_lastAnnounced); !state.empty()) {
		phrase += ", ";
		phrase += state;
	}
	const std::string tooltip = TextForKey(match.widget->tooltipKey);

	D2RL::LogInfoF(context, "[focus] %s/%s (%s) -> \"%s\" coverage=%.2f", match.panel->panel, match.widget->name, match.widget->type, phrase.c_str(), match.score);
	ScreenReader::Speak(phrase, true);
	if (!tooltip.empty()) {
		ScreenReader::Speak(tooltip, false);
	}
}

}

void Tick(const D2RL::PluginContext* context) {
	if (context == nullptr || !Widgets::IsAvailable()) {
		return;
	}
	// A value change is read one update after its message so the layout has
	// moved the handle; it must not wait for the focus indicator to move.
	if (g_pendingValue.widget != nullptr) {
		const PendingValue pending = g_pendingValue;
		g_pendingValue             = {};
		const std::string id       = WidgetId(*pending.panel, *pending.widget);
		if (std::strcmp(pending.widget->type, SliderWidgetType) == 0) {
			if (const auto percent = SliderPercent(*pending.panel, *pending.widget); percent) {
				D2RL::LogInfoF(context, "[value] %s -> %d%%", id.c_str(), *percent);
				ScreenReader::Speak(PercentText(*percent), true);
			}
		} else {
			// A dropdown takes the row picked in its popup; toggles and
			// carousels step from the player's last horizontal input.
			if (std::strcmp(pending.widget->type, DropdownWidgetType) == 0 && g_dropdown.id == id && g_dropdown.popupIndex) {
				ControlState::SetIndex(id, *g_dropdown.popupIndex);
			} else {
				ControlState::OnUpdateMessage(*pending.widget, id);
			}
			const std::string state = ControlState::Describe(*pending.widget, id);
			D2RL::LogInfoF(context, "[value] %s -> \"%s\" (last input %d)", id.c_str(), state.c_str(), static_cast<int>(InputDirection::LastHorizontal(1000)));
			if (!state.empty()) {
				ScreenReader::Speak(state, true);
			} else {
				ProbeValueChildren(context, *pending.panel, *pending.widget);
			}
		}
	}

	const auto indicator = IndicatorRect();
	if (indicator == g_lastIndicator) {
		return;
	}
	g_lastIndicator = indicator;
	if (!indicator) {
		return;
	}

	// An open dropdown popup captures focus: read its rows, not the panels below.
	if (PanelState::Active() == DropdownPopupPanel) {
		if (const Generated::PanelWidgets* popup = Catalog(DropdownPopupPanel); popup != nullptr) {
			AnnouncePopupRow(context, *popup, *indicator);
		}
		return;
	}

	Match match = Resolve(context, *indicator, false);
	if (match.widget == nullptr || match.score < MinimumCoverage) {
		D2RL::LogInfoF(context, "[focus] indicator at %d,%d %dx%d matched nothing (best %.2f); screen root %s", indicator->x, indicator->y, indicator->width, indicator->height, match.score,
			Describe(Widgets::PanelRect(g_focusManagerName != nullptr ? g_focusManagerName : FocusManagerPanelNames[0])).c_str());
		match = Resolve(context, *indicator, true);
		return;
	}
	Announce(context, match);
}

bool AnnounceValue(const D2RL::PluginContext* context, std::string_view target, std::string_view command, std::string_view value) {
	const auto& open = PanelState::OpenPanels();
	for (auto it = open.rbegin(); it != open.rend(); ++it) {
		const Generated::PanelWidgets* catalog = Catalog(*it);
		if (catalog == nullptr) {
			continue;
		}
		for (uint32_t i = 0; i < catalog->count; ++i) {
			const Generated::WidgetEntry& widget = catalog->widgets[i];
			if (widget.updateTarget == nullptr || target != widget.updateTarget || command != widget.updateCommand) {
				continue;
			}
			// Only the focused control's changes are spoken: panels also fire
			// every control's update message when they initialize.
			if (WidgetId(*catalog, widget) != g_lastAnnounced) {
				return true;
			}
			if (!value.empty()) {
				const std::string label = Label(widget);
				const std::string text  = ValueText(widget, value);
				char              phrase[512] {};
				std::snprintf(phrase, sizeof(phrase), Strings::Current().valueChanged, label.c_str(), text.c_str());
				D2RL::LogInfoF(context, "[value] %s <- \"%.*s\" -> \"%s\"", WidgetId(*catalog, widget).c_str(), static_cast<int>(value.size()), value.data(), phrase);
				ScreenReader::Speak(phrase, true);
			} else {
				g_pendingValue = { catalog, &widget };
			}
			return true;
		}
	}
	return false;
}

void Reset() {
	g_lastIndicator.reset();
	g_lastAnnounced.clear();
}

}
