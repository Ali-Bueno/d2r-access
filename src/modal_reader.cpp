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

// The death screen fills its two boxes after it opens (YouDiedModalHD.json):
// "You have died / Press <button> to continue." and "Death takes its toll of N
// Gold". Its Continue button holds the focus, so it is read here, not left to
// the focus reader, and polled like the title prompt.
// The game's news and links over the main menu; a tester who owns the expansion asked to silence it (2026-10-20).
constexpr std::string_view MenuLinksPanel = "MainMenuLinks";
std::atomic<bool> g_menuNews{true};
constexpr std::string_view DeathPanel = "YouDiedModal";
constexpr std::string_view DeathBoxes[] = {"MainText", "SubText", "ContinueButton"};
bool g_deathSpoken = false;
uint64_t g_deathPolled = 0;

void PollDeath(const D2RL::PluginContext* context, const std::string& active) {
	const uint64_t now = GetTickCount64();
	if (now - g_deathPolled < TitlePollMs) return;
	g_deathPolled = now;
	const uintptr_t panel = NativeWidgets::FindPanel(active);
	if (!panel) return;
	std::string text;
	for (const auto box : DeathBoxes) {
		const uintptr_t child = NativeWidgets::FindChild(panel, box);
		std::string raw = child ? NativeWidgets::ReadText(child) : std::string{};
		for (size_t at = raw.find('\n'); at != std::string::npos; at = raw.find('\n', at + 2)) raw.replace(at, 1, ". ");
		const auto part = ControllerText::Read(raw);
		if (box == DeathBoxes[0] && part.empty()) return; // not filled in yet
		if (part.empty()) continue;
		if (!text.empty() && text.back() != '.') text += '.';
		if (!text.empty()) text += ' ';
		text += part;
	}
	g_deathSpoken = true;
	AsyncLog::InfoF(context, "[modal] %s -> \"%s\"", active.c_str(), text.c_str());
	ScreenReader::Speak(text, true);
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

void Reset() { g_lastPanel.clear(); g_titleSpoken = g_deathSpoken = false; }

void SetTitleAnnouncement(std::string text) { g_titleAnnouncement = std::move(text); }

bool MenuNews() { return g_menuNews.load(); }
void SetMenuNews(bool on) { g_menuNews = on; }

void Tick(const D2RL::PluginContext* context) {
	const std::string active(PanelState::Active());
	if (active == g_lastPanel) {
		if (active == TitlePanel && !g_titleSpoken) PollTitlePrompt(context, active);
		if (active == TitlePanel) PassTitle(context, GetTickCount64());
		if (active == DeathPanel && !g_deathSpoken) PollDeath(context, active);
		return;
	}
	if (g_lastPanel == TitlePanel && g_titlePadPress)
		AsyncLog::InfoF(context, "[modal] title: the game took the pad press in %llu ms (now %s)",
			static_cast<unsigned long long>(GetTickCount64() - g_titlePadPress), active.c_str());
	g_lastPanel = active;
	g_titleSpoken = g_deathSpoken = false;
	g_deathPolled = g_titlePadPress = 0;
	if (active.empty()) return;
	if (active == MenuLinksPanel && !g_menuNews.load()) {
		AsyncLog::InfoF(context, "[modal] %s: main menu news off", active.c_str());
		return;
	}
	if (active == TitlePanel && !g_titleAnnouncement.empty()) {
		AsyncLog::InfoF(context, "[modal] title: plugin announcement \"%s\"", g_titleAnnouncement.c_str());
		ScreenReader::Speak(std::exchange(g_titleAnnouncement, {}), false);
	}
	if (active == DeathPanel) {
		PollDeath(context, active);
		return;
	}

	// The character panel has its own reader, which names the character behind
	// the title and keeps every statistic on the right stick. Read from here it
	// was thirty-odd bare labels and numbers in one breath (MEASURED
	// 2026-09-21), cut off in turn by the tab reader.
	if (CharacterStatsReader::Owns(active)) {
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
	std::string            phrase, legends;
	for (size_t index = 0; index < snapshot.nodes.size(); ++index) {
		const UiText::Node& node = snapshot.nodes[index];
		if (!Speakable(node) || UnderTabBar(snapshot, node.parent)) continue;
		auto text = UiText::Clean(node.text);
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
