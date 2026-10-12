#include "cairn_scroll_service.h"
#include "native_cairn.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"

#include <optional>
#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
std::vector<std::string> panels;
std::vector<std::string> speech;
std::optional<CairnStones::Order> order;
uint64_t orderSession = 1;
bool visible = false, available = true, interrupted = true;
constexpr uintptr_t ScrollPanel = 0x1234;

void Tick(uint64_t session = 1, bool ready = true, bool foreground = true) {
	CairnScrollService::Tick(nullptr, session, ready, foreground);
}
}

namespace D2RAccess::NativeCairn {
bool Available() { return available; }
std::optional<CairnStones::Order> Read(uint64_t session) {
	return session == orderSession ? order : std::nullopt;
}
}
namespace D2RAccess::NativeWidgets {
uintptr_t FindPanel(std::string_view name) {
	return name == "ScrollOfInifuss" ? ScrollPanel : 0;
}
}
namespace D2RAccess::NativeUi { bool Visible(uintptr_t panel) { return panel == ScrollPanel && visible; } }
namespace D2RAccess::PanelState { const std::vector<std::string>& OpenPanels() { return panels; } }
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) {
	speech.emplace_back(text);
	interrupted = interrupt;
}
}
namespace D2RAccess::AsyncLog { void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {} }

int main() {
	Strings::Select(Strings::Language::esES);
	CairnScrollService::Reset();
	order = CairnStones::Order{2, 4, 0, 1, 3};
	Tick();
	Check(speech.empty(), "closed scroll does not reveal the sequence");
	panels = {"ScrollOfInifuss"};
	Tick();
	Check(speech.empty(), "a panel listed as open must also be visible");
	visible = true;
	order.reset();
	Tick();
	Check(speech.empty(), "open scroll waits for its packet");
	order = CairnStones::Order{2, 4, 0, 1, 3};
	Tick(1, true, false);
	Check(speech.empty(), "an unfocused game defers the reading");
	Tick();
	Check(speech.size() == 1 && speech.back() ==
		"Orden de las piedras: gota hacia abajo, cero, gota hacia arriba, uno, dos" && !interrupted,
		"the visible scroll reads the exact localized sequence after the panel title");
	Tick();
	Check(speech.size() == 1, "one opening gets one sequence announcement");
	visible = false;
	Tick();
	visible = true;
	Tick();
	Check(speech.size() == 1, "temporary visibility changes do not count as reopening");
	panels.clear();
	Tick();
	Check(speech.size() == 1, "closing the scroll is silent");
	panels = {"ScrollOfInifuss"};
	Tick();
	Check(speech.size() == 2 && speech.back() == speech.front(), "reopening repeats the full sequence");
	orderSession = 2;
	order.reset();
	Tick(2);
	Check(speech.size() == 2, "a new game cannot speak the prior order");
	order = CairnStones::Order{0, 1, 2, 3, 4};
	Tick(2);
	Check(speech.size() == 3 && speech.back() ==
		"Orden de las piedras: gota hacia arriba, uno, gota hacia abajo, dos, cero",
		"a new game's packet supplies its own sequence");
	CairnScrollService::Reset();
}
