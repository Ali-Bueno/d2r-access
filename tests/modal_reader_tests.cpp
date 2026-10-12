#define NOMINMAX
#include <windows.h>

#include "character_stats_reader.h"
#include "control_hints.h"
#include "modal_reader.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "test_support.h"

#include <string>
#include <vector>

namespace {
using namespace D2RAccess;
struct Spoken { std::string text; bool interrupt; };
std::vector<Spoken> speech;
std::string active;
UiText::Snapshot tree;
constexpr uintptr_t Panel = 1;
bool promptShown = false;
std::string promptText;

void Node(int parent, UiText::Kind kind, std::string name, std::string text = {}) {
	tree.nodes.push_back({static_cast<uintptr_t>(tree.nodes.size() + 1), parent, kind, std::move(name), {}, std::move(text)});
}
}

namespace D2RAccess::PanelState { std::string_view Active() { return active; } }
namespace D2RAccess::InputDirection { bool FaceButtonHeld(FaceButton) { return false; } }
namespace D2RAccess::NativeUi {
Focus GameFocus() { return {}; } // these panels open with nothing focused
bool Visible(uintptr_t) { return promptShown; }
UiText::Snapshot Snapshot(uintptr_t, uintptr_t) { return tree; }
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view) { return Panel; }
uintptr_t FindChild(uintptr_t, std::string_view name) { return name == "InputPrompt" ? 7 : 0; }
std::string ReadText(uintptr_t) { return promptText; }
}
namespace D2RAccess::Localization { std::string Text(std::string_view key) { return key == "KeyboardTitleScreenPrompt" ? "Press Any Key to Begin" : ""; } }
namespace D2RAccess::CharacterStatsReader { bool Owns(std::string_view panel) { return panel == "CharacterStatsPanel"; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool interrupt) { speech.push_back({std::string(text), interrupt}); } }
namespace D2RAccess::AsyncLog { void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {} }
bool autoDetail = false;
namespace D2RAccess::AutoDetail { bool Enabled() { return autoDetail; } bool ControlTooltips() { return true; } }

int main() {
	using UiText::Kind;
	// The delete-character modal: a prompt the player answers with a button.
	active = "DeleteCharacterModal";
	Node(-1, Kind::Other, "DeleteCharacterModal");
	Node(0, Kind::Text, "Prompt", "Delete this character?");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 1 && speech[0].text == "Delete this character?", "a focusless panel reads what it shows");
	Check(!speech[0].interrupt,
		"and queues it behind the title the window announcer already spoke, instead of cutting that title off");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 1, "the same panel is read once, not on every tick");

	// The quest log opens with nothing focused too, and the only text on it is
	// the act tab bar's captions (QuestLogPanelExpansionHD.json: textStrings
	// "I" to "V"), built as the bar's Text<N> children.
	active = "QuestLogPanelExpansion";
	tree = {};
	Node(-1, Kind::Other, "QuestLogPanelExpansion");
	Node(0, Kind::Tabs, "Tabs", "I");
	Node(1, Kind::Text, "Text0", "I");
	Node(1, Kind::Text, "Text1", "II");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 1, "tab captions are left to the tab reader instead of being read as a list");
	active = "DeleteCharacterModal";
	tree = {};
	Node(-1, Kind::Other, "DeleteCharacterModal");
	Node(0, Kind::Tabs, "Tabs", "I");
	Node(1, Kind::Other, "Tab0");
	Node(2, Kind::Text, "Text0", "I");
	Node(0, Kind::Text, "Prompt", "Delete this character?");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 2 && speech.back().text == "Delete this character?",
		"only what sits under a tab bar is skipped, however deep, and the rest of the panel still reads");
	active = "CharacterStatsPanel";
	ModalReader::Tick(nullptr);
	Check(speech.size() == 2, "the character panel is left to its own reader");

	// Button legends are control hints: held for right stick down, and spoken
	// unasked only last, with autodetail and control tooltips on.
	active = "ConfirmationModal";
	tree = {};
	Node(-1, Kind::Other, "ConfirmationModal");
	Node(0, Kind::Text, "Prompt", "Leave the game?");
	Node(0, Kind::Legend, "ConfirmLegend", "A: Confirm");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 3 && speech.back().text == "Leave the game?", "with autodetail off the prompt is read without its buttons");
	Check(ControlHints::Current() == "A: Confirm", "and the buttons wait for right stick down");
	autoDetail = true;
	active = "DeleteCharacterModal";
	ModalReader::Tick(nullptr);
	Check(speech.size() == 5 && speech[3].text == "Leave the game?" && speech[4].text == "A: Confirm" && !speech[4].interrupt,
		"with autodetail and control tooltips on the buttons follow the prompt, last and queued");
	for (const auto kind : {Kind::InventoryGrid, Kind::InventorySlot, Kind::InventoryItem}) {
		ModalReader::Reset(); active = "UncataloguedInventoryPanel"; tree = {};
		Node(-1, Kind::Other, active);
		Node(0, kind, "Items");
		Node(0, Kind::Text, "text_i_left", "I");
		Node(0, Kind::Text, "gold_amount", "5470");
		Node(0, Kind::Button, "gold_button", "Drop Gold");
		Node(0, Kind::Text, "HotkeyLabel0", "\xEE\x80\x83");
		ModalReader::Tick(nullptr);
		Check(speech.size() == 5, "a transient focusless inventory never narrates its unrelated labels or actions");
	}
	active = "DeleteCharacterModal"; tree = {};
	Node(-1, Kind::Other, active);
	Node(0, Kind::Text, "Prompt", "Delete this character?");
	Node(0, Kind::Legend, "GlyphOnly", "\xEE\x80\x83");
	Node(0, Kind::Legend, "Confirm", "\xEE\x80\x81 Confirm");
	ModalReader::Tick(nullptr);
	Check(speech.size() == 7 && speech[5].text == "Delete this character?" && speech[6].text == "Confirm"
		&& ControlHints::Current() == "Confirm", "modal prompts retain their words while controller font glyphs are stripped");
	// The title screen's prompt appears after the panel does and is read once.
	active = "TitleScreenPanel"; tree = {};
	Node(-1, Kind::Other, active);
	speech.clear();
	ModalReader::Tick(nullptr);
	Check(speech.empty(), "no prompt yet, nothing to say");
	promptShown = true; promptText = "Press \xEE\x80\x93 to Begin";
	Sleep(300);
	ModalReader::Tick(nullptr);
	Check(speech.size() == 1 && speech[0].text == "Press Any Key to Begin" && !speech[0].interrupt,
		"a controller prompt whose button is a glyph reads the whole keyboard string, queued");
	Sleep(300);
	ModalReader::Tick(nullptr);
	Check(speech.size() == 1, "and only once");
	ModalReader::Reset();
	ModalReader::Tick(nullptr);
	promptText = "Press Any Key to Begin";
	Sleep(300);
	ModalReader::Tick(nullptr);
	Check(speech.size() == 2 && speech[1].text == "Press Any Key to Begin", "a plain prompt is spoken as shown");
	Passed();
}
