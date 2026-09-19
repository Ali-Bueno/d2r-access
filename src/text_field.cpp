#include "text_field.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"

#include <cstdint>
#include <iterator>
#include <string>

namespace D2RAccess::TextField {

namespace {

// The editable archetypes. `ConsoleInput` is the loader's own console, which
// is classified the same way and must not be read back to the player.
constexpr const char* EditableTypes[]  = {"InputTextBoxWidget"};
constexpr const char* IgnoredNames[]   = {"ConsoleInput"};

uintptr_t   g_widget   = 0;
bool        g_editable = false;
std::string g_text;

bool Matches(const std::string& value, const char* const* list, size_t count) {
	for (size_t at = 0; at < count; ++at) {
		if (value == list[at]) return true;
	}
	return false;
}

std::string Read(uintptr_t widget) {
	const NativeMemory::ReadScope scope;
	return NativeWidgets::ReadText(widget);
}

}

void Reset() {
	g_widget   = 0;
	g_editable = false;
	g_text.clear();
}

void Tick(const D2RL::PluginContext* context) {
	const NativeUi::Focus focus = NativeUi::CurrentFocus();
	if (focus.widget == 0) {
		Reset();
		return;
	}

	if (focus.widget != g_widget) {
		g_widget = focus.widget;
		const NativeUi::Type type = NativeUi::Classify(focus.widget);
		const std::string    name = NativeWidgets::ReadName(focus.widget);
		g_editable = Matches(type.name, EditableTypes, std::size(EditableTypes))
			&& !Matches(name, IgnoredNames, std::size(IgnoredNames));
		g_text = g_editable ? Read(focus.widget) : std::string {};
		// Arriving on a field: say what it already holds, after the focus
		// tracker's own "<label>, text field", so the order is what the
		// control is and then what is in it. An empty box adds nothing --
		// the label already said everything there is to say.
		if (g_editable && !g_text.empty()) {
			AsyncLog::InfoF(context, "[field] %s holds \"%s\"", name.c_str(), g_text.c_str());
			ScreenReader::Speak(g_text, false);
		}
		return;
	}
	if (!g_editable) return;

	const std::string current = Read(g_widget);
	if (current == g_text) return;

	const bool deleted  = current.size() < g_text.size() && g_text.compare(0, current.size(), current) == 0;
	const bool appended = current.size() > g_text.size() && current.compare(0, g_text.size(), g_text) == 0;
	if (deleted) {
		// Exactly what the backspace took, which is the one thing the player
		// has no other way to know. Queued rather than interrupting, so
		// holding backspace still names every character instead of leaving
		// only the last one audible.
		const std::string removed = g_text.substr(current.size());
		AsyncLog::InfoF(context, "[field] removed \"%s\"", removed.c_str());
		ScreenReader::Speak(removed, false);
	} else if (!appended) {
		// Neither a clean insertion nor a clean deletion: the box was replaced
		// wholesale, so read the result rather than trying to describe an edit.
		AsyncLog::InfoF(context, "[field] now \"%s\"", current.c_str());
		ScreenReader::Speak(current, true);
	}
	g_text = current;
}

}
