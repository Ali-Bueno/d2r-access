#include "tab_highlight.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "screen_reader.h"
#include "ui_text.h"

#include <string>

namespace D2RAccess::TabHighlight {
namespace {
uintptr_t   g_strip = 0;   // the strip the highlight was last read from
std::string g_text;        // and the tab it was on
}

void Tick(const D2RL::PluginContext* context, bool uiOwnsSpeech) {
	if (uiOwnsSpeech) return;
	const NativeMemory::ReadScope scope;
	const NativeUi::Focus focus = NativeUi::Focus();
	const bool onStrip = focus.widget != 0 && NativeUi::Classify(focus.widget).kind == UiText::Kind::Tabs;
	if (!onStrip) { Reset(); return; }
	// SelectedTabText resolves the strip's selected index to that tab's own
	// label, so the text changing is the highlight having moved.
	const std::string text = UiText::Clean(NativeUi::SelectedTabText(focus.widget));
	if (focus.widget != g_strip) {
		// Landing on the strip is already announced by the focus tracker
		// (d497e64), so the first reading is only a baseline.
		g_strip = focus.widget;
		g_text = text;
		AsyncLog::InfoF(context, "[strip] on %p -> \"%s\"", reinterpret_cast<void*>(focus.widget), text.c_str());
		return;
	}
	if (text == g_text) return;
	// Logged whether or not there is anything to say: a strip whose highlight
	// the D-pad moves without moving the selected index produces no line here
	// at all, and that absence is the measurement that says the highlight is
	// not the selected index.
	AsyncLog::InfoF(context, "[strip] \"%s\" -> \"%s\"", g_text.c_str(), text.c_str());
	g_text = text;
	if (text.empty()) return;
	// A highlight move is a context change and the player is waiting on it.
	ScreenReader::Speak(text, true);
}

void Reset() {
	g_strip = 0;
	g_text.clear();
}

}
