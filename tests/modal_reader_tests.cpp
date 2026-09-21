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

void Node(int parent, UiText::Kind kind, std::string name, std::string text = {}) {
	tree.nodes.push_back({static_cast<uintptr_t>(tree.nodes.size() + 1), parent, kind, std::move(name), {}, std::move(text)});
}
}

namespace D2RAccess::PanelState { std::string_view Active() { return active; } }
namespace D2RAccess::NativeUi {
Focus CurrentFocus() { return {}; } // these panels open with nothing focused
UiText::Snapshot Snapshot(uintptr_t, uintptr_t) { return tree; }
}
namespace D2RAccess::NativeWidgets { uintptr_t FindPanel(std::string_view) { return Panel; } }
namespace D2RAccess::ScreenReader { void Speak(std::string_view text, bool interrupt) { speech.push_back({std::string(text), interrupt}); } }
namespace D2RAccess::AsyncLog { void InfoF(const D2RL::PluginContext*, const char*, ...) {} }

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
	Passed();
}
