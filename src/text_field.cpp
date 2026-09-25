#include "text_field.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_skill_navigation.h"
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
constexpr const char* EditableTypes[]  = {"InputTextBoxWidget", "NumericInputTextBoxWidget"};
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
	return NativeUi::InputText(widget);
}

}

void Reset() {
	g_widget   = 0;
	g_editable = false;
	g_text.clear();
}

void Tick(const D2RL::PluginContext* context) {
	NativeUi::Focus focus = NativeUi::CurrentFocus();
	if (const uintptr_t editor = NativeSkillNavigation::ActiveTextInput(focus.panel)) focus.widget = editor;
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
		// Arrival is silent: the focus tracker says "<label>, text field,
		// <contents>" as one phrase, so the contents can no longer race it.
		// Logged even when empty and even when not editable: "the box read
		// nothing" and "the box was never recognised" are different faults, and
		// silence cannot tell them apart.
		AsyncLog::InfoF(context, "[field] focus name=\"%s\" type=\"%s\" editable=%d text=\"%s\"",
			name.c_str(), type.name.c_str(), g_editable ? 1 : 0, g_text.c_str());
		return;
	}
	if (!g_editable) return;

	const std::string current = Read(g_widget);
	if (current == g_text) return;

	const bool deleted  = current.size() < g_text.size() && g_text.compare(0, current.size(), current) == 0;
	const bool appended = current.size() > g_text.size() && current.compare(0, g_text.size(), g_text) == 0;
	if (deleted) {
		// Exactly what the backspace took, which is the one thing the player
		// has no other way to know. Interrupting, not queued: while editing,
		// what was just deleted matters more than finishing the previous
		// announcement, and a backlog lags behind the cursor.
		const std::string removed = g_text.substr(current.size());
		AsyncLog::InfoF(context, "[field] removed \"%s\"", removed.c_str());
		ScreenReader::Speak(removed, true);
	} else if (!appended) {
		// Neither a clean insertion nor a clean deletion: the box was replaced
		// wholesale, so read the result rather than trying to describe an edit.
		AsyncLog::InfoF(context, "[field] now \"%s\"", current.c_str());
		ScreenReader::Speak(current, true);
	}
	g_text = current;
}

}
