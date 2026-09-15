#include "focus_tracker.h"

#include "generated/panel_widgets.h"
#include "character_saves.h"
#include "control_state.h"
#include "display_modes.h"
#include "input_direction.h"
#include "localization.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "widgets.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

// The indicator is polled at most this often: resolving it costs four loader
// calls, and menus do not move faster than this.
constexpr unsigned long long PollIntervalMs = 33;

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
	std::optional<int>             before; // state index when the message arrived
};

// A dropdown opens a popup panel whose rows are clones of one template, all
// with the same name (DropdownListContentsHD.json). The highlighted option is
// the row under the focus indicator: its index is the indicator's distance
// from the first row in row heights.
constexpr const char* DropdownPopupPanel  = "DropdownListContents";
// A dropdown whose options are built at runtime declares this placeholder.
constexpr const char* DynamicStatesMarker = "?";
// The one runtime list we can rebuild: the display's modes (VideoOptionsPanel).
constexpr const char* ScreenResolutionWidget = "ScreenResolution";
// DropdownListContentsHD.json: rows live in ListWidget "Items", scrolled by
// ScrollControllerWidget "ScrollController". Measured 2026-09-15: the game
// clones ItemTemplate into 13 children of Items named "ListItem0".."ListItem12"
// (one per VISIBLE slot, recycled as the list scrolls), so no rect reveals
// which option a slot shows; "Items", "Selector" and the scroll bar's
// "Indicator" report screen positions or stale values. Until the slots'
// text can be read, the scroll is tracked from the presses: one that left
// the indicator on the same row scrolled the list by one. It must follow
// within one update.
constexpr unsigned PopupDirectionMaxAgeMs = 400;

// CharacterSelectPanelHD.json: the character list's rows are clones of one
// ListItemWidget template whose texts (name, class and level) are set at
// runtime, so the row under the indicator is spoken from the save files
// (character_saves.h), most recently played first like the list.
constexpr const char* CharacterSelectPanelName = "CharacterSelectPanel";
constexpr const char* CharacterRowWidget       = "CharacterTemplate";
// The controller legend under the list (filled at runtime, unreadable) shows
// play on the A face button and delete on X (observed on an Xbox pad,
// 2026-09-15); the layout's QuickPlayButton text and DeleteCharacter tooltip
// name the actions.
struct LegendAction {
	const char* button;
	const char* textKey;
};
constexpr LegendAction CharacterRowActions[] = {
	{ "A", "PlayTab" },
	{ "X", "strDelete" },
};

std::string g_focusedCharacter;

// Widgets the game announces itself through a UI message that carries the
// selection (CharacterCreate:Hover <ClassKey>), so the focus tracker stays
// quiet on them instead of speaking their panel's title.
constexpr const char* MessageAnnouncedTypes[] = { "HDCharacterSelectorWidget" };
constexpr const char* DropdownPopupItem   = "ItemTemplate";
constexpr const char* DropdownWidgetType  = "DropdownListWidget";

// Controls the game disables depending on another control's state. The SDK
// cannot read the enabled flag, so the dependency observed in the game is
// recorded here (indices refer to the controlling widget's `states`).
struct DisabledWhen {
	const char* panel;
	const char* widget;
	const char* controller; // widget in the same panel whose state decides
	int         controllerIndex;
};
constexpr DisabledWhen DisabledWhenRules[] = {
	{ "VideoOptionsPanel", "ScreenResolution", "WindowMode", 1 }, // resolution is fixed in Fullscreen
};

// The popup shows a window of rows: once the highlight reaches the last
// visible row the list scrolls under a still indicator, so the option index is
// the visible row plus how far the list has scrolled.
struct DropdownSession {
	const Generated::PanelWidgets* panel      = nullptr;
	const Generated::WidgetEntry*  widget     = nullptr;
	std::string                    id;
	std::optional<int>             popupIndex;   // last option highlighted in the popup
	std::optional<int>             visibleRow;   // row under the indicator when it was read
	int                            scrollOffset = 0;
};
DropdownSession g_dropdown;
bool            g_popupWasActive = false;

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
const Generated::PanelWidgets* g_lastMatchedPanel = nullptr; // checked first: focus usually stays in one panel
std::unordered_map<std::string, std::optional<Widgets::Rect>> g_rectCache; // widget id -> resolved rect (or failure)
std::optional<Widgets::Rect> g_lastIndicator;
bool                         g_forceResolve = false; // content moved under a still indicator (scroll, panel change)
int                          g_scrollInputs = 0;     // ManualScrollInput messages since the last popup read
unsigned long long           g_lastPollTick = 0;
std::string                  g_lastAnnounced;
PendingValue                 g_pendingValue; // read on the next UI tick, after the layout updated

std::string WidgetId(const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	return std::string(catalog.panel) + "/" + widget.name;
}

const Generated::WidgetEntry* FindWidget(const Generated::PanelWidgets& catalog, const char* name) {
	for (uint32_t i = 0; i < catalog.count; ++i) {
		if (std::strcmp(catalog.widgets[i].name, name) == 0) {
			return &catalog.widgets[i];
		}
	}
	return nullptr;
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
// step's layout anchor. Resolving walks the widget tree by name through the
// loader on every call, so results are cached until the UI changes.
std::optional<Widgets::Rect> WidgetRect(const Generated::PanelWidgets& catalog, const Generated::WidgetEntry& widget) {
	const std::string id = WidgetId(catalog, widget);
	if (const auto cached = g_rectCache.find(id); cached != g_rectCache.end()) {
		return cached->second;
	}
	std::vector<Widgets::Step> steps;
	steps.reserve(widget.pathLength);
	for (uint32_t i = 0; i < widget.pathLength; ++i) {
		steps.push_back({ widget.path[i].name, widget.path[i].anchorX, widget.path[i].anchorY });
	}
	const Widgets::Step root { nullptr, catalog.rootAnchorX, catalog.rootAnchorY };
	const auto          rect = Widgets::AbsoluteRect(catalog.panel, root, steps);
	g_rectCache[id]          = rect;
	return rect;
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
	std::vector<const Generated::PanelWidgets*> order;
	if (g_lastMatchedPanel != nullptr && std::find(open.begin(), open.end(), g_lastMatchedPanel->panel) != open.end()) {
		order.push_back(g_lastMatchedPanel);
	}
	for (auto it = open.rbegin(); it != open.rend(); ++it) {
		const Generated::PanelWidgets* catalog = Catalog(*it);
		if (catalog != nullptr && catalog != g_lastMatchedPanel) {
			order.push_back(catalog);
		}
	}
	for (const Generated::PanelWidgets* catalog : order) {
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
			// A stacked layout (options table) lays out visible rows only:
			// when two siblings end up on the same spot, the earlier one was
			// hidden, so on a tie within a panel the later widget wins.
			const double score = Coverage(indicator, *rect);
			if (score > best.score || (score == best.score && score > 0.0 && best.panel == catalog)) {
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

// Own text, else the row label next to it, else a literal, else the tooltip
// (icon-only buttons such as DeleteCharacter carry nothing else), else the name.
std::string Label(const Generated::WidgetEntry& widget) {
	std::string text = TextForKey(widget.textKey);
	if (text.empty()) {
		text = TextForKey(widget.labelKey);
	}
	if (text.empty() && widget.text != nullptr && widget.text[0] != '\0') {
		text = widget.text;
	}
	if (text.empty()) {
		text = TextForKey(widget.tooltipKey);
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

// Number of options in the open dropdown's popup, when known.
std::optional<int> PopupOptionCount() {
	if (HasStaticStates(g_dropdown.widget)) {
		return static_cast<int>(g_dropdown.widget->stateCount);
	}
	if (g_dropdown.widget != nullptr && std::strcmp(g_dropdown.widget->name, ScreenResolutionWidget) == 0) {
		return static_cast<int>(DisplayModes::Modes().size());
	}
	return std::nullopt;
}

// Fallback option index, tracking the list's scroll from the presses: the
// popup is taken as unscrolled when it opens; afterwards every scroll input
// the game reported either moved the highlight one row or, when the
// highlight was already at the edge, scrolled the list by one. So the inputs
// not accounted for by the highlight's movement are the rows scrolled. A
// highlight that moved against the pressed direction wrapped around.
int PopupTrackedOption(int visibleRow) {
	const auto count     = PopupOptionCount();
	const auto direction = InputDirection::LastVertical(PopupDirectionMaxAgeMs);
	const int  inputs    = g_scrollInputs;
	g_scrollInputs       = 0;
	if (!g_dropdown.visibleRow) {
		g_dropdown.scrollOffset = 0;
	} else if (direction == InputDirection::Vertical::Down && visibleRow < *g_dropdown.visibleRow) {
		g_dropdown.scrollOffset = 0; // wrapped to the top
	} else if (direction == InputDirection::Vertical::Up && visibleRow > *g_dropdown.visibleRow && count) {
		g_dropdown.scrollOffset = *count - 1 - visibleRow; // wrapped to the bottom
	} else {
		const int moved    = std::abs(visibleRow - *g_dropdown.visibleRow);
		const int scrolled = std::max(0, inputs - moved);
		if (direction == InputDirection::Vertical::Down) {
			g_dropdown.scrollOffset += scrolled;
		} else if (direction == InputDirection::Vertical::Up) {
			g_dropdown.scrollOffset -= scrolled;
		}
	}
	g_dropdown.scrollOffset = std::max(0, g_dropdown.scrollOffset);
	if (count) {
		g_dropdown.scrollOffset = std::min(g_dropdown.scrollOffset, std::max(0, *count - 1 - visibleRow));
	}
	g_dropdown.visibleRow = visibleRow;
	return visibleRow + g_dropdown.scrollOffset;
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
	const int visibleRow = static_cast<int>(std::lround(static_cast<double>(indicator.y - row->y) / static_cast<double>(row->height)));
	if (visibleRow < 0) {
		return;
	}
	const int index = PopupTrackedOption(visibleRow);
	if (g_dropdown.popupIndex && *g_dropdown.popupIndex == index) {
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
		char text[64] {};
		const auto& modes = DisplayModes::Modes();
		if (g_dropdown.widget != nullptr && std::strcmp(g_dropdown.widget->name, ScreenResolutionWidget) == 0 && index < static_cast<int>(modes.size())) {
			std::snprintf(text, sizeof(text), Strings::Current().resolution, modes[static_cast<size_t>(index)].width, modes[static_cast<size_t>(index)].height);
		} else {
			// Other runtime lists have no known content or count.
			std::snprintf(text, sizeof(text), Strings::Current().optionIndex, index + 1);
		}
		phrase = text;
	}
	D2RL::LogInfoF(context, "[focus] popup option %d (row %d + scroll %d) -> \"%s\"", index, visibleRow, g_dropdown.scrollOffset, phrase.c_str());
	ScreenReader::Speak(phrase, true);
}

// Index of the character list row under the indicator, or -1 when the
// matched widget is not that list's row template.
int CharacterRowIndex(const Match& match, const Widgets::Rect& indicator) {
	if (std::strcmp(match.panel->panel, CharacterSelectPanelName) != 0 || std::strcmp(match.widget->name, CharacterRowWidget) != 0) {
		return -1;
	}
	const auto row = WidgetRect(*match.panel, *match.widget);
	if (!row || row->height <= 0) {
		return -1;
	}
	return std::max(0, static_cast<int>(std::lround(static_cast<double>(indicator.y - row->y) / static_cast<double>(row->height))));
}

// "name, class, level N" for the character shown on that row; records the
// name for the delete modal.
std::string CharacterRowPhrase(const D2RL::PluginContext* context, int rowIndex) {
	const std::vector<CharacterSaves::Character> characters = CharacterSaves::Load(context);
	g_focusedCharacter.clear();
	if (rowIndex >= static_cast<int>(characters.size())) {
		char text[64] {};
		std::snprintf(text, sizeof(text), Strings::Current().optionIndex, rowIndex + 1);
		return text;
	}
	g_focusedCharacter = characters[static_cast<size_t>(rowIndex)].name;
	const CharacterSaves::Character& character = characters[static_cast<size_t>(rowIndex)];
	std::string                      phrase    = character.name;
	if (const char* classKey = CharacterSaves::ClassKey(character.classIndex); classKey != nullptr) {
		const std::string className = Localization::Text(classKey);
		phrase += ", ";
		phrase += className.empty() ? classKey : className;
	}
	char level[32] {};
	std::snprintf(level, sizeof(level), Strings::Current().characterLevel, character.level);
	phrase += ", ";
	phrase += level;
	return phrase;
}

void Announce(const D2RL::PluginContext* context, const Match& match, const Widgets::Rect& indicator) {
	const int   characterRow = CharacterRowIndex(match, indicator);
	std::string id           = std::string(match.panel->panel) + "/" + match.widget->name;
	if (characterRow >= 0) {
		id += "#" + std::to_string(characterRow);
	}
	if (id == g_lastAnnounced) {
		return;
	}
	g_lastAnnounced = std::move(id);
	g_lastMatchedPanel = match.panel;
	for (const char* type : MessageAnnouncedTypes) {
		if (std::strcmp(match.widget->type, type) == 0) {
			return;
		}
	}
	if (characterRow >= 0) {
		const std::string phrase = CharacterRowPhrase(context, characterRow);
		D2RL::LogInfoF(context, "[focus] %s row %d -> \"%s\"", g_lastAnnounced.c_str(), characterRow, phrase.c_str());
		ScreenReader::Speak(phrase, true);
		std::string legend;
		for (const LegendAction& action : CharacterRowActions) {
			if (const std::string text = TextForKey(action.textKey); !text.empty()) {
				legend += legend.empty() ? "" : ", ";
				legend += std::string(action.button) + ": " + text;
			}
		}
		if (!legend.empty()) {
			ScreenReader::Speak(legend, false);
		}
		return;
	}
	g_focusedCharacter.clear();
	if (std::strcmp(match.widget->type, DropdownWidgetType) == 0) {
		g_dropdown = { match.panel, match.widget, g_lastAnnounced };
	}

	const std::string label  = Label(*match.widget);
	std::string       phrase = label;
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
	for (const DisabledWhen& rule : DisabledWhenRules) {
		if (std::strcmp(rule.panel, match.panel->panel) != 0 || std::strcmp(rule.widget, match.widget->name) != 0) {
			continue;
		}
		const Generated::WidgetEntry* controller = FindWidget(*match.panel, rule.controller);
		if (controller != nullptr && ControlState::Index(*controller, WidgetId(*match.panel, *controller)) == rule.controllerIndex) {
			phrase += ", ";
			phrase += Strings::Current().unavailable;
		}
	}
	const std::string tooltip = TextForKey(match.widget->tooltipKey);

	D2RL::LogInfoF(context, "[focus] %s/%s (%s) -> \"%s\" coverage=%.2f", match.panel->panel, match.widget->name, match.widget->type, phrase.c_str(), match.score);
	ScreenReader::Speak(phrase, true);
	if (!tooltip.empty() && tooltip != label) {
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
				ControlState::OnUpdateMessage(*pending.widget, id, pending.before);
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

	const unsigned long long now = GetTickCount64();
	if (!g_forceResolve && now - g_lastPollTick < PollIntervalMs) {
		return;
	}
	g_lastPollTick = now;
	Widgets::BeginPoll();


	// When a list scrolls to keep the focused row visible, the indicator can
	// stay put while the rows move under it, so a scroll forces a re-check.
	const auto indicator = IndicatorRect();
	if (indicator == g_lastIndicator && !g_forceResolve) {
		return;
	}
	g_forceResolve  = false;
	g_lastIndicator = indicator;
	if (!indicator) {
		return;
	}

	// An open dropdown popup captures focus: read its rows, not the panels below.
	const bool popupActive = PanelState::Active() == DropdownPopupPanel;
	if (popupActive && !g_popupWasActive) {
		g_dropdown.popupIndex.reset(); // a fresh popup starts unscrolled
		g_dropdown.visibleRow.reset();
		g_dropdown.scrollOffset = 0;
	}
	g_popupWasActive = popupActive;
	if (popupActive) {
		if (const Generated::PanelWidgets* popup = Catalog(DropdownPopupPanel); popup != nullptr) {
			AnnouncePopupRow(context, *popup, *indicator);
		}
		return;
	}

	Match match = Resolve(context, *indicator, false);
	if (match.widget == nullptr || match.score < MinimumCoverage) {
		// Cached rects may be stale (a list scrolled, a row appeared): re-resolve once.
		g_rectCache.clear();
		match = Resolve(context, *indicator, false);
	}
	if (match.widget == nullptr || match.score < MinimumCoverage) {
		D2RL::LogInfoF(context, "[focus] indicator at %d,%d %dx%d matched nothing (best %.2f); screen root %s", indicator->x, indicator->y, indicator->width, indicator->height, match.score,
			Describe(Widgets::PanelRect(g_focusManagerName != nullptr ? g_focusManagerName : FocusManagerPanelNames[0])).c_str());
		match = Resolve(context, *indicator, true);
		return;
	}
	Announce(context, match, *indicator);
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
				g_pendingValue = { catalog, &widget, ControlState::Index(widget, g_lastAnnounced) };
			}
			return true;
		}
	}
	return false;
}

void InvalidateRects() {
	g_rectCache.clear();
	g_forceResolve = true;
}

void PanelsChanged() {
	Widgets::ForgetHandles();
	InvalidateRects();
}

void NoteScrollInput() {
	++g_scrollInputs;
	InvalidateRects();
}

std::string_view FocusedCharacter() {
	return g_focusedCharacter;
}

void Reset() {
	g_focusedCharacter.clear();
	g_scrollInputs = 0;
	g_lastIndicator.reset();
	g_lastAnnounced.clear();
	g_lastMatchedPanel = nullptr;
	InvalidateRects();
}

}
