#include "native_progression.h"
#include "native_controller_help.h"
#include "native_widgets.h"
#include "progression_text.h"
#include "speech_strings.h"
#include "test_support.h"

#include <map>

namespace {
using namespace D2RAccess;
struct Widget {
	uintptr_t parent;
	std::string name, text;
	NativeUi::Type type;
	bool visible = true;
};
std::map<uintptr_t, Widget> widgets;
}
namespace D2RAccess::NativeUi {
bool Visible(uintptr_t widget) {
	for (size_t depth = 0; depth < widgets.size(); ++depth) {
		const auto it = widgets.find(widget);
		if (it == widgets.end() || !it->second.visible) return false;
		if (!it->second.parent) return true;
		widget = it->second.parent;
	}
	return false;
}
uintptr_t Parent(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).parent : 0; }
Type Classify(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).type : Type{}; }
std::vector<uintptr_t> FindControls(uintptr_t parent, UiText::Kind kind) {
	std::vector<uintptr_t> result;
	for (const auto& [id, widget] : widgets) if (widget.parent == parent && widget.type.kind == kind) result.push_back(id);
	return result;
}
}
// No list of skills here: every button is read as the tree reads its own.
namespace D2RAccess::NativeSkillIcons { bool InList(uintptr_t) { return false; } }
// No "Tab<n>" here: a tab is called by its own text, as TabLabel falls back to.
namespace D2RAccess::NativeUi { std::string TabLabel(uintptr_t widget) { return NativeWidgets::ReadText(widget); } }
namespace D2RAccess::NativeControllerHelp { std::string ForFocus(NativeUi::Focus) { return "X: Asignar habilidad"; } }
namespace D2RAccess::NativeWidgets {
std::string ReadName(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).name : ""; }
std::string ReadText(uintptr_t widget) { return widgets.contains(widget) ? widgets.at(widget).text : ""; }
uintptr_t FindChild(uintptr_t parent, std::string_view name) {
	for (const auto& [id, widget] : widgets) {
		if (widget.parent != parent) continue;
		if (widget.name == name) return id;
		if (const auto child = FindChild(id, name)) return child;
	}
	return 0;
}
}
namespace D2RAccess::Localization { std::string Text(std::string_view key) { return key == "minipanelchar" ? "Personaje" : ""; } }
// The game's skill tables, as skilldesc.txt places Salto: page 1, row 3, column 2.
namespace D2RAccess::NativeSkillBar {
std::string ClassSkillName(int page, int row, int column) { return page == 1 && row == 3 && column == 2 ? "Salto" : ""; }
}

int main() {
	using namespace D2RAccess;
	Strings::Select(Strings::Language::esES);
	const NativeUi::Type stat{"CharacterStatLevelUpButtonWidget", UiText::Kind::StatButton};
	widgets = {
		{1, {0, "AnyCharacterPanel", "", {}}},
		{2, {1, "LevelUpInfo", "", {}}},
		{3, {2, "StrengthPointButton", "Gastar punto de estadística", stat}},
		{4, {1, "StrengthTitle", "Fuerza", {}}},
		{5, {1, "StrengthStat", "30", {}}},
		{6, {4, "Tooltip", "Aumenta el daño.", {}}},
		{7, {1, "CombinedPointsInfo", "5 puntos de estadísticas restantes", {}}},
	};
	const auto strength = NativeProgression::Read({1, 3}, stat);
	Check(strength.readout.body.empty(), "the panel's name is the window's to say, not every stat's");
	Check(strength.readout.option == "Fuerza. 30", "a stat reads its label and live value, and nothing else");
	Check(strength.description == "Aumenta el daño." && strength.points == "5 puntos de estadísticas restantes",
		"its explanation and the points left are held apart, for right stick up and BACK");
	Check(strength.readout.focusKey == "3" && !strength.skill, "stat focus identity is kept without classifying it as a skill");
	for (const auto& pair : {std::pair{"Dex", "Destreza"}, {"Vitality", "Vitalidad"}, {"Energy", "Energía"}}) {
		widgets[3].name = std::string(pair.first) + "PointButton";
		widgets[4].name = std::string(pair.first) + "Title"; widgets[4].text = pair.second;
		widgets[5].name = std::string(pair.first) + "Stat";
		Check(NativeProgression::Read({1, 3}, stat).readout.option == std::string(pair.second) + ". 30", "every stat resolves its own companion label");
	}
	widgets[5].text = "31"; widgets[7].text = "4 puntos de estadísticas restantes";
	const auto increased = NativeProgression::Read({1, 3}, stat);
	Check(increased.readout.option.ends_with("31") && increased.points == "4 puntos de estadísticas restantes",
		"spending a point refreshes both live values");
	widgets[5].visible = false;
	Check(NativeProgression::Read({1, 3}, stat).readout.option.find("31") == std::string::npos, "hidden stat values are not guessed");
	widgets[4].visible = false;
	Check(NativeProgression::Read({1, 3}, stat).readout.Empty(), "missing label leaves the generic action fallback available");
	widgets[4].visible = true; widgets[1].visible = false;
	Check(NativeProgression::Read({1, 3}, stat).readout.Empty(), "closed panels do not expose stats");

	const NativeUi::Type button{"ButtonWidget", UiText::Kind::Button}, tab{"TabWidget", UiText::Kind::TabButton};
	widgets = {
		{10, {0, "AnySkillPanel", "", {}}},
		{11, {10, "Tab2", "Habilidades de combate", tab}},
		{12, {11, "SkillButtonContainer", "", {}}},
		{13, {12, "SkillButton1 1,2", "", button}},
		{14, {10, "SkillCounter", "1 elección de habilidad restante", {}}},
	};
	const auto skill = NativeProgression::Read({10, 13}, button);
	Check(skill.skill && skill.readout.body == "Habilidades de combate", "plain buttons acquire skill role from their native tab container");
	Check(skill.readout.coordinates == "Fila 3, columna 2" && skill.points == "1 elección de habilidad restante",
		"the generated cell and live skill points are available, with no position in the branch");
	widgets[15] = {13, "HotkeyText", "\xEE\x80\x8F", {}};
	const auto bound = NativeProgression::Read({10, 13}, button);
	Check(bound.help == "Asignada a A" && bound.controls == "X: Asignar habilidad",
		"the current binding is part of the readout, and the actions are a control hint of their own");
	widgets[15].visible = false;
	const auto unbound = NativeProgression::Read({10, 13}, button);
	Check(unbound.help.empty() && unbound.controls == "X: Asignar habilidad", "hidden former binding is omitted");
	widgets[16] = {12, "SkillButton1 0,0", "", button};
	widgets[17] = {12, "SkillButton1 2,5", "", button, false};
	widgets[13].name = "SkillButton3"; widgets[12].name = "CommonSkillsContainer";
	Check(NativeProgression::Read({10, 13}, button).skill && NativeProgression::Read({10, 13}, button).readout.coordinates.empty(), "common skills remain readable without invented coordinates");
	Check(NativeProgression::Read({10, 13}, button).name.empty(), "a skill outside the class tree has no table name and waits for its tooltip");
	widgets[12].name = "ItemSkillsContainer";
	Check(NativeProgression::Read({10, 13}, button).skill, "item-granted skills use the same reader");
	widgets[12].name = "UnrelatedButtons";
	Check(!NativeProgression::Read({10, 13}, button).skill, "unrelated actions do not become skills");
	widgets[12].name = "SkillButtonContainer"; widgets[11].visible = false;
	Check(!NativeProgression::Read({10, 13}, button).skill, "hidden skill branches are not read");

	for (const auto name : {"SkillButton3", "Other 0,0", "SkillButton1 -1,2", "SkillButton1 0,-1", "SkillButton1 0,3junk", "SkillButton1 0,99999999999999999"})
		Check(ProgressionText::SkillCell(name).x == -1, "malformed or non-grid names have no inferred coordinates");
	Check(ProgressionText::SkillCell("SkillButton2 2,5").y == 5, "last authored skill row is retained");
	const auto blizzard = ProgressionText::ParseSkillButton("SkillButton3 0,4"); // live sorceress cell, skilldesc page 3 row 5 column 1
	Check(blizzard.tree == 3 && blizzard.cell.x == 0 && blizzard.cell.y == 4, "the button name gives the skill page with the cell");
	Check(ProgressionText::ParseSkillButton("SkillButton1 0,3junk").tree == -1, "a malformed name gives no page either");
	const std::string font = "\xEE\x81\xBE";
	const auto raw = "Grito: +5 s por nivel\n" + font + "2Aturdir recibe bonus de:" + font + "1\n\nDuración: 1.2 s\nPrimer nivel\nNivel necesario: 12\ny aumenta tu índice de ataque.\nAturde a tu objetivo\n" + font + "2Aturdir\n\nTodavía no has aprendido esta habilidad";
	const auto description = ProgressionText::SkillDescription(raw);
	Check(description == "Aturdir. Todavía no has aprendido esta habilidad. Aturde a tu objetivo. y aumenta tu índice de ataque. Nivel necesario: 12. Primer nivel. Duración: 1.2 s. Aturdir recibe bonus de: Grito: +5 s por nivel", "skill name leads, every line is set off by a pause, formatting digits are removed");
	Check(ProgressionText::JoinLines({"Damage 5 to 10", "Mana Cost: 3.", "", "Level 2"}) == "Damage 5 to 10. Mana Cost: 3. Level 2", "lines get a full stop unless they end in punctuation, empty ones are skipped");
	Check(ProgressionText::SkillDescription("Lanza el objeto equipado.\n\nLanzamiento") == "Lanzamiento. Lanza el objeto equipado.", "unstyled common skill names also lead their descriptions");
	Check(ProgressionText::SkillDescription("\r\n\n").empty(), "empty formatted tooltip remains empty");
	Check(ProgressionText::SkillDescription("Coste: 2 p.\nNivel actual: 1\n" + font + "2Aullido").starts_with("Aullido. Nivel actual: 1"), "learned skill levels are preserved");
	const auto spoken = ProgressionText::SkillReadout("Porrazo. Descripción larga", "0 puntos", "Asignada a A. X: Asignar habilidad");
	Check(spoken.headline == "Porrazo. 0 puntos. Asignada a A. X: Asignar habilidad", "the skill's name and the state the player can act on are what a move reads");
	Check(spoken.detail == "Descripción larga", "what the skill does waits to be asked for");
	const auto bare = ProgressionText::SkillReadout("Porrazo", "0 puntos", "");
	Check(bare.headline == "Porrazo. 0 puntos" && bare.detail.empty(), "a skill with no explanation has nothing held back");
	Passed();
}
