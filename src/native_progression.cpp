#include "native_progression.h"
#include "native_controller_help.h"
#include "controller_text.h"
#include "localization.h"
#include "native_widgets.h"
#include "progression_text.h"
#include "speech_strings.h"

#include <cstdio>
#include <algorithm>
#include <tuple>

namespace D2RAccess::NativeProgression {
namespace {
void Append(std::string& text, const std::string& part) {
	if (part.empty()) return;
	if (!text.empty()) text += text.ends_with('.') ? " " : ". ";
	text += part;
}

std::string Text(uintptr_t widget) {
	return NativeUi::Visible(widget) ? UiText::Clean(NativeWidgets::ReadText(widget)) : std::string{};
}

std::string ChildText(uintptr_t widget, std::string_view child) {
	return Text(NativeWidgets::FindChild(widget, child));
}

}

Details Read(NativeUi::Focus focus, const NativeUi::Type& type) {
	Details result;
	if (!NativeUi::Visible(focus.panel) || !NativeUi::Visible(focus.widget)) return result;
	const auto name = NativeWidgets::ReadName(focus.widget);
	if (type.kind == UiText::Kind::StatButton) {
		// CharacterStatsPanel{HD,Controller/HD}: each <stat>PointButton uses
		// the sibling <stat>Title and <stat>Stat; stats themselves are live text.
		constexpr std::string_view suffix = "PointButton";
		if (!name.ends_with(suffix)) return result;
		const auto stem = name.substr(0, name.size() - suffix.size());
		const auto title = NativeWidgets::FindChild(focus.panel, stem + "Title");
		result.readout.option = Text(title);
		if (result.readout.option.empty()) return result;
		result.value = ChildText(focus.panel, stem + "Stat");
		result.points = ChildText(focus.panel, "CombinedPointsInfo");
		// The stat and its value, and nothing else on the move (the user,
		// 2026-09-21: "only the statistic highlighted should be read with
		// autodetail off"). Its explanation waits for right stick up or
		// autodetail, the points left for BACK, and the button's own "Spend
		// Stat Point" is what its controls already say.
		Append(result.readout.option, result.value);
		result.description = ChildText(title, "Tooltip");
	} else if (type.kind == UiText::Kind::Button) {
		// Skill-tree class buttons are plain ButtonWidget instances. Their
		// owning containers (including granted/common skills) supply the role.
		const auto container = NativeUi::Parent(focus.widget);
		const auto containerName = NativeWidgets::ReadName(container);
		if (containerName != "SkillButtonContainer" && containerName != "CommonSkillsContainer"
			&& containerName != "ItemSkillsContainer") return result;
		const auto tab = NativeUi::Parent(container);
		if (NativeUi::Classify(tab).kind != UiText::Kind::TabButton) return result;
		result.skill = true;
		result.readout.body = Text(tab);
		result.points = ChildText(focus.panel, "SkillCounter");
		const auto hotkey = NativeWidgets::FindChild(focus.widget, "HotkeyText");
		const auto binding = NativeUi::Visible(hotkey) ? ControllerText::Read(NativeWidgets::ReadText(hotkey)) : std::string{};
		if (!binding.empty()) {
			char phrase[200]{};
			std::snprintf(phrase, sizeof(phrase), Strings::Current().assignedTo, binding.c_str());
			result.help = phrase;
		}
		result.controls = NativeControllerHelp::ForFocus(focus);
		const auto cell = ProgressionText::SkillCell(name);
		if (cell.x >= 0) {
			char phrase[100]{};
			std::snprintf(phrase, sizeof(phrase), Strings::Current().gridCell, cell.y + 1, cell.x + 1);
			result.readout.coordinates = phrase;
		}
	} else return result;
	result.readout.context = NativeWidgets::ReadName(focus.panel);
	result.readout.focusKey = std::to_string(focus.widget);
	return result;
}
}
