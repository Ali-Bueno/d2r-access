#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

// Every phrase the plugin speaks lives in one Table per game language, so
// wording and translation are changed in one place, never inside the services
// or the hooks. Game text (labels, tooltips, panel titles, item names) comes
// from the game's own string tables; these are only the plugin's connective
// words. The per-language tables live in src/strings/<locale>.h.
namespace D2RAccess::Strings {

// The game's thirteen text locales (the per-locale columns of
// data/local/lng/strings/*.json), sorted by code. Settings.json stores the
// locale by its ui.json column index instead: see control_state.cpp.
enum class Language { enUS, deDE, esES, esMX, frFR, itIT, jaJP, koKR, plPL, ptBR, ruRU, zhCN, zhTW };

inline constexpr std::size_t LanguageCount = 13;

inline constexpr std::array<std::string_view, LanguageCount> LocaleCodes{
	"enUS", "deDE", "esES", "esMX", "frFR", "itIT", "jaJP", "koKR", "plPL", "ptBR", "ruRU", "zhCN", "zhTW"};

struct Table {
	const char* pluginLoaded;
	const char* screenReaderMissing;
	const char* panelOpened;   // %s = panel title
	const char* panelClosed;   // %s = panel title
	const char* panelToggled;  // %s = panel title
	const char* navigation;    // %s = destination screen
	const char* valueChanged;  // %s = label, %s = value
	const char* roleButton;
	const char* roleCheckbox;
	const char* roleDropdown;
	const char* roleSlider;
	const char* roleTextField;
	const char* roleTabs;
	const char* roleList;
	const char* checked;
	const char* unchecked;
	const char* optionIndex;       // %d = index (1-based), count unknown
	const char* resolution;        // %u = width, %u = height
	const char* characterLevel;    // %d = level
	const char* unavailable;       // spoken after a control the game has disabled
	const char* modalConfirmHint;  // spoken after a confirmation modal's title
	const char* modalDismissHint;  // spoken after a dismissable modal's title
	const char* dialogueMenu;
	const char* gameTextEnabled;
	const char* gameTextDisabled;
	const char* inventory;
	const char* stash;
	const char* equipment;
	const char* slotEmpty;
	const char* slotOccupied;
	const char* itemInHand;
	const char* itemReleased;
	const char* belt;
	const char* gridCell; // one-based row, column
	const char* detailNone;   // right stick up with nothing further to read
	const char* assignedTo; // %s = native controller binding
	std::array<const char*, 19> controllerButtons; // U+E000..U+E012, native 0x872E50
	// InventorySlotWidget location values, verified against live slots and the stock layout.
	std::array<const char*, 11> equipmentSlots;
	const char* radarEnabled;
	const char* footstepsEnabled;
	const char* footstepsDisabled;
	const char* footstepsUnavailable;
	const char* radarDisabled;
	const char* radarUnavailable;
	const char* radarReaderUnavailable;
	const char* radarWaitingPlayer;
	std::array<const char*, 4> radarUiStates; // inactive, unavailable, dialogue, paused
	const char* radarAudioUnavailable;
	const char* radarTest;
	const char* radarDepth; // %.0f = clear depth in collision subtiles
	const char* radarNearbyObstacle;
	const char* radarLimited;
	std::array<const char*, 4> radarDirections; // Radar::Direction order: up, left, right, down
	std::array<const char*, 3> radarStates;
	const char* radarMoving; // %s = actual recent screen displacement direction
	const char* radarStationary;
	const char* radarMotionUnknown;
	const char* radarStraight;
	std::array<const char*, 8> radarMovementDirections; // clockwise from screen up
	const char* scanEnabled;
	const char* scanDisabled;
	const char* pathfinderEnabled;  // L3 toggles the long-range pathfinder
	const char* pathfinderDisabled;
	const char* scanUnavailable;
	std::array<const char*, 4> scanDirections; // Radar::Direction order: up, left, right, down
	const char* scanNothing; // %s = scan direction
	const char* scanStep; // exactly one step
	const char* scanSteps; // %d = whole steps
	const char* scanAdjacent; // under one step away
	const char* npcEnabled; // friendly NPC announcer toggles
	const char* npcDisabled;
	// Map survey: the level-wide catalogue, cycled by category and by object.
	// Indexed by the switch in MapSurveyService::CategoryName, not by
	// CategoryOrder: a new category appends here and adds a case there.
	std::array<const char*, 11> surveyCategories;
	const char* surveySteps;    // %s = name, %s = direction, %d = steps on foot
	const char* surveyNoRoute;  // %s = name, %s = direction; known but unreachable
	const char* surveyBearing;  // %s = name, %s = direction; a bearing is all that is known
	const char* surveyRoom;             // %d = room number
	const char* surveyRoomUnexplored;   // %d = room number, never entered yet
	const char* surveyEmpty;    // %s = category
	const char* surveyNothing;  // nothing catalogued on this level yet
	const char* exitUnnamed; // a level exit whose destination name is unavailable
	const char* itemSpeechEnabled; // nearby ground item reading toggles
	const char* itemSpeechDisabled;
	const char* itemSoundsEnabled; // ground item loop toggles
	const char* itemSoundsDisabled;
	const char* exitSoundsEnabled; // level exit loop toggles
	const char* exitSoundsDisabled;
	const char* beaconCleared;
	const char* beaconUnavailable;
	const char* beaconNothing;
	const char* beaconFollowing; // %s = target name
	const char* beaconArrived; // %s = target name
	const char* beaconLost; // %s = target name
	const char* questUpdated;
	const char* questGoalDone; // PrimaryGoalDone; reward collection may still be pending.
	const char* questAlertsEnabled;
	const char* questAlertsDisabled;
	const char* questNoObjective;
	const char* keyboardPadEnabled;  // the simulated controller, toggled live
	const char* keyboardPadDisabled;
	const char* vitalsLife;          // %d = whole percent of life left
	const char* vitalsMana;          // %d = whole percent of mana left
	const char* vitalsEnabled;
	const char* vitalsDisabled;
	const char* vitalsOrbTextMissing; // %s, %s = the game's own orb text option labels
	const char* equipmentBroken;      // %s = equipment slot of the item that broke
	const char* currentOfMaximum;     // %s = current value, %s = maximum
	const char* itemPlacedIn;         // %s = where the carried item was just placed
	// Item quality suffixes indexed by the game's item quality value
	// (D2RL::ItemQuality: 0 none, 1 low, 2 normal, 3 superior, 4 magic, 5 set,
	// 6 rare, 7 unique, 8 crafted); empty entries are never spoken.
	std::array<const char*, 9> itemQualities;
};

// Visits every phrase of a table, single fields and array entries alike, in
// declaration order. The one place that enumerates the fields: the completeness
// guard below and the tests that compare translations against English use it.
template<class Visit> constexpr void ForEachPhrase(const Table& t, Visit visit) {
	const char* const singles[] = {
		t.pluginLoaded, t.screenReaderMissing, t.panelOpened, t.panelClosed, t.panelToggled, t.navigation,
		t.valueChanged, t.roleButton, t.roleCheckbox, t.roleDropdown, t.roleSlider, t.roleTextField, t.roleTabs,
		t.roleList, t.checked, t.unchecked, t.optionIndex, t.resolution, t.characterLevel,
		t.unavailable, t.modalConfirmHint, t.modalDismissHint, t.dialogueMenu, t.gameTextEnabled,
		t.gameTextDisabled, t.inventory, t.stash, t.equipment, t.slotEmpty, t.slotOccupied, t.itemInHand,
		t.itemReleased, t.belt, t.gridCell, t.detailNone, t.assignedTo, t.radarEnabled, t.footstepsEnabled,
		t.footstepsDisabled, t.footstepsUnavailable, t.radarDisabled, t.radarUnavailable,
		t.radarReaderUnavailable, t.radarWaitingPlayer, t.radarAudioUnavailable, t.radarTest, t.radarDepth,
		t.radarNearbyObstacle, t.radarLimited, t.radarMoving, t.radarStationary, t.radarMotionUnknown,
		t.radarStraight, t.scanEnabled, t.scanDisabled, t.pathfinderEnabled, t.pathfinderDisabled, t.scanUnavailable, t.scanNothing, t.scanStep,
		t.scanSteps, t.scanAdjacent, t.npcEnabled, t.npcDisabled, t.exitUnnamed,
		t.itemSpeechEnabled, t.itemSpeechDisabled, t.itemSoundsEnabled, t.itemSoundsDisabled,
		t.exitSoundsEnabled, t.exitSoundsDisabled, t.beaconCleared, t.beaconUnavailable,
		t.beaconNothing, t.beaconFollowing, t.beaconArrived, t.beaconLost,
		t.questUpdated, t.questGoalDone, t.questAlertsEnabled, t.questAlertsDisabled, t.questNoObjective,
		t.keyboardPadEnabled, t.keyboardPadDisabled,
		t.vitalsLife, t.vitalsMana, t.vitalsEnabled, t.vitalsDisabled, t.vitalsOrbTextMissing,
		t.equipmentBroken, t.currentOfMaximum, t.itemPlacedIn};
	for (const char* s : singles) visit(s);
	const auto each = [&](const auto& list) { for (const char* s : list) visit(s); };
	each(t.controllerButtons); each(t.equipmentSlots); each(t.radarUiStates); each(t.radarDirections);
	each(t.radarStates); each(t.radarMovementDirections); each(t.scanDirections); each(t.itemQualities);
}

// Compile-time guard: a language table that leaves a field out of its
// designated initializer would hand a null format string to the speech layer.
constexpr bool Complete(const Table& t) {
	bool complete = true;
	ForEachPhrase(t, [&](const char* s) { if (s == nullptr) complete = false; });
	return complete;
}

// The table of the language chosen from the game's text language (see
// Localization::DetectSpeechLanguage); English until detection succeeds.
const Table& Current();
Language     Selected();
void         Select(Language language);
const Table& For(Language language);

std::optional<Language> FromLocale(std::string_view code);
std::string_view        LocaleCode(Language language);

// The game exposes no locale API, so the text language is recognised by
// resolving one game string whose translation differs in every locale and
// matching it against these values (from data/local/lng/strings/ui.json).
struct LocaleProbe {
	std::string_view                                key;
	std::array<std::string_view, LanguageCount>     values; // Language order
};
extern const LocaleProbe TextLocaleProbe;

// The language whose probe value matches, if any.
std::optional<Language> FromProbeValue(std::string_view value);

}
