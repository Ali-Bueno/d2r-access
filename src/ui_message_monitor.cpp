#include "ui_message_monitor.h"

#include "generated/panel_titles.h"
#include "localization.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "code_dump.h"
#include "control_state.h"
#include "focus_tracker.h"
#include "speech_strings.h"
#include "ui_thread_pump.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace D2RAccess::UiMessageMonitor {

namespace {

// Message vocabulary used by the game's own panel plumbing, as documented by
// the SDK (widget-localization sample: "PanelManager:ClosePanel:<name>") and
// observed in the plugin's own trace.
constexpr const char* PanelManagerTarget        = "PanelManager";
constexpr const char* OpenPanelCommand          = "OpenPanel";
constexpr const char* OpenExclusivePanelCommand = "OpenExclusivePanel";
constexpr const char* ClosePanelCommand         = "ClosePanel";
constexpr const char* UnloadPanelCommand        = "UnloadPanel";
constexpr const char* TogglePanelCommand        = "TogglePanel";

// Front-end screen transitions ("FrontEndNavigation:ToExit", "To<Screen>").
// Panels opened before the front end is entered (the title screen, and the
// in-game HUD panels the game preloads then, such as EmoteWheelPanel) are not
// shown in the front end even though no close message follows, so entering
// it starts the panel record afresh.
constexpr const char* FrontEndNavigationTarget = "FrontEndNavigation";
constexpr const char* NavigationCommandPrefix  = "To";

// Screens that carry a selection in the message text itself
// ("CharacterCreate:Hover:Barbarian"); the text doubles as a string-table key.
constexpr const char* HoverCommand  = "Hover";
constexpr const char* SelectCommand = "Select";

// CharacterCreatePanelHD.json fills its "ClassDescription" text box at
// runtime; the texts are these string-table records (local/lng/strings/ui.json),
// keyed here by the class key the Hover message carries.
struct ClassDescription {
	const char* classKey;
	const char* descriptionKey;
};
constexpr ClassDescription ClassDescriptions[] = {
	{ "Amazon", "strAmazonDesc" },
	{ "Assassin", "strAssDesc" },
	{ "Barbarian", "strBarbDesc" },
	{ "Druid", "strDruDesc" },
	{ "Necromancer", "strNecroDesc" },
	{ "Paladin", "strPalDesc" },
	{ "Sorceress", "strSorcDesc" },
	{ "Warlock", "strWarlockDesc" },
};

// Modal panels have no static title but always deserve an announcement. Their
// prompt text is set at runtime (not readable yet), so the controller hint is
// spoken instead: ConfirmationModal has Confirm/Dismiss buttons
// (ModalMessage:Confirm / ModalMessage:Dismiss), DismissableModal only Dismiss.
constexpr const char* ModalNameSuffix        = "Modal";
constexpr const char* ConfirmationModalPanel = "ConfirmationModal";
constexpr const char* DismissableModalPanel  = "DismissableModal";

// Modals whose prompt is set at runtime but whose opening message says what
// they ask: that message's text is spoken with the modal's title.
// CharacterSelectPanelHD.json: DeleteCharacter button -> "CharacterSelect:
// ShowDeleteCharacterModal" (tooltip "@strDelete"); the game then opens a
// ConfirmationModal.
struct ModalPrompt {
	const char* target;
	const char* command;
	const char* promptKey;
};
constexpr ModalPrompt ModalPrompts[] = {
	{ "CharacterSelect", "ShowDeleteCharacterModal", "strDelete" },
};
std::string g_pendingModalPrompt;

// Option values are re-read from Settings.json after these, since the game
// wrote (or reverted) them.
constexpr const char* OptionsPanelTarget    = "OptionsPanelMessage";
constexpr const char* SettingsPanelName     = "SettingsPanel";
constexpr const char* OptionsCommitCommands[] = { "AcceptChanges", "CancelChanges", "ResetToDefaults" };
// Changing the text language reloads the game's string tables (and the front
// end), so the speech language is probed again afterwards.
constexpr const char* SetTextLanguageCommand = "SetTextLanguage";

// Scrolling moves every row of a list, so cached widget rects go stale.
constexpr const char* ScrollControllerTarget = "ScrollController";

// Messages that flood the trace without carrying UI state (observed per
// frame). Entries are "target" or "target:command".
constexpr const char* TraceIgnored[] = { "MonsterHealth", "InputMessage", "CharacterSelect:SelectAct", "MainMenuHD" };

const D2RL::SharedEventServiceV1*  g_events  = nullptr;
const D2RL::PluginContext*         g_context = nullptr;
D2RL::SharedEvents::ListenerHandle g_handle  = D2RL::SharedEvents::InvalidHandle;
bool                               g_trace   = true;
std::string                        g_lastSelection;

const char* Safe(const char* text) {
	return text != nullptr ? text : "";
}

bool EndsWith(std::string_view text, std::string_view suffix) {
	return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool IsTraceNoise(const char* target, const char* command) {
	char pair[128] {};
	std::snprintf(pair, sizeof(pair), "%s:%s", target, command);
	for (const char* ignored : TraceIgnored) {
		if (std::strcmp(target, ignored) == 0 || std::strcmp(pair, ignored) == 0) {
			return true;
		}
	}
	return false;
}

void Trace(const D2RL::PluginContext* context, const D2RL::SharedEvents::UiMessageEvent& event) {
	char line[512] {};
	std::snprintf(line, sizeof(line), "[ui] target=\"%s\" command=\"%s\" text=\"%s\"", Safe(event.target), Safe(event.command), Safe(event.text));
	context->LogInfo(line);
}

void Say(const char* format, const std::string& subject, bool interrupt) {
	char phrase[256] {};
	std::snprintf(phrase, sizeof(phrase), format, subject.c_str());
	ScreenReader::Speak(phrase, interrupt);
}

// Localized title from the generated layout table, or empty when the panel has none.
std::string PanelTitle(std::string_view panel) {
	for (const auto& entry : Generated::PanelTitles) {
		if (panel == entry.panel) {
			return Localization::Text(entry.stringKey);
		}
	}
	return {};
}

// Helper panels (tooltips layer, link bars, loading overlays) open and close
// constantly; only screens, titled panels and modals are worth hearing.
bool IsAnnouncedPanel(std::string_view panel, bool exclusive, const std::string& title) {
	return exclusive || !title.empty() || EndsWith(panel, ModalNameSuffix);
}

// Opening a panel is a context change: interrupt. Closing one is only worth
// hearing when it was the active panel, and it never interrupts, so an open
// that follows immediately wins.
void AnnouncePanel(const D2RL::SharedEvents::UiMessageEvent& event) {
	const std::string panel     = Safe(event.text);
	const bool        exclusive = std::strcmp(event.command, OpenExclusivePanelCommand) == 0;
	const std::string title     = PanelTitle(panel);
	const std::string spoken    = title.empty() ? panel : title;

	if (std::strcmp(event.command, OpenPanelCommand) == 0 || exclusive) {
		PanelState::Opened(panel);
		if (IsAnnouncedPanel(panel, exclusive, title)) {
			Say(Strings::Current().panelOpened, spoken, true);
			if (!g_pendingModalPrompt.empty() && EndsWith(panel, ModalNameSuffix)) {
				ScreenReader::Speak(g_pendingModalPrompt, false);
				g_pendingModalPrompt.clear();
			}
			if (panel == ConfirmationModalPanel) {
				ScreenReader::Speak(Strings::Current().modalConfirmHint, false);
			} else if (panel == DismissableModalPanel) {
				ScreenReader::Speak(Strings::Current().modalDismissHint, false);
			}
		}
	} else if (std::strcmp(event.command, ClosePanelCommand) == 0 || std::strcmp(event.command, UnloadPanelCommand) == 0) {
		if (PanelState::Closed(panel) && IsAnnouncedPanel(panel, false, title)) {
			Say(Strings::Current().panelClosed, spoken, false);
		}
		if (panel == SettingsPanelName) {
			ControlState::Invalidate();
		}
	} else if (std::strcmp(event.command, TogglePanelCommand) == 0) {
		Say(Strings::Current().panelToggled, spoken, true);
	}
}

constexpr const char* FrontEndReadyCommand = "ToFrontEnd";

// "ToCharacterSelect" -> "CharacterSelect".
void AnnounceNavigation(const D2RL::SharedEvents::UiMessageEvent& event) {
	const char*  destination  = event.command;
	const size_t prefixLength = std::strlen(NavigationCommandPrefix);
	if (std::strncmp(destination, NavigationCommandPrefix, prefixLength) == 0) {
		destination += prefixLength;
	}
	Say(Strings::Current().navigation, destination, true);
}

// Hover/Select messages name the hovered item; speak it once per change.
void AnnounceSelection(const D2RL::SharedEvents::UiMessageEvent& event) {
	const std::string item = Safe(event.text);
	if (item.empty() || item == g_lastSelection) {
		return;
	}
	g_lastSelection = item;
	std::string label = Localization::Text(item);
	if (label.empty()) {
		label = item;
	}
	ScreenReader::Speak(label, true);
	for (const ClassDescription& entry : ClassDescriptions) {
		if (item == entry.classKey) {
			if (const std::string description = Localization::Text(entry.descriptionKey); !description.empty()) {
				ScreenReader::Speak(description, false);
			}
		}
	}
}

auto __cdecl OnUiMessage(const D2RL::PluginContext* context, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept -> D2RL::SharedEvents::UiMessageAction {
	using D2RL::SharedEvents::UiMessageAction;
	if (context == nullptr || event == nullptr || event->structSize < D2RL::SharedEvents::UiMessageEventRequiredSize) {
		return UiMessageAction::Continue;
	}
	if (event->target == nullptr || event->command == nullptr) {
		return UiMessageAction::Continue;
	}

	// UI messages arrive on the UI thread: a safe place to (re)arm the focus pump.
	UiThreadPump::Kick(context);

	if (g_trace && !IsTraceNoise(event->target, event->command)) {
		Trace(context, *event);
	}
	if (std::strcmp(event->target, PanelManagerTarget) == 0) {
		FocusTracker::InvalidateRects();
		Localization::DetectSpeechLanguage(context); // no-op once detected
		AnnouncePanel(*event);
	} else if (std::strcmp(event->target, ScrollControllerTarget) == 0) {
		FocusTracker::InvalidateRects();
	} else if (std::strcmp(event->target, FrontEndNavigationTarget) == 0) {
		AnnounceNavigation(*event);
		if (std::strcmp(event->command, FrontEndReadyCommand) == 0) {
			PanelState::Reset();
			FocusTracker::Reset();
			Localization::DetectSpeechLanguage(context);
			(void)CodeDump::EnsureDumped(context);
		}
	} else if (std::strcmp(event->command, HoverCommand) == 0 || std::strcmp(event->command, SelectCommand) == 0) {
		AnnounceSelection(*event);
	} else {
		if (std::strcmp(event->target, OptionsPanelTarget) == 0) {
			for (const char* commit : OptionsCommitCommands) {
				if (std::strcmp(event->command, commit) == 0) {
					ControlState::Invalidate();
				}
			}
			if (std::strcmp(event->command, SetTextLanguageCommand) == 0) {
				Localization::ForgetSpeechLanguage();
			}
		}
		for (const ModalPrompt& prompt : ModalPrompts) {
			if (std::strcmp(event->target, prompt.target) == 0 && std::strcmp(event->command, prompt.command) == 0) {
				g_pendingModalPrompt = Localization::Text(prompt.promptKey);
			}
		}
		// Value controls report changes through their layout update message.
		(void)FocusTracker::AnnounceValue(context, event->target, event->command, Safe(event->text));
	}
	return UiMessageAction::Continue;
}

}

bool Register(const D2RL::PluginContext* context) {
	if (context == nullptr) {
		return false;
	}
	if (context->QueryService(D2RL::ServiceId::SharedEvent, D2RL::SharedEventServiceV1Version, &g_events) != D2RL::ServiceQueryResult::Success) {
		return false;
	}
	if (!D2RL::HasSharedEventServiceV1Field(g_events, D2RL::SharedEventServiceV1RequiredSize)) {
		return false;
	}

	const D2RL::SharedEvents::UiMessageListener listener {
		.structSize = D2RL::SharedEvents::UiMessageListenerSize,
		.callback   = OnUiMessage,
	};
	if (g_events->registerUiMessageListener(context, &listener, &g_handle) != D2RL::SharedEvents::Result::Success) {
		return false;
	}
	g_context = context;
	return true;
}

void Unregister() {
	if (g_events != nullptr && g_context != nullptr && g_handle != D2RL::SharedEvents::InvalidHandle) {
		(void)g_events->unregisterUiMessageListener(g_context, g_handle);
	}
	g_handle  = D2RL::SharedEvents::InvalidHandle;
	g_context = nullptr;
	g_events  = nullptr;
	g_lastSelection.clear();
	PanelState::Reset();
}

void SetTraceEnabled(bool enabled) {
	g_trace = enabled;
}

bool IsTraceEnabled() {
	return g_trace;
}

}
