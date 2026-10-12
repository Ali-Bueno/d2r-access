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
inline constexpr std::size_t PresetPlaceWords = 13, PlaceInteriorWords = 11, PlaceFeatureWords = 24;



inline constexpr std::array<std::string_view, LanguageCount> LocaleCodes{
	"enUS", "deDE", "esES", "esMX", "frFR", "itIT", "jaJP", "koKR", "plPL", "ptBR", "ruRU", "zhCN", "zhTW"};

// Where in the level the player stands: three screen thirds per axis.
inline constexpr std::size_t TerrainPlaceCount = 9;
// The sound glossary's entries, in the order sound_glossary.cpp plays them.
inline constexpr std::size_t GlossarySoundCount = 49;
inline constexpr std::size_t ManaAlertWords = 3; // Vitals::ManaAlertLevels

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
	const char* belt;
	const char* beltRow; // %d = one-based row
	const char* beltColumnCount; // %d = physical items across all rows of this column
	const char* beltAutomatic;
	const char* gridCell; // one-based row, column
	// The cells an item takes up, %d = columns then %d = rows, by shape: one by
	// one, one column by several rows, several columns by one row, several by
	// several. "Several" is only ever 2 to 4 -- no item is wider than 2 or
	// taller than 4 (src/generated/item_sizes.h) -- which is what lets the
	// Polish and Russian tables use a single plural.
	std::array<const char*, 4> itemSize;
	const char* detailNone;   // right stick up with nothing further to read
	const char* assignedTo; // %s = native controller binding
	std::array<const char*, 19> controllerButtons; // U+E000..U+E012, native 0x872E50
	// InventorySlotWidget location values, verified against live slots and the stock layout.
	std::array<const char*, 11> equipmentSlots;
	const char* radarEnabled;
	const char* footstepsEnabled;
	const char* footstepsDisabled;
	const char* footstepsUnavailable;
	const char* movementEnabled;
	const char* movementDisabled;
	const char* movementUnavailable;
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
	std::array<const char*, 5> radarStates; // Radar::State order: unknown, open, blocked, exit, door
	const char* radarMoving; // %s = actual recent screen displacement direction
	const char* radarStationary;
	const char* radarMotionUnknown;
	const char* radarStraight;
	const char* wallBumpEnabled;
	const char* wallBumpDisabled;
	const char* smallObstaclesEnabled;
	const char* smallObstaclesDisabled;
	std::array<const char*, 8> radarMovementDirections; // clockwise from screen up
	const char* navigationExploration; // mode status: guidance uses explored areas
	const char* navigationFullAssistance; // mode status: guidance includes unexplored areas
	std::array<const char*, 4> scanDirections; // Radar::Direction order: up, left, right, down
	const char* scanStep; // exactly one step
	const char* scanSteps; // %d = whole steps
	const char* scanAdjacent; // under one step away
	const char* npcEnabled; // friendly NPC announcer toggles
	const char* npcDisabled;
	const char* entityCuesEnabled;
	const char* entityCuesDisabled;
	const char* entityCueNpcsEnabled;
	const char* entityCueNpcsDisabled;
	const char* entityCueEnemiesEnabled;
	const char* entityCueEnemiesDisabled;
	const char* entityCueRangedEnabled;
	const char* entityCueRangedDisabled;
	const char* entityCueAudioUnavailable;
	const char* entityCueReaderUnavailable;
	// Map survey: the level-wide catalogue, cycled by category and by object.
	// Indexed by the switch in MapSurveyService::CategoryName, not by
	// CategoryOrder: a new category appends here and adds a case there.
	std::array<const char*, 15> surveyCategories;
	const char* surveySteps;    // %s = name, %s = distance, %s = direction
	const char* surveyBearing;  // %s = name, %s = direction; only when there is no distance at all
	const char* surveyRoom;             // %d = room number
	const char* surveyRoomUnexplored;   // %d = room number, never entered yet
	const char* corpseOwn;              // the local player's corpse
	const char* corpseOf;               // %s = another player's name
	const char* surveyEmpty;    // %s = category
	const char* surveyNothing;  // nothing catalogued on this level yet
	const char* exitUnnamed; // a level exit whose destination name is unavailable
	const char* containerUnnamed; // a loot container whose localized name is unavailable
	const char* monsterUnique; // a unique or super unique monster, which the game marks by its name's gold alone
	const char* itemSpeechEnabled; // nearby ground item reading toggles
	const char* itemSpeechDisabled;
	const char* itemSoundsEnabled; // ground item loop toggles
	const char* itemSoundsDisabled;
	const char* exitSoundsEnabled; // level exit loop toggles
	const char* exitSoundsDisabled;
	const char* chestSoundsEnabled; // container loop toggles
	const char* chestSoundsDisabled;
	const char* doorSoundsEnabled; // shut door loop toggles
	const char* doorSoundsDisabled;
	const char* questSoundsEnabled; // quest object loop toggles (Cairn Stones, the Inifuss tree, tomes)
	const char* questSoundsDisabled;
	const char* stashSoundEnabled; // the stash's loop toggle
	const char* stashSoundDisabled;
	const char* beaconCleared;
	const char* beaconUnavailable;
	const char* beaconNothing;
	const char* beaconFollowing; // %s = target name
	const char* beaconArrived; // %s = target name
	const char* beaconLost; // %s = target name
	const char* beaconKnownRouteOnly; // %s = target name; no explored-area route
	const char* beaconKnownRouteEnded; // %s = target name; known route no longer available
	const char* placeFoundExit; // %s = an exit the scanner lists for the first time on this map
	const char* placeFound;     // %s = a waypoint or stash the scanner lists for the first time
	const char* placeFoundQuest; // %s = a quest object the scanner lists for the first time
	const char* questUpdated;
	const char* questGoalDone; // PrimaryGoalDone; reward collection may still be pending.
	const char* questAlertsEnabled;
	const char* questAlertsDisabled;
	const char* questNoObjective;
	const char* cairnStoneSymbol; // %s = native object name, %s = fixed scroll symbol
	const char* cairnScrollOrder; // %s = spoken symbol sequence, assembled when scroll opens
	std::array<const char*, 5> cairnSymbols; // scroll Symbol1..5: DropUp, I, DropDown, II, Zero
	std::array<const char*, 7> tombSymbols; // TalRashaSymbol frames 0..6, shared with tomb entrance names
	const char* sealNumbered; // %s = native seal name, %d = its number (Generated::SealOrdinal)
	const char* sealsLeft; // %d = Chaos Sanctuary seals not yet open
	const char* sealsAllOpen; // every Chaos Sanctuary seal is open
	const char* vitalsLife;          // %d = whole percent of life left
	const char* vitalsMana;          // %d = whole percent of mana left
	const char* vitalsEnabled;
	const char* vitalsDisabled;
	const char* vitalsOrbTextMissing; // %s, %s = the game's own orb text option labels
	const char* vitalsOrbTextMissingOne; // %s = missing orb option label
	const char* playerPoisoned;
	const char* playerPoisonCleared;
	const char* mercenaryLife; // %d = current hireling life percentage
	const char* equipmentBroken;      // %s = equipment slot of the item that broke
	const char* currentOfMaximum;     // %s = current value, %s = maximum
	const char* equipRefused;         // the game kept the item in hand
	const char* equipRefusedBecause;  // %s = the requirements drawn red
	const char* goldCarried;          // %s = how much gold the player is carrying
	const char* goldStashed;          // %s = how much gold the open stash tab holds
	const char* handsEmpty;           // a weapon swap to empty hands
	// Player-to-player trade (docs/trade.md). %s = the partner's name throughout.
	const char* tradeRequested;       // %s asked this player to trade
	const char* tradeAsking;          // this player asked %s
	const char* tradeWith;            // the trade window's title
	const char* tradeYourOffer;       // the lower grid
	const char* tradeTheirOffer;      // the upper grid
	const char* tradePartnerGold;     // %s = partner, %s = gold offered
	const char* tradePartnerItems;    // %s = partner: their offered items changed (no count)
	const char* tradePartnerAccepted;
	const char* tradePartnerWithdrew;
	const char* tradeWeWithdrew;
	const char* tradeCompleted;
	const char* tradeCancelled;
	const char* tradeEnded;           // no completion signal to tell completed from cancelled
	const char* tradePartnerUnknown;  // stands in for a name the roster does not give
	const char* autoDetailEnabled;    // autodetail: the whole description on every highlight
	const char* autoDetailDisabled;
	const char* controlTooltipsEnabled;  // Ctrl+H: the controls among what autodetail reads
	const char* controlTooltipsDisabled;
	const char* listSeparator; // joins the items of a spoken list
	// Terrain overview: how far the level reaches around the player and how
	// much of it is still unwalked. `docs/terrain-overview.md`
	const char* terrainOverview;       // %s = level name, %s = where in it, %s = the four directions
	const char* terrainSeparator;      // joins the four direction phrases
	const char* terrainDirection;      // %s = direction, %s = how far it reaches, %s = "" or the separator plus the state
	const char* terrainEdge;           // %s = distance to the level's outer edge
	const char* terrainBorder;         // %s = the level across the edge, %s = distance to it
	const char* terrainExplored;
	const char* terrainUnexplored;
	const char* terrainUnexploredFrom; // %s = distance where the unexplored part starts
	const char* terrainPercent;        // %d = share of the level's rooms seen so far
	const char* terrainUnavailable;    // the level's layout could not be read
	// Room announcer: the ways out of a dungeon room. `room_service.h`
	const char* roomExits;     // %s = the ways out, already joined
	const char* roomExitNamed; // %s = a level exit's own name, %s = its direction
	const char* roomNoExits;
	const char* roomOpening;   // a plain gap between two streamed rooms, with no name of its own
	const char* wayUnexplored; // %s = a way out (direction, passage or exit name) never taken yet
	const char* roomOutside;   // BACK+Y where there is no dungeon room to describe
	const char* roomsEnabled;
	const char* roomsDisabled;
	const char* roomsUnavailable;
	const char* finderEnabled;  // the exit finder, BACK+A: ways out by side on the right stick
	const char* finderDisabled;
	const char* finderNothing;  // %s = a side (scanDirections): no way out on that side
	const char* automapLogged;         // the automap diagnostic finished; details are in the log
	const char* automapWatchOn;        // the automap watch logs every change from now on
	const char* automapWatchOff;
	const char* skillBarSlot;          // %s = controller button, %s = skill name and HUD count
	// Keyboard play: walking, exploring, retreating and fighting. `keyboard_service.h`
	const char* walkTo; // %s = what the character sets out for
	const char* walkStopped; // a walk, a retreat or a fight ended by the player
	const char* walkBlocked; // %s = the target; the character stopped making headway
	const char* walkNoRoute; // %s = the target
	const char* walkNothing; // the scanner has named nothing to walk to
	const char* exploreTo;
	const char* exploreArrived;
	const char* exploreNothing; // every room that can be reached is explored
	const char* retreatTo; // %s = the way the player came into the level, by name, or retreatEntrance
	const char* retreatEntrance; // where the player first stood in the level, when nothing they came through stands there
	const char* targetNone; // no monster to fight or step to
	const char* targetOutOfReach; // %s = a monster with no way to it
	const char* interactNothing;
	const char* actionsUnavailable; // the game's command could not be verified on this build
	// The keyboard's settings list, and the switches that had no phrase of their own. `settings_list.h`
	const char* settingsOpened;
	const char* settingsClosed;
	const char* skillBarReaderOn;
	const char* skillBarReaderOff;
	const char* tradeAlertsOn;
	const char* tradeAlertsOff;
	const char* weaponSwapAlertsOn;
	const char* weaponSwapAlertsOff;
	const char* handSpeechOn;  // the pick-up and placement speech switch
	const char* handSpeechOff;
	const char* skillGridOn;
	const char* skillGridOff;
	const char* explorationSoundsOn;
	const char* explorationSoundsOff;
	const char* discoveriesOn;
	const char* discoveriesOff;
	const char* beaconSoundsOn; // the beacon's pings and its lock, release and arrival tones
	const char* beaconSoundsOff;
	const char* modSoundsOn; // one switch over every sound the mod makes (mod_sounds.h)
	const char* modSoundsOff;
	const char* reachFilterOn; // the scanner lists only what can be got to on foot (map_survey_service.h)
	const char* reachFilterOff;
	const char* teleported; // a teleport pad D used has sent the player (keyboard_service.cpp)
	const char* weaponSet; // %d = the weapon set now in use, 1 or 2
	const char* weaponSetNumberOn;
	const char* weaponSetNumberOff;
	const char* gaitWalking; // the game now walks the character
	const char* gaitRunning; // the game now runs the character
	const char* staminaPercent; // %d = whole percent of stamina left
	const char* staminaOut;
	const char* movementAlertsOn;
	const char* movementAlertsOff;
	const char* ammoLeft; // %s = a stack in hand (arrows, javelins), %d = how many it holds
	const char* ammoAlertsOn;
	const char* ammoAlertsOff;
	const char* townNpcsEverywhereOn;
	const char* townNpcsEverywhereOff;
	const char* pickupClickOn;
	const char* pickupClickOff;
	const char* gridEdgeOn;
	const char* gridEdgeOff;
	const char* silverItem; // the colour word for a white-name item drawn gray
	const char* silverItemsOn;
	const char* silverItemsOff;
	const char* glossarySound; // %s = what a sound of the glossary means
	const char* faultMemory; // the UI update ran out of memory; the mod stopped its own movement
	const char* faultAction; // the UI update failed; the mod stopped its own movement
	std::array<const char*, GlossarySoundCount> glossarySounds;
	// Item quality suffixes indexed by the game's item quality value
	// (D2RL::ItemQuality: 0 none, 1 low, 2 normal, 3 superior, 4 magic, 5 set,
	// 6 rare, 7 unique, 8 crafted); empty entries are never spoken.
	std::array<const char*, 9> itemQualities;
	// Screen thirds, behind row first: behind-left, behind, behind-right,
	// left, centre, right, ahead-left, ahead, ahead-right.
	std::array<const char*, TerrainPlaceCount> terrainPlaces;
	const char* itemInHand;
	const char* itemReleased;
	const char* entityCueOrdinaryEnabled;
	const char* entityCueOrdinaryDisabled;
	const char* entityCueReachEnabled;
	const char* entityCueReachDisabled;
	const char* attackReachMelee;  // %s = spoken distance ("3 pasos")
	const char* attackReachRanged; // %s = spoken distance
	const char* attackModeAuto;    // the reach follows the bound skills and the weapon
	const char* attackModeFixed;   // the reach was set by a command or the config
	const char* deathPlace;             // where the player last died, on that level
	const char* deathRoute;             // %s = level died on, %s = the exit toward it
	const char* deathRouteWaypoint;     // %s = level died on, %s = waypoint, %s = level to pick
	std::array<const char*, PresetPlaceWords> placePresets; // the first, PresetPlace::None, is never spoken
	std::array<const char*, PlaceInteriorWords> placeInteriors;
	std::array<const char*, PlaceFeatureWords> placeFeatures;
	const char* placeOutside; // left an interior for open ground
	const char* placeCorridor;
	const char* placeDeadEnd; // a corridor shut at one end
	const char* placeBend;
	const char* placeJunction;
	const char* placeTowards; // %s = what a corridor leads to: a level exit or a landmark
	const char* placesEnabled;
	const char* placesDisabled;
	const char* placesUnavailable;
	const char* bossLife; // %s = the boss's name, %d = whole percent of its life left
	const char* bossLifeEnabled;
	const char* bossLifeDisabled;
	const char* itemPlacedIn;         // %s = where the carried item was just placed
	const char* itemSocketedIn;       // %s = item the carried gem, rune or jewel went into
	const char* itemAddedTo;          // %s = stack or tome the carried item joined
	const char* itemSwapped;          // %s = item the game handed back in exchange
	const char* itemDropped;          // the drop button threw the carried item on the ground
	const char* socketsNone;          // the item under a gem in hand has no sockets
	const char* socketsCount;         // %d = how many sockets the item under a gem in hand has
	const char* socketsFree;          // %d = how many of them are empty
	const char* socketsFull;          // ... every one filled
	const char* socketRefused;        // the game kept the gem out of the item
	const char* itemArrived;          // %s = item, %s = where it landed (bought, handed over)
	const char* itemUnequippedTo;     // %s = item, %s = where the taken-off item landed
	const char* spaceKey;             // what a typed space is called when echoed
	const char* statPointsFocusOn;
	const char* statPointsFocusOff;
	const char* screenHintsOn;
	const char* screenHintsOff;
	const char* hintNewCharacter;     // %s = the button that creates a character
	const char* hintRandomName;       // %s = the button that picks a random name
	const char* developmentBuild;     // said after "loaded" by a local build, where a release says its tag
	const char* shop;                 // a vendor's window when its vendor cannot be named
	const char* goldPickedUp;         // %d = the gold a pile brought in
	const char* itemMovedToStash;     // %s = item, %s = the stash tab Y sorted it into
	const char* stashGems;
	const char* stashRunes;
	const char* stashMaterials;
	const char* mercenarySkills;      // %s = the mercenary's skills, as a list
	const char* lastEpochSoundsOn;
	const char* lastEpochSoundsOff;
	const char* enemyPanStrength;     // %d = percent of the compiled stereo spread
	const char* enemyPitchStrength;   // %d = percent of the compiled rear pitch drop
	const char* beaconPanStrength;
	const char* beaconPitchStrength;
	const char* backToModOn;
	const char* backToModOff;
	const char* shrineSoundsOn;
	const char* shrineSoundsOff;
	const char* wallTonesOn;
	const char* wallTonesOff;
	std::array<const char*, ManaAlertWords> manaAlerts; // every change, low mana only, off
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
		t.gameTextDisabled, t.inventory, t.stash, t.equipment, t.slotEmpty, t.slotOccupied,
		t.belt, t.beltRow, t.beltColumnCount, t.beltAutomatic, t.gridCell, t.detailNone, t.assignedTo, t.radarEnabled, t.footstepsEnabled,
		t.footstepsDisabled, t.footstepsUnavailable, t.movementEnabled, t.movementDisabled, t.movementUnavailable,
		t.radarDisabled, t.radarUnavailable,
		t.radarReaderUnavailable, t.radarWaitingPlayer, t.radarAudioUnavailable, t.radarTest, t.radarDepth,
		t.radarNearbyObstacle, t.radarLimited, t.radarMoving, t.radarStationary, t.radarMotionUnknown,
		t.radarStraight,
		t.wallBumpEnabled, t.wallBumpDisabled, t.smallObstaclesEnabled, t.smallObstaclesDisabled,
		t.navigationExploration, t.navigationFullAssistance, t.scanStep,
		t.scanSteps, t.scanAdjacent, t.npcEnabled, t.npcDisabled,
		t.entityCuesEnabled, t.entityCuesDisabled, t.entityCueNpcsEnabled, t.entityCueNpcsDisabled,
		t.entityCueEnemiesEnabled, t.entityCueEnemiesDisabled, t.entityCueRangedEnabled,
		t.entityCueRangedDisabled, t.entityCueAudioUnavailable, t.entityCueReaderUnavailable,
		t.exitUnnamed, t.containerUnnamed, t.monsterUnique,
		t.itemSpeechEnabled, t.itemSpeechDisabled, t.itemSoundsEnabled, t.itemSoundsDisabled,
		t.exitSoundsEnabled, t.exitSoundsDisabled, t.chestSoundsEnabled, t.chestSoundsDisabled,
		t.doorSoundsEnabled, t.doorSoundsDisabled, t.questSoundsEnabled, t.questSoundsDisabled,
		t.stashSoundEnabled, t.stashSoundDisabled,
		t.beaconCleared, t.beaconUnavailable,
		t.beaconNothing, t.beaconFollowing, t.beaconArrived, t.beaconLost,
		t.beaconKnownRouteOnly, t.beaconKnownRouteEnded, t.corpseOwn, t.corpseOf,
		t.questUpdated, t.questGoalDone, t.questAlertsEnabled, t.questAlertsDisabled, t.questNoObjective,
		t.cairnStoneSymbol, t.cairnScrollOrder, t.sealNumbered, t.sealsLeft, t.sealsAllOpen,
		t.vitalsLife, t.vitalsMana, t.vitalsEnabled, t.vitalsDisabled, t.vitalsOrbTextMissing,
		t.vitalsOrbTextMissingOne, t.playerPoisoned, t.playerPoisonCleared, t.mercenaryLife, t.equipmentBroken, t.currentOfMaximum, t.equipRefused, t.equipRefusedBecause, t.goldCarried, t.goldStashed, t.handsEmpty, t.tradeRequested, t.tradeAsking, t.tradeWith, t.tradeYourOffer, t.tradeTheirOffer, t.tradePartnerGold, t.tradePartnerItems, t.tradePartnerAccepted, t.tradePartnerWithdrew, t.tradeWeWithdrew, t.tradeCompleted, t.tradeCancelled, t.tradeEnded, t.tradePartnerUnknown, t.autoDetailEnabled, t.autoDetailDisabled, t.controlTooltipsEnabled, t.controlTooltipsDisabled, t.listSeparator,
		t.terrainOverview, t.terrainSeparator, t.terrainDirection, t.terrainEdge, t.terrainBorder,
		t.terrainExplored, t.terrainUnexplored, t.terrainUnexploredFrom, t.terrainPercent, t.terrainUnavailable,
		t.roomExits, t.roomExitNamed, t.roomNoExits, t.roomOpening, t.wayUnexplored, t.placeFoundExit, t.placeFound, t.placeFoundQuest, t.roomOutside, t.roomsEnabled, t.roomsDisabled, t.roomsUnavailable, t.finderEnabled, t.finderDisabled, t.finderNothing,
		t.automapLogged, t.automapWatchOn, t.automapWatchOff, t.skillBarSlot,
		t.walkTo, t.walkStopped, t.walkBlocked, t.walkNoRoute, t.walkNothing, t.exploreTo, t.exploreArrived, t.exploreNothing,
		t.retreatTo, t.retreatEntrance, t.targetNone, t.targetOutOfReach, t.interactNothing, t.actionsUnavailable,
		t.settingsOpened, t.settingsClosed, t.skillBarReaderOn, t.skillBarReaderOff, t.tradeAlertsOn, t.tradeAlertsOff, t.weaponSwapAlertsOn, t.weaponSwapAlertsOff, t.handSpeechOn, t.handSpeechOff,
		t.skillGridOn, t.skillGridOff, t.explorationSoundsOn, t.explorationSoundsOff, t.discoveriesOn, t.discoveriesOff,
		t.beaconSoundsOn, t.beaconSoundsOff, t.modSoundsOn, t.modSoundsOff, t.reachFilterOn, t.reachFilterOff, t.teleported, t.weaponSet, t.weaponSetNumberOn, t.weaponSetNumberOff, t.gaitWalking, t.gaitRunning, t.staminaPercent, t.staminaOut, t.movementAlertsOn, t.movementAlertsOff, t.ammoLeft, t.ammoAlertsOn, t.ammoAlertsOff, t.townNpcsEverywhereOn, t.townNpcsEverywhereOff, t.pickupClickOn, t.pickupClickOff, t.gridEdgeOn, t.gridEdgeOff, t.silverItem, t.silverItemsOn, t.silverItemsOff, t.glossarySound, t.faultMemory, t.faultAction};
	for (const char* s : singles) visit(s);
	const auto each = [&](const auto& list) { for (const char* s : list) visit(s); };
	each(t.itemSize); each(t.controllerButtons); each(t.equipmentSlots); each(t.radarUiStates); each(t.radarDirections);
	each(t.radarStates); each(t.radarMovementDirections); each(t.scanDirections); each(t.itemQualities); each(t.terrainPlaces); each(t.cairnSymbols); each(t.tombSymbols);
	for (const char* s : {t.itemInHand, t.itemReleased, t.entityCueOrdinaryEnabled, t.entityCueOrdinaryDisabled, t.entityCueReachEnabled, t.entityCueReachDisabled, t.attackReachMelee, t.attackReachRanged, t.attackModeAuto, t.attackModeFixed, t.deathPlace, t.deathRoute, t.deathRouteWaypoint, t.placeOutside, t.placeCorridor, t.placeDeadEnd, t.placeBend, t.placeJunction, t.placeTowards, t.placesEnabled, t.placesDisabled, t.placesUnavailable, t.bossLife, t.bossLifeEnabled, t.bossLifeDisabled, t.itemPlacedIn, t.itemSocketedIn, t.itemAddedTo, t.itemSwapped, t.itemDropped, t.socketsNone, t.socketsCount, t.socketsFree, t.socketsFull, t.socketRefused, t.itemArrived, t.itemUnequippedTo, t.spaceKey, t.statPointsFocusOn, t.statPointsFocusOff, t.screenHintsOn, t.screenHintsOff, t.hintNewCharacter, t.hintRandomName,
		t.developmentBuild, t.shop, t.goldPickedUp, t.itemMovedToStash, t.stashGems, t.stashRunes, t.stashMaterials, t.mercenarySkills,
		t.lastEpochSoundsOn, t.lastEpochSoundsOff, t.enemyPanStrength, t.enemyPitchStrength, t.beaconPanStrength, t.beaconPitchStrength,
		t.backToModOn, t.backToModOff, t.shrineSoundsOn, t.shrineSoundsOff, t.wallTonesOn, t.wallTonesOff}) visit(s);
	each(t.manaAlerts);
	each(t.placePresets);
	each(t.placeInteriors);
	each(t.placeFeatures);
	each(t.glossarySounds);

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
