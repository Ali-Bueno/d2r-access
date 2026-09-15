#pragma once

#include <D2RLPlugin/api.h>

#include <string>
#include <string_view>

// Thin wrapper over the SDK's LocalizationService: resolves the game's own
// string-table keys ("strCancel", "OptionsHD") in the player's language.
namespace D2RAccess::Localization {

bool Initialize(const D2RL::PluginContext* context);
void Shutdown();

// Localized text for a string-table key (without the leading '@'), or an empty
// string when the key is unknown or the service is unavailable.
std::string Text(std::string_view key);

// Picks the plugin's own speech language from the game's text language by
// resolving one known game string. The tables are not loaded when the plugin
// loads, so call again once the front end is up; it settles on first success.
void DetectSpeechLanguage(const D2RL::PluginContext* context);

// The game's text language changed (it reloads its string tables): probe
// again on the next DetectSpeechLanguage call.
void ForgetSpeechLanguage();

}
