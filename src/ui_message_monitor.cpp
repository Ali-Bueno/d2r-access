#include "async_log.h"
#include "ui_message_monitor.h"
#include "audio_description.h"
#include "movie_probe.h"

#include "quest_service.h"
#include "menu_navigation.h"

#include "generated/panel_titles.h"
#include "localization.h"
#include "panel_state.h"
#include "death_place_service.h"
#include "screen_reader.h"
#include "code_dump.h"
#include "control_hints.h"
#include "control_state.h"
#include "focus_tracker.h"
#include "legacy_graphics.h"
#include "native_widgets.h"
#include "speech_strings.h"
#include "ui_text.h"
#include "ui_thread_pump.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

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
// A panel's title is not cut off by the first focus the game gives it a moment later
// (a tester, 2026-10-15: "Main Menu" cut off by the character it lands on).
constexpr unsigned long long PanelTitleHoldMs = 700;
constexpr std::string_view MainMenuPanel = "MainMenuPanel";
constexpr const char* TogglePanelCommand        = "TogglePanel";

// Front-end screen transitions ("FrontEndNavigation:ToExit", "To<Screen>").
// Panels opened before the front end is entered (the title screen, and the
// in-game HUD panels the game preloads then, such as EmoteWheelPanel) are not
// shown in the front end even though no close message follows, so entering
// it starts the panel record afresh.
constexpr const char* FrontEndNavigationTarget = "FrontEndNavigation";

// An act cinematic's start and end, as the game announces them (live, 2026-09-28).
constexpr const char* MoviePlayerTarget = "MoviePlayerMessage";
constexpr const char* MoviePlayCommand  = "Play";
constexpr const char* CinematicsTarget  = "Cinematics";
constexpr const char* MovieDoneCommand  = "MovieComplete";
constexpr const char* VideosPlayCommand = "Play"; // the Videos list, its index as text
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
constexpr std::string_view RebindPanel       = "ButtonBindingModal";
// Character select's order to delete comes just before its modal closes (a tester, 2026-10-22).
constexpr std::string_view DeleteModalPanel       = "DeleteCharacterModal";
constexpr const char*      CharacterSelectTarget  = "CharacterSelect";
constexpr const char*      DeleteCharacterCommand = "DeleteCharacter";
constexpr const char*      KeyBindingTarget       = "KeyBindingPanelMessage";
constexpr const char*      StartBindingCommand    = "StartBinding";
bool g_characterDeleted = false;
// A confirmed gold window closes as the gold moves; the move is the news, not the close (a tester, 2026-10-23).
constexpr std::string_view GoldModalPanel     = "DropGoldModal";
constexpr const char*      GoldPanelTarget    = "GoldPanelMessage";
constexpr const char*      GoldConfirmCommand = "DropGold";
bool g_goldConfirmed = false;
// Character select's difficulty window has no title widget; the game's own
// "Select Difficulty" names it, not its layout name (a tester, 2026-10-30).
constexpr std::string_view DifficultyModalPanel = "CharacterDifficultyModal";
constexpr std::string_view SelectDifficultyKey  = "strselectdiff";
// The credits have no title widget either: the main menu's "Credits" button names them, not
// "Credits Modal" (a tester, 2026-11-02).
constexpr std::string_view CreditsModalPanel = "CreditsModal";
constexpr std::string_view CreditsKey        = "strGameCredits";

// A modal's prompt (ConfirmationModalHD.json: Frame/Prompt) is set at
// runtime; it is read natively a few updates after the modal opens, then the
// controller hint follows it.
const std::string  ModalPromptPath[] = { "Frame", "Prompt" };
constexpr unsigned ModalTextDelayMs  = 120;
struct ModalRead {
	std::string        panel;
	unsigned long long dueTick = 0;
};
ModalRead g_modalRead;
// The gold window's title is set as it opens: Drop Gold, Deposit, Withdraw or Offer by
// what else is open; the layout's own is always Drop Gold (a tester, 2026-10-20).
constexpr std::string_view RuntimeTitlePanels[] = { "DropGoldModal" };
const std::string  RuntimeTitlePath[] = { "Title" };
ModalRead g_titleRead;
// The death and end screens say themselves, "You have died" first (ModalReader); the
// layout name only delayed that (a tester, 2026-10-23).
constexpr std::string_view SelfReadPanels[] = { "YouDiedModal", "EndScreenPanel", "EndScreenPanelExpansion" };

// Widgets whose runtime text is still being probed natively when their panel
// opens, to learn where their class keeps it (see native_widgets.h).
struct TextProbe {
	const char* panel;
	const char* path[3];
};
constexpr TextProbe TextProbes[] = {
	{ "ConfirmationModal", { "Frame", "Confirm", nullptr } },
	{ "ConfirmationModal", { "Frame", "Dismiss", nullptr } },
	{ "DismissableModal", { "Frame", "Dismiss", nullptr } },
};
constexpr unsigned TextProbeDelayMs = 150;

void RequestTextProbes(std::string_view panel) {
	for (const TextProbe& probe : TextProbes) {
		if (panel != probe.panel) {
			continue;
		}
		std::vector<std::string> path;
		for (const char* step : probe.path) {
			if (step != nullptr) {
				path.emplace_back(step);
			}
		}
		NativeWidgets::RequestProbe(panel, path, TextProbeDelayMs);
	}
}

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

// The quest log announces its own traffic here: SelectQuest, SelectTab,
// SetQuestAlert, QuestUpdate. None carries text, so the quest watch reads the
// panel itself once the game has filled it.
constexpr const char* QuestLogTarget = "QuestLogPanelMessage";
// Sent on every switch between legacy and modern graphics, by the key and by the creation
// screen's button alike (MEASURED 2026-11-11, docs/legacy-graphics.md).
constexpr const char* SystemTarget = "SystemMessage";
constexpr const char* RendererChangedCommand = "RenderingToHDChanged";

// Messages that flood the trace without carrying UI state (observed per
// frame). Entries are "target" or "target:command".
constexpr const char* TraceIgnored[] = { "MonsterHealth", "InputMessage", "CharacterSelect:SelectAct", "MainMenuHD" };

const D2RL::SharedEventService*  g_events  = nullptr;
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
	D2RAccess::AsyncLog::Info(context, line);
}

void Say(const char* format, const std::string& subject, bool interrupt, unsigned long long holdMs = 0) {
	char phrase[256] {};
	std::snprintf(phrase, sizeof(phrase), format, subject.c_str());
	// The window path's own line, in the words the player actually hears.
	// [tabs] already records what the tab reader said; without this pair the
	// log cannot show whether the same name was spoken twice, once by each
	// path, which is what the "tabs read twice" report needs to settle.
	D2RAccess::AsyncLog::InfoF(g_context, "[window] \"%s\"%s", phrase, interrupt ? " (interrupts)" : "");
	if (holdMs) ScreenReader::Urge(phrase, GetTickCount64() + holdMs);
	else ScreenReader::Speak(phrase, interrupt);
}

// Localized title from the generated layout table, or empty when the panel has none.
std::string PanelTitle(std::string_view panel) {
	for (const auto& entry : Generated::PanelTitles) {
		if (panel == entry.panel) {
			return Localization::Text(entry.stringKey);
		}
	}
	// The game's strings have no plain "Main Menu" (only "Return to Main Menu").
	if (panel == MainMenuPanel) return Strings::Current().mainMenu;
	if (panel == DifficultyModalPanel) return Localization::Text(SelectDifficultyKey);
	if (panel == CreditsModalPanel) return Localization::Text(CreditsKey);
	return {};
}

// An untitled panel's layout name in words, "TitleScreenPanel" as "Title Screen".
std::string PanelWords(std::string_view panel) {
	constexpr std::string_view Suffix = "Panel";
	if (panel.size() > Suffix.size() && panel.ends_with(Suffix)) panel.remove_suffix(Suffix.size());
	std::string words;
	for (size_t i = 0; i < panel.size(); ++i) {
		const bool upper = panel[i] >= 'A' && panel[i] <= 'Z';
		if (i && upper && panel[i - 1] >= 'a' && panel[i - 1] <= 'z') words += ' ';
		words += panel[i];
	}
	return words;
}

// Helper panels (tooltips layer, link bars, loading overlays) open and close
// constantly; only screens, titled panels and modals are worth hearing.
bool IsAnnouncedPanel(std::string_view panel, bool exclusive, const std::string& title) {
	if (std::ranges::find(SelfReadPanels, panel) != std::end(SelfReadPanels)) return false;
	return exclusive || !title.empty() || EndsWith(panel, ModalNameSuffix);
}

// Opening a panel is a context change: interrupt. Closing one is only worth
// hearing when it was the active panel, and it never interrupts, so an open
// that follows immediately wins.
void AnnouncePanel(const D2RL::SharedEvents::UiMessageEvent& event) {
	const std::string panel     = Safe(event.text);
	const bool        exclusive = std::strcmp(event.command, OpenExclusivePanelCommand) == 0;
	const std::string title     = PanelTitle(panel);
	const std::string spoken    = title.empty() ? PanelWords(panel) : title;

	if (std::strcmp(event.command, OpenPanelCommand) == 0 || exclusive) {
		PanelState::Opened(panel);
		RequestTextProbes(panel);
		if (IsAnnouncedPanel(panel, exclusive, title)) {
			if (std::ranges::find(RuntimeTitlePanels, panel) != std::end(RuntimeTitlePanels)) {
				g_titleRead = { panel, GetTickCount64() + ModalTextDelayMs };
			} else {
				Say(Strings::Current().panelOpened, spoken, true, PanelTitleHoldMs);
				// Cleaned as the tab reader cleans a tab's text, so the switcher's
				// report of this same panel compares equal.
				PanelState::Announced(UiText::Clean(spoken));
			}
			if (panel == ConfirmationModalPanel || panel == DismissableModalPanel) {
				g_modalRead = { panel, GetTickCount64() + ModalTextDelayMs };
			}
		}
	} else if (std::strcmp(event.command, ClosePanelCommand) == 0 || std::strcmp(event.command, UnloadPanelCommand) == 0) {
		// The rebinding window's close is said as what it bound (SkillBarService).
		if (panel == DeleteModalPanel && std::exchange(g_characterDeleted, false)) {
			PanelState::Closed(panel);
			Say("%s", Strings::Current().characterDeleted, false, PanelTitleHoldMs);
		} else if (panel == GoldModalPanel && std::exchange(g_goldConfirmed, false)) {
			PanelState::Closed(panel);
		} else if (PanelState::Closed(panel) && IsAnnouncedPanel(panel, false, title) && panel != RebindPanel) {
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
	if (event->command && std::strcmp(event->command, OpenPanelCommand) == 0
		&& std::strcmp(Safe(event->text), "YouDiedModal") == 0) DeathPlaceService::NoteDeath();
	if (event->target == nullptr || event->command == nullptr) {
		return UiMessageAction::Continue;
	}

	// UI messages arrive on the UI thread: a safe place to (re)arm the focus pump.
	MenuNavigation::NoteAction(event->target, event->command);
	UiThreadPump::Kick(context);

	if (g_trace && !IsTraceNoise(event->target, event->command)) {
		Trace(context, *event);
	}
	if (std::strcmp(event->target, PanelManagerTarget) == 0) {
		FocusTracker::PanelsChanged();
		Localization::DetectSpeechLanguage(context); // no-op once detected
		AnnouncePanel(*event);
	} else if (std::strcmp(event->target, MoviePlayerTarget) == 0 && std::strcmp(event->command, MoviePlayCommand) == 0) {
		PanelState::MovieStarted();
		AsyncLog::Info(context, "[ui] cinematic started: cues paused");
		AudioDescription::MovieStarted(context);
		MovieProbe::Started();
	} else if (std::strcmp(event->target, CinematicsTarget) == 0 && std::strcmp(event->command, MovieDoneCommand) == 0) {
		PanelState::MovieEnded();
		AsyncLog::Info(context, "[ui] cinematic ended");
		AudioDescription::MovieEnded(context);
		MovieProbe::Ended();
	} else if (std::strcmp(event->target, CinematicsTarget) == 0) {
		AsyncLog::InfoF(context, "[ui] cinematics: %s text=\"%s\"", event->command, event->text ? event->text : "");
		if (std::strcmp(event->command, VideosPlayCommand) == 0) AudioDescription::MenuChoice(context, event->text ? event->text : "");
	} else if (std::strcmp(event->target, CharacterSelectTarget) == 0 && std::strcmp(event->command, DeleteCharacterCommand) == 0) {
		g_characterDeleted = true;
	} else if (std::strcmp(event->target, KeyBindingTarget) == 0 && std::strcmp(event->command, StartBindingCommand) == 0) {
		ScreenReader::Speak(Strings::Current().keyBindPrompt, true);
	} else if (std::strcmp(event->target, GoldPanelTarget) == 0 && std::strcmp(event->command, GoldConfirmCommand) == 0) {
		g_goldConfirmed = true;
	} else if (std::strcmp(event->target, ScrollControllerTarget) == 0) {
		FocusTracker::NoteScrollInput();
	} else if (std::strcmp(event->target, QuestLogTarget) == 0) {
		QuestService::Note(event->command);
	} else if (std::strcmp(event->target, SystemTarget) == 0 && std::strcmp(event->command, RendererChangedCommand) == 0) {
		LegacyGraphics::Changed(GetTickCount64());
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
	if (context->QueryService(&g_events) != D2RL::ServiceQueryResult::Success) {
		return false;
	}
	if (!D2RL::HasSharedEventServiceField(g_events, D2RL::SharedEventServiceRequiredSize)) {
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
	g_titleRead = {};
	PanelState::Reset();
}

// A runtime title is read natively a few updates after its panel opened.
void TickRuntimeTitle(const D2RL::PluginContext* context) {
	if (g_titleRead.panel.empty() || GetTickCount64() < g_titleRead.dueTick) return;
	const std::string panel = std::move(g_titleRead.panel);
	g_titleRead = {};
	const auto& open = PanelState::OpenPanels();
	if (std::ranges::find(open, panel) == open.end()) return;
	std::string title = UiText::Clean(NativeWidgets::TextAt(panel, RuntimeTitlePath));
	D2RAccess::AsyncLog::InfoF(context, "[modal] %s title: \"%s\"", panel.c_str(), title.c_str());
	if (title.empty()) title = PanelTitle(panel);
	Say(Strings::Current().panelOpened, title, true, PanelTitleHoldMs);
	PanelState::Announced(UiText::Clean(title));
}

void Tick(const D2RL::PluginContext* context) {
	TickRuntimeTitle(context);
	if (g_modalRead.panel.empty() || GetTickCount64() < g_modalRead.dueTick) {
		return;
	}
	const std::string panel = std::move(g_modalRead.panel);
	g_modalRead             = {};
	const std::string prompt = NativeWidgets::TextAt(panel, ModalPromptPath);
	D2RAccess::AsyncLog::InfoF(context, "[modal] %s prompt: \"%s\"", panel.c_str(), prompt.c_str());
	if (!prompt.empty()) {
		ScreenReader::Speak(prompt, false);
	}
	// Which button confirms is a control hint like any other: held for right
	// stick down, and spoken unasked only last, with autodetail and control
	// tooltips on.
	const char* hint = panel == ConfirmationModalPanel ? Strings::Current().modalConfirmHint : Strings::Current().modalDismissHint;
	ControlHints::Offer(hint);
	ControlHints::SpeakAfter(context, hint);
}

void SetTraceEnabled(bool enabled) {
	g_trace = enabled;
}

bool IsTraceEnabled() {
	return g_trace;
}

}
