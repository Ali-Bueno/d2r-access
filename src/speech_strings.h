#pragma once

#include <string_view>
#include <array>

// Every phrase the plugin speaks lives here so wording and translation are
// changed in one place, never inside the services or the hooks. Game text
// (labels, tooltips, panel titles) comes from the game's own string tables;
// these are only the plugin's connective words.
namespace D2RAccess::Strings {

enum class Language { English, Spanish };

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
	const char* optionOfCount;     // %d = index (1-based), %d = count
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
	const char* belt;
	const char* gridCell; // one-based row, column
	// InventorySlotWidget location values, verified against live slots and the stock layout.
	std::array<const char*, 11> equipmentSlots;
	const char* radarEnabled;
	const char* radarDisabled;
	const char* radarUnavailable;
	const char* radarWaitingHeading;
	const char* radarAudioUnavailable;
	const char* radarTest;
	const char* radarDepth; // %.0f = clear depth in collision subtiles
	std::array<const char*, 4> radarDirections;
	std::array<const char*, 3> radarStates;
};

inline constexpr Table English {
	.pluginLoaded        = "Diablo 2 accessibility loaded",
	.screenReaderMissing = "screen reader unavailable",
	.panelOpened         = "%s",
	.panelClosed         = "%s closed",
	.panelToggled        = "%s",
	.navigation          = "going to %s",
	.valueChanged        = "%s: %s",
	.roleButton          = "button",
	.roleCheckbox        = "checkbox",
	.roleDropdown        = "dropdown",
	.roleSlider          = "slider",
	.roleTextField       = "text field",
	.roleTabs            = "tabs",
	.roleList            = "list",
	.checked             = "checked",
	.unchecked           = "unchecked",
	.optionOfCount       = "option %d of %d",
	.optionIndex         = "option %d",
	.resolution          = "%u by %u",
	.characterLevel      = "level %d",
	.unavailable         = "unavailable",
	.modalConfirmHint    = "A to confirm, B to cancel",
	.modalDismissHint    = "A to close",
	.dialogueMenu        = "Conversation",
	.gameTextEnabled     = "In-game text reading on",
	.gameTextDisabled    = "In-game text reading off",
	.inventory           = "Inventory",
	.stash               = "Stash",
	.equipment           = "Equipment",
	.belt                = "Belt",
	.gridCell            = "Row %d, column %d",
	.equipmentSlots      = { "", "Head", "Neck", "Torso", "Right weapon slot", "Left weapon slot",
		"Right ring", "Left ring", "Belt", "Feet", "Gloves" },
	.radarEnabled = "Radar enabled",
	.radarDisabled = "Radar disabled",
	.radarUnavailable = "Radar waiting for loaded terrain, or paused",
	.radarWaitingHeading = "Walk a few steps to orient the radar",
	.radarAudioUnavailable = "Radar audio unavailable",
	.radarTest = "Radar sound test: front, left, right, back; opening then blocked",
	.radarDepth = "%.0f subtiles clear",
	.radarDirections = {"Front", "Left", "Right", "Back"},
	.radarStates = {"unknown", "open", "blocked"},
};

inline constexpr Table Spanish {
	.pluginLoaded        = "Accesibilidad de Diablo 2 cargada",
	.screenReaderMissing = "lector de pantalla no disponible",
	.panelOpened         = "%s",
	.panelClosed         = "%s cerrado",
	.panelToggled        = "%s",
	.navigation          = "yendo a %s",
	.valueChanged        = "%s: %s",
	.roleButton          = "botón",
	.roleCheckbox        = "casilla",
	.roleDropdown        = "desplegable",
	.roleSlider          = "deslizador",
	.roleTextField       = "campo de texto",
	.roleTabs            = "pestañas",
	.roleList            = "lista",
	.checked             = "marcado",
	.unchecked           = "no marcado",
	.optionOfCount       = "opción %d de %d",
	.optionIndex         = "opción %d",
	.resolution          = "%u por %u",
	.characterLevel      = "nivel %d",
	.unavailable         = "no disponible",
	.modalConfirmHint    = "A para confirmar, B para cancelar",
	.modalDismissHint    = "A para cerrar",
	.dialogueMenu        = "Conversación",
	.gameTextEnabled     = "Lectura de textos del juego activada",
	.gameTextDisabled    = "Lectura de textos del juego desactivada",
	.inventory           = "Inventario",
	.stash               = "Alijo",
	.equipment           = "Equipo",
	.belt                = "Cinturón",
	.gridCell            = "Fila %d, columna %d",
	.equipmentSlots      = { "", "Cabeza", "Cuello", "Torso", "Arma derecha", "Arma izquierda",
		"Anillo derecho", "Anillo izquierdo", "Cinturón", "Pies", "Guantes" },
	.radarEnabled = "Radar activado",
	.radarDisabled = "Radar desactivado",
	.radarUnavailable = "Radar esperando terreno cargado, o en pausa",
	.radarWaitingHeading = "Camina unos pasos para orientar el radar",
	.radarAudioUnavailable = "Sonidos del radar no disponibles",
	.radarTest = "Prueba de radar: delante, izquierda, derecha, atrás; apertura y después bloqueo",
	.radarDepth = "%.0f subcasillas libres",
	.radarDirections = {"Delante", "Izquierda", "Derecha", "Atrás"},
	.radarStates = {"desconocido", "abierto", "bloqueado"},
};

// Chosen once at load from the game's own text language (see plugin.cpp).
const Table& Current();
void         Select(Language language);

// The game's "strCancel" string in each language, used to detect which text
// locale the game is running (values from data/local/lng/strings/ui.json).
inline constexpr std::string_view CancelProbeKey     = "strCancel";
inline constexpr std::string_view CancelProbeSpanish = "CANCELAR";

}
