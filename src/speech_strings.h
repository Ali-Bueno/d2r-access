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
	const char* skillOfCount; // one-based skill position within the visible branch
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
	const char* radarOccupied;
	std::array<const char*, 4> radarDirections;
	std::array<const char*, 3> radarStates;
	const char* radarMoving; // %s = actual recent screen displacement direction
	const char* radarStationary;
	const char* radarMotionUnknown;
	const char* radarStraight;
	std::array<const char*, 8> radarMovementDirections; // clockwise from screen up
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
	.skillOfCount        = "Skill %d of %d",
	.assignedTo          = "Assigned to %s",
	.controllerButtons   = {"D-pad", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Menu", "View", "Left stick", "Press left stick", "Right stick", "Press right stick", "LB", "RB", "LT", "RT", "A", "B", "X", "Y"},
	.equipmentSlots      = { "", "Head", "Neck", "Torso", "Right weapon slot", "Left weapon slot",
		"Right ring", "Left ring", "Belt", "Feet", "Gloves" },
	.radarEnabled = "Radar enabled",
	.footstepsEnabled = "Stalled footstep filter enabled",
	.footstepsDisabled = "Stalled footstep filter disabled",
	.footstepsUnavailable = "Footstep filter unavailable for this game build",
	.radarDisabled = "Radar disabled",
	.radarUnavailable = "Radar terrain is not available yet",
	.radarReaderUnavailable = "Radar terrain reader could not start",
	.radarWaitingPlayer = "Radar waiting for the character to enter the game",
	.radarUiStates = {"Radar suspended while the game window is inactive", "Radar cannot read the menu state",
		"Radar suspended during dialogue", "Radar suspended in the pause menu"},
	.radarAudioUnavailable = "Radar audio unavailable",
	.radarTest = "Radar sound test: up, left, right, down; opening then blocked",
	.radarDepth = "%.0f subtiles clear",
	.radarNearbyObstacle = "obstacle nearby",
	.radarLimited = "limited space",
	.radarOccupied = "occupied passage",
	.radarDirections = {"Up", "Left", "Right", "Down"},
	.radarStates = {"unknown", "open", "blocked"},
	.radarMoving = "Moving %s",
	.radarStationary = "No recent movement",
	.radarMotionUnknown = "Movement not yet measured",
	.radarStraight = "Straight-line passages",
	.radarMovementDirections = {"up", "up and right", "right", "down and right", "down", "down and left", "left", "up and left"},
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
	.skillOfCount        = "Habilidad %d de %d",
	.assignedTo          = "Asignada a %s",
	.controllerButtons   = {"Cruceta", "Cruceta arriba", "Cruceta abajo", "Cruceta izquierda", "Cruceta derecha", "Menú", "Vista", "Stick izquierdo", "Pulsar stick izquierdo", "Stick derecho", "Pulsar stick derecho", "LB", "RB", "LT", "RT", "A", "B", "X", "Y"},
	.equipmentSlots      = { "", "Cabeza", "Cuello", "Torso", "Arma derecha", "Arma izquierda",
		"Anillo derecho", "Anillo izquierdo", "Cinturón", "Pies", "Guantes" },
	.radarEnabled = "Radar activado",
	.footstepsEnabled = "Silenciar pasos al quedar bloqueado: activado",
	.footstepsDisabled = "Silenciar pasos al quedar bloqueado: desactivado",
	.footstepsUnavailable = "Filtro de pasos no disponible para esta versión del juego",
	.radarDisabled = "Radar desactivado",
	.radarUnavailable = "Terreno del radar todavía no disponible",
	.radarReaderUnavailable = "No se pudo iniciar el lector de terreno del radar",
	.radarWaitingPlayer = "Radar esperando a que el personaje entre en la partida",
	.radarUiStates = {"Radar suspendido: la ventana del juego no está activa", "El radar no puede leer el estado de los menús",
		"Radar suspendido durante el diálogo", "Radar suspendido en el menú de pausa"},
	.radarAudioUnavailable = "Sonidos del radar no disponibles",
	.radarTest = "Prueba de radar: arriba, izquierda, derecha, abajo; apertura y después bloqueo",
	.radarDepth = "%.0f subcasillas libres",
	.radarNearbyObstacle = "obstáculo cercano",
	.radarLimited = "espacio limitado",
	.radarOccupied = "paso ocupado",
	.radarDirections = {"Arriba", "Izquierda", "Derecha", "Abajo"},
	.radarStates = {"desconocido", "abierto", "bloqueado"},
	.radarMoving = "Desplazándote hacia %s",
	.radarStationary = "Sin desplazamiento reciente",
	.radarMotionUnknown = "Desplazamiento todavía sin medir",
	.radarStraight = "Pasos en línea recta",
	.radarMovementDirections = {"arriba", "arriba y derecha", "derecha", "abajo y derecha", "abajo", "abajo y izquierda", "izquierda", "arriba y izquierda"},
};

// Chosen once at load from the game's own text language (see plugin.cpp).
const Table& Current();
void         Select(Language language);

// The game's "strCancel" string in each language, used to detect which text
// locale the game is running (values from data/local/lng/strings/ui.json).
inline constexpr std::string_view CancelProbeKey     = "strCancel";
inline constexpr std::string_view CancelProbeSpanish = "CANCELAR";

}
