#include "text_field.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_skill_navigation.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "text_field_edit.h"

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace D2RAccess::TextField {

namespace {

// The editable archetypes. `ConsoleInput` is the loader's own console, which
// is classified the same way and must not be read back to the player.
constexpr const char* EditableTypes[]  = {"InputTextBoxWidget", "NumericInputTextBoxWidget", "AutocompleteInputTextBoxWidget"};
constexpr const char* IgnoredNames[]   = {"ConsoleInput"};
// The chat box (ChatPanelHD.json), said as it opens and closes: nothing on screen
// takes focus with it (a tester, 2026-10-14: "/players 8" typed blind).
constexpr std::string_view ChatPanel = "ChatPanel";

uintptr_t   g_widget   = 0;
bool        g_editable = false;
bool        g_chat     = false;
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
	g_chat     = false;
	g_text.clear();
}

void Tick(const D2RL::PluginContext* context) {
	NativeUi::Focus focus = NativeUi::CurrentFocus();
	uintptr_t owner = 0;
	if (const uintptr_t editor = NativeSkillNavigation::ActiveTextInput(focus.panel)) focus.widget = editor;
	else if (const uintptr_t any = NativeSkillNavigation::AnyActiveTextInput(owner)) focus = {owner, any};
	if (focus.widget != g_widget && g_chat) {
		AsyncLog::Info(context, "[field] chat closed");
		ScreenReader::Speak(Strings::Current().chatClosed, false);
	}
	if (focus.widget == 0) {
		Reset();
		return;
	}

	if (focus.widget != g_widget) {
		g_widget = focus.widget;
		g_chat = false;
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
		if (g_editable && NativeWidgets::ReadName(focus.panel) == ChatPanel) {
			g_chat = true;
			ScreenReader::Speak(Strings::Current().chatOpened, true);
		}
		return;
	}
	if (!g_editable) return;

	const std::string current = Read(g_widget);
	if (current == g_text) return;

	const Edit edit = Classify(g_text, current);
	if (edit.kind == EditKind::Removed) {
		// Interrupting: what was just deleted matters more than the previous
		// announcement, and a backlog lags behind the cursor.
		AsyncLog::InfoF(context, "[field] removed \"%s\"", edit.text.c_str());
		ScreenReader::Speak(edit.text, true);
	} else if (edit.kind == EditKind::Added) {
		AsyncLog::InfoF(context, "[field] added \"%s\"", edit.text.c_str());
		ScreenReader::Speak(edit.text == " " ? Strings::Current().spaceKey : edit.text, true);
	} else {
		// The box was replaced wholesale: read the result, not the edit.
		AsyncLog::InfoF(context, "[field] now \"%s\"", current.c_str());
		ScreenReader::Speak(current, true);
	}
	g_text = current;
}

}
