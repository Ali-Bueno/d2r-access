#include "modal_reader.h"

#include "async_log.h"
#include "character_stats_reader.h"
#include "control_hints.h"
#include "controller_text.h"
#include "input_direction.h"
#include "localization.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "ui_text.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace D2RAccess::ModalReader {

namespace {

std::string g_lastPanel;

// The title screen shows "Press Any Key to Begin" some time after the panel
// opens, so it is polled for until it appears or the panel changes.
constexpr std::string_view TitlePanel = "TitleScreenPanel";
constexpr std::string_view TitlePrompt = "InputPrompt";
constexpr const char* TitleKeyboardKey = "KeyboardTitleScreenPrompt";
constexpr std::string_view ControllerGlyph = "\xEE\x80\x93"; // U+E013, stripped by UiText::Clean
constexpr uint64_t TitlePollMs = 250;
bool g_titleSpoken = false;
uint64_t g_titlePolled = 0;
// Said once, queued right behind the title's own announcement (the plugin's version).
std::string g_titleAnnouncement;

// A tester: the title took only the keyboard, never the pad. A pad press the
// title has not answered within this long (a few frames) gets a key press in
// its place; one the game did take has changed the panel by then, and nothing
// is sent. docs/controller-mode.md#the-title-and-the-pad
constexpr uint64_t TitlePadAnswerMs = 400;
uint64_t g_titlePadPress = 0;
bool g_titlePadHeld = false;

void PassTitle(const D2RL::PluginContext* context, uint64_t now) {
	using InputDirection::FaceButton;
	bool held = false;
	for (const auto button : {FaceButton::A, FaceButton::B, FaceButton::X, FaceButton::Y})
		held |= InputDirection::FaceButtonHeld(button);
	if (held && !g_titlePadHeld && !g_titlePadPress) {
		g_titlePadPress = now;
		AsyncLog::InfoF(context, "[modal] title: pad press seen (prompt spoken=%d)", g_titleSpoken ? 1 : 0);
	}
	g_titlePadHeld = held;
	if (!g_titlePadPress || now - g_titlePadPress < TitlePadAnswerMs) return;
	g_titlePadPress = 0;
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (process != GetCurrentProcessId()) return;
	INPUT keys[2]{};
	keys[0].type = keys[1].type = INPUT_KEYBOARD;
	keys[0].ki.wVk = keys[1].ki.wVk = VK_SPACE;
	keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
	const UINT sent = SendInput(2, keys, sizeof(INPUT));
	AsyncLog::InfoF(context, "[modal] the title did not take a pad press in %llu ms: a key sent in its place (%u events)",
		static_cast<unsigned long long>(TitlePadAnswerMs), sent);
}

// The skill tree says its branch and the choices left through the interaction reader; read from
// here it was each assigned skill's level and hotkey, "1, X, 1, B", ahead of them (a tester, 2026-11-02).
// HelpButtonPanel is the "Help (H)" label that comes back whenever a menu closes in mouse mode.
constexpr std::string_view OwnReaders[] = {"SkillsTreePanel", "HelpButtonPanel"};

// The game's news and links over the main menu; a tester who owns the expansion asked to silence it (2026-10-20).
constexpr std::string_view MenuLinksPanel = "MainMenuLinks";
std::atomic<bool> g_menuNews{true};

// Screens that fill their boxes after they open while their one button holds the
// focus: read here, not left to the focus reader, and polled like the title prompt
// until the box filled last has its text. Each sentence's boxes are joined by spaces.
struct SelfReadScreen {
	std::string_view panel;
	std::string_view filledLast;
	std::vector<std::vector<std::string_view>> sentences;
};
const SelfReadScreen SelfReadScreens[] = {
	// YouDiedModalHD.json: "You have died / Press <button> to continue." and "Death takes its toll of N Gold".
	{"YouDiedModal", "MainText", {{"MainText"}, {"SubText"}, {"ContinueButton"}}},
	// EndScreenPanelHD.json, after the last boss: "Congratulations! Tank Will Henceforth Be Known
	// As Slayer Tank" (a tester, 2026-10-30). The expansion's evens out four boxes, no Vanquished.
	{"EndScreenPanel", "Title", {{"Congratulations"}, {"Vanquished"}, {"Name", "Henceforth", "Title"}, {"Ok"}}},
	{"EndScreenPanelExpansion", "Title", {{"Congratulations"}, {"Name", "Henceforth", "Title"}, {"Ok"}}},
};
bool g_selfReadSpoken = false;
uint64_t g_selfReadPolled = 0;

const SelfReadScreen* SelfRead(std::string_view panel) {
	const auto screen = std::ranges::find(SelfReadScreens, panel, &SelfReadScreen::panel);
	return screen == std::end(SelfReadScreens) ? nullptr : &*screen;
}

void PollSelfRead(const D2RL::PluginContext* context, const SelfReadScreen& screen) {
	const uint64_t now = GetTickCount64();
	if (now - g_selfReadPolled < TitlePollMs) return;
	g_selfReadPolled = now;
	const uintptr_t panel = NativeWidgets::FindPanel(screen.panel);
	if (!panel) return;
	const auto read = [panel](std::string_view box) {
		const uintptr_t child = NativeWidgets::FindChild(panel, box);
		std::string raw = child ? NativeWidgets::ReadText(child) : std::string{};
		for (size_t at = raw.find('\n'); at != std::string::npos; at = raw.find('\n', at + 2)) raw.replace(at, 1, ". ");
		return ControllerText::Read(raw);
	};
	if (read(screen.filledLast).empty()) return;
	std::string text;
	for (const auto& sentence : screen.sentences) {
		std::string words;
		for (const auto box : sentence) {
			const auto part = read(box);
			if (part.empty()) continue;
			if (!words.empty()) words += ' ';
			words += part;
		}
		if (words.empty()) continue;
		if (!text.empty() && text.back() != '.' && text.back() != '!' && text.back() != '?') text += '.';
		if (!text.empty()) text += ' ';
		text += words;
	}
	g_selfReadSpoken = true;
	AsyncLog::InfoF(context, "[modal] %s -> \"%s\"", std::string(screen.panel).c_str(), text.c_str());
	ScreenReader::Speak(text, true);
}

// Deleting a character takes A held while the modal counts down on screen (a tester,
// 2026-10-22). The count is said from the hold; the Confirm button's live text is logged
// to learn whether the game shows it there. docs/delete-character-modal.md
constexpr std::string_view DeletePanel = "DeleteCharacterModal";
constexpr std::string_view DeleteConfirm = "Confirm";
uint64_t g_deleteHeldSince = 0;
bool g_keyHolds = false; // the keyboard's A is held on Confirm
bool g_keyMenu = false;  // the keyboard's cursor has the menu, and reads its controls as it comes to them
int g_deleteSaid = 0;
std::string g_deleteFace;

void PollDeleteHold(const D2RL::PluginContext* context) {
	const uint64_t now = GetTickCount64();
	if (!InputDirection::FaceButtonHeld(InputDirection::FaceButton::A) && !g_keyHolds) {
		if (g_deleteHeldSince) AsyncLog::InfoF(context, "[modal] delete: A let go after %llu ms",
			static_cast<unsigned long long>(now - g_deleteHeldSince));
		g_deleteHeldSince = 0;
		g_deleteSaid = 0;
		return;
	}
	if (!g_deleteHeldSince) g_deleteHeldSince = now;
	const uintptr_t panel = NativeWidgets::FindPanel(DeletePanel);
	if (const uintptr_t confirm = panel ? NativeWidgets::FindChild(panel, DeleteConfirm) : 0) {
		if (std::string face = NativeWidgets::ReadText(confirm); face != g_deleteFace) {
			AsyncLog::InfoF(context, "[modal] delete: Confirm reads \"%s\" after %llu ms held", face.c_str(),
				static_cast<unsigned long long>(now - g_deleteHeldSince));
			g_deleteFace = std::move(face);
		}
	}
	const int left = HoldCountdown(now - g_deleteHeldSince);
	if (left > 0 && left != g_deleteSaid) {
		g_deleteSaid = left;
		ScreenReader::Speak(std::to_string(left), true);
	}
}

bool Speakable(const UiText::Node& node) {
	if (node.text.empty() || !node.enabled) return false;
	switch (node.kind) {
	case UiText::Kind::Text:
	case UiText::Kind::LinkedText:
	case UiText::Kind::ScrollingText:
	case UiText::Kind::Legend:
	case UiText::Kind::Button:
	case UiText::Kind::Focusable:
		return true;
	default:
		return false;
	}
}

// Rows of options, or of anything else to step through: more than a message to be read out.
bool HasRows(const UiText::Snapshot& snapshot) {
	return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(), [](const auto& node) {
		return node.kind == UiText::Kind::ValueControl || node.kind == UiText::Kind::Toggle || node.kind == UiText::Kind::ListItem;
	});
}

bool HasInventory(const UiText::Snapshot& snapshot) {
	return std::any_of(snapshot.nodes.begin(), snapshot.nodes.end(), [](const auto& node) {
		return node.kind == UiText::Kind::InventoryGrid || node.kind == UiText::Kind::InventorySlot
			|| node.kind == UiText::Kind::InventoryItem;
	});
}

// A tab bar's captions belong to the tab reader, which names the selected one
// as the player moves along the bar. Read here they are every caption at once
// with no selection -- the quest log's act tabs came out as "I, II, III, IV,
// V". They are the bar's own Text<N> children, the same boxes
// NativeUi::SelectedTabText reads.
bool UnderTabBar(const UiText::Snapshot& snapshot, int parent) {
	for (int at = parent; at >= 0 && static_cast<size_t>(at) < snapshot.nodes.size();
	     at = snapshot.nodes[static_cast<size_t>(at)].parent) {
		if (snapshot.nodes[static_cast<size_t>(at)].kind == UiText::Kind::Tabs) return true;
	}
	return false;
}

// A confirmation modal's legends draw only the button bound to their action, select on
// Confirm and back on Dismiss (Controller/ConfirmationModalHD.json), and hold no text:
// deleting a character said "press and hold" and never which button (the user,
// 2026-10-21). The pad's buttons for those actions, A and B (docs/controller-glyphs.md).
constexpr uint32_t SelectGlyph = 0xE00F, BackGlyph = 0xE010;
uint32_t LegendGlyph(const UiText::Snapshot& snapshot, size_t button) {
	for (size_t at = button + 1; at < snapshot.nodes.size(); ++at) {
		const auto& node = snapshot.nodes[at];
		if (node.parent != static_cast<int>(button) || node.kind != UiText::Kind::Legend || !node.text.empty()) continue;
		if (node.name == "ConfirmLegend") return SelectGlyph;
		if (node.name == "DismissLegend") return BackGlyph;
	}
	return 0;
}

// The panels open at the last tick, to tell the ones a burst of messages just opened.
std::vector<std::string> g_seenOpen;

// The top panel, or a menu the same burst opened under overlays: the help screen opens
// HelpPanelBackground and MiniMenuPanel over itself (log of 2026-11-02), which left it unread.
std::string ReadablePanel() {
	const auto& open = PanelState::OpenPanels();
	std::string active(PanelState::Active());
	if (!active.empty() && PanelState::NotMenu(active)) {
		const auto menu = std::find_if(open.rbegin(), open.rend(), [](const std::string& panel) { return !PanelState::NotMenu(panel); });
		if (menu != open.rend() && std::ranges::find(g_seenOpen, *menu) == g_seenOpen.end()) active = *menu;
	}
	g_seenOpen = open;
	return active;
}

void Append(std::string& phrase, const std::string& part) {
	if (part.empty()) return;
	if (!phrase.empty()) phrase += ", ";
	phrase += part;
}

}

void PollTitlePrompt(const D2RL::PluginContext* context, const std::string& active) {
	const uint64_t now = GetTickCount64();
	if (now - g_titlePolled < TitlePollMs) return;
	g_titlePolled = now;
	const uintptr_t panel = NativeWidgets::FindPanel(active);
	const uintptr_t prompt = panel ? NativeWidgets::FindChild(panel, TitlePrompt) : 0;
	if (!prompt || !NativeUi::Visible(prompt)) return;
	const std::string raw = NativeWidgets::ReadText(prompt);
	std::string text = UiText::Clean(raw);
	if (text.empty()) return;
	// The controller prompt's button is a glyph the cleaner drops; the keyboard
	// string reads whole.
	if (raw.find(ControllerGlyph) != std::string::npos) {
		if (const std::string whole = UiText::Clean(Localization::Text(TitleKeyboardKey)); !whole.empty()) text = whole;
	}
	g_titleSpoken = true;
	AsyncLog::InfoF(context, "[modal] %s/%s -> \"%s\"", active.c_str(), std::string(TitlePrompt).c_str(), text.c_str());
	ScreenReader::Speak(text, false);
}

void SetKeyHold(bool held) { g_keyHolds = held; }
void SetKeyMenu(bool engaged) { g_keyMenu = engaged; }

int HoldCountdown(uint64_t heldMs) {
	const uint64_t whole = heldMs / 1000;
	return whole >= static_cast<uint64_t>(DeleteHoldSeconds) ? 0 : DeleteHoldSeconds - static_cast<int>(whole);
}

void Reset() { g_lastPanel.clear(); g_titleSpoken = g_selfReadSpoken = false; }

void SetTitleAnnouncement(std::string text) { g_titleAnnouncement = std::move(text); }

bool MenuNews() { return g_menuNews.load(); }
void SetMenuNews(bool on) { g_menuNews = on; }

void Tick(const D2RL::PluginContext* context) {
	const std::string active = ReadablePanel();
	if (active == g_lastPanel) {
		if (active == TitlePanel && !g_titleSpoken) PollTitlePrompt(context, active);
		if (active == TitlePanel) PassTitle(context, GetTickCount64());
		if (const auto* screen = SelfRead(active); screen && !g_selfReadSpoken) PollSelfRead(context, *screen);
		if (active == DeletePanel) PollDeleteHold(context);
		return;
	}
	g_deleteHeldSince = 0;
	g_deleteSaid = 0;
	g_deleteFace.clear();
	if (g_lastPanel == TitlePanel && g_titlePadPress)
		AsyncLog::InfoF(context, "[modal] title: the game took the pad press in %llu ms (now %s)",
			static_cast<unsigned long long>(GetTickCount64() - g_titlePadPress), active.c_str());
	g_lastPanel = active;
	g_titleSpoken = g_selfReadSpoken = false;
	g_selfReadPolled = g_titlePadPress = 0;
	if (active.empty()) return;
	if (active == MenuLinksPanel && !g_menuNews.load()) {
		AsyncLog::InfoF(context, "[modal] %s: main menu news off", active.c_str());
		return;
	}
	if (active == TitlePanel && !g_titleAnnouncement.empty()) {
		AsyncLog::InfoF(context, "[modal] title: plugin announcement \"%s\"", g_titleAnnouncement.c_str());
		ScreenReader::Speak(std::exchange(g_titleAnnouncement, {}), false);
	}
	if (const auto* screen = SelfRead(active)) {
		PollSelfRead(context, *screen);
		return;
	}

	// The character panel has its own reader, which names the character behind
	// the title and keeps every statistic on the right stick. Read from here it
	// was thirty-odd bare labels and numbers in one breath (MEASURED
	// 2026-09-21), cut off in turn by the tab reader.
	if (CharacterStatsReader::Owns(active) || std::ranges::find(OwnReaders, active) != std::end(OwnReaders)) {
		AsyncLog::InfoF(context, "[modal] %s is read by its own reader", active.c_str());
		return;
	}

	// A panel the player can navigate is the focus reader's job; this is only
	// for the ones the game opens without giving anything focus.
	if (NativeUi::GameFocus().widget != 0) return;

	const uintptr_t panel = NativeWidgets::FindPanel(active);
	if (panel == 0) return;

	// Read the LIVE tree, not the catalogue. The catalogue is generated from
	// the layout JSON and can simply lack a control: DeleteCharacterModal is
	// catalogued as Confirm, ConfirmLegend, Dismiss and DismissLegend, with no
	// entry at all for Prompt -- the TextBoxWidget that holds the message. A
	// catalogue-driven read could never reach it, however many widget types it
	// was taught to look for, which is why this panel stayed silent while
	// reporting that it had found both of its legends.
	const UiText::Snapshot snapshot = NativeUi::Snapshot(panel, 0);
	// Inventory panels briefly lose focus when tabs change; their labels are
	// not a modal message. The shared item reader will announce the next focus.
	if (HasInventory(snapshot)) return;
	// In keyboard mode nothing has the game's focus, and the options came out in one breath,
	// a thousand characters, before the cursor could name their first row (2026-11-04).
	if (g_keyMenu && HasRows(snapshot)) {
		AsyncLog::InfoF(context, "[modal] %s is read by the keyboard's cursor", active.c_str());
		return;
	}
	std::string            phrase, legends;
	for (size_t index = 0; index < snapshot.nodes.size(); ++index) {
		const UiText::Node& node = snapshot.nodes[index];
		if (!Speakable(node) || UnderTabBar(snapshot, node.parent)) continue;
		// Control help names its button by a controller symbol, which Clean alone drops: the help
		// screen's "Hold [<button>] to run". A legend's symbol is its button, said apart.
		auto text = node.kind == UiText::Kind::Text ? ControllerText::Read(node.text) : UiText::Clean(node.text);
		if (text.empty()) continue;
		if (node.kind == UiText::Kind::Button)
			if (const auto glyph = LegendGlyph(snapshot, index)) text = std::string(ControllerText::Button(glyph)) + ": " + text;
		AsyncLog::InfoF(context, "[modal] %s/%s (%s) -> \"%s\"", active.c_str(), node.name.c_str(), node.type.c_str(), text.c_str());
		// A button legend is a control hint: held for right stick down, and
		// spoken unasked only last, with autodetail and control tooltips on.
		Append(node.kind == UiText::Kind::Legend ? legends : phrase, text);
	}
	ControlHints::Offer(legends);
	if (phrase.empty() && legends.empty()) {
		AsyncLog::InfoF(context, "[modal] %s: %zu nodes, none with speakable text", active.c_str(), snapshot.nodes.size());
		return;
	}
	// Queued, never interrupting: this is what the panel says AFTER its title.
	// The window announcer speaks that title the moment the panel opens, with
	// interrupt, so nothing stale is left ahead of this -- cutting in here only
	// ever cut the title off. MEASURED 2026-09-21: "Quest Log" was interrupted
	// 42 ms after it began by this reader speaking the act tabs' "I".
	if (!phrase.empty()) ScreenReader::Speak(phrase, false);
	ControlHints::SpeakAfter(context, legends);
}

}
