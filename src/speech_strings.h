#pragma once

#include <string_view>

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
	const char* modalConfirmHint;  // spoken after a confirmation modal's title
	const char* modalDismissHint;  // spoken after a dismissable modal's title
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
	.modalConfirmHint    = "A to confirm, B to cancel",
	.modalDismissHint    = "A to close",
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
	.modalConfirmHint    = "A para confirmar, B para cancelar",
	.modalDismissHint    = "A para cerrar",
};

// Chosen once at load from the game's own text language (see plugin.cpp).
const Table& Current();
void         Select(Language language);

// The game's "strCancel" string in each language, used to detect which text
// locale the game is running (values from data/local/lng/strings/ui.json).
inline constexpr std::string_view CancelProbeKey     = "strCancel";
inline constexpr std::string_view CancelProbeSpanish = "CANCELAR";

}
