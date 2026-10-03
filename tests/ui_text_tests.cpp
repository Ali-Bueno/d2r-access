#include "ui_text.h"
#include "speech_strings.h"
#include "test_support.h"

using namespace D2RAccess;
using namespace D2RAccess::UiText;

Snapshot Conversation() {
	return { 1, 4, "NpcDialogPanel", Surface::Menu, {
		{1, -1, Kind::Other, "NpcDialogPanel", "NpcDialogPanel", ""},
		{2, 0, Kind::Text, "TextTemplate", "TextBoxWidget", "Akara"},
		{3, 0, Kind::List, "ListTemplate", "ListWidget", ""},
		{4, 2, Kind::ListItem, "ListItem0", "HoverListItemWidget", ""},
		{5, 3, Kind::Text, "TextBox", "TextBoxWidget", "Hablar"},
		{6, 2, Kind::ListItem, "ListItem1", "HoverListItemWidget", ""},
		{7, 5, Kind::Text, "TextBox", "TextBoxWidget", "Comerciar"},
		{8, 0, Kind::Button, "ButtonTemplate", "ButtonWidget", "Salir"},
	} };
}

void JournalDetails() {
	Snapshot journal{40, 41, "QuestLogPanelExpansion", Surface::Menu, {
		{40, -1, Kind::Other, "Root", "QuestLogPanel", ""},
		{41, 0, Kind::QuestButton, "Quest3", "QuestLogButtonWidget", "The Search for Cain"},
		{42, 0, Kind::Text, "ShortDescription", "TextBoxWidget", "The Search for Cain"},
		{43, 0, Kind::Text, "LongDescription", "TextBoxWidget", "Go to the Stony Field."},
		{44, 0, Kind::Tabs, "Tabs", "TabBarWidget", "I"},
		{45, 4, Kind::Text, "TabLabel", "TextBoxWidget", "I"},
		{46, 0, Kind::Text, "Title", "TextBoxWidget", "Quest Log"},
		{47, 0, Kind::Button, "Speech", "ButtonWidget", "Play Quest Dialogue"},
		{48, 0, Kind::Text, "ObjectiveCopy", "TextBoxWidget", "Go to the Stony Field."},
		{49, 0, Kind::LinkedText, "Progress", "TextBoxWithLinksWidget", "Find the Cairn Stones."},
		{50, 0, Kind::Text, "ProgressCopy", "TextBoxWidget", "Find the Cairn Stones."},
	}};
	auto readout = Describe(journal);
	Check(readout.body == "The Search for Cain" && readout.option == "Go to the Stony Field. Find the Cairn Stones.",
		"journal keeps all current detail text, without tab labels or repeated objective lines");
	Check(readout.help == "Play Quest Dialogue", "journal includes its visible speech action caption");
	Reader reader;
	reader.Update(readout, 0);
	Check(reader.Update(readout, 150).text == "The Search for Cain. Go to the Stony Field. Find the Cairn Stones.",
		"the quest action is a control hint, never part of the spoken details");
	Check(reader.Repeat() == "The Search for Cain. Go to the Stony Field. Find the Cairn Stones.",
		"and a manual repeat reads the details alone");
	journal.nodes[7].enabled = false;
	Check(Describe(journal).help.empty(), "an unavailable speech button does not promise an action");
	journal.nodes[2].text = "Tools of the Trade";
	readout = Describe(journal);
	Check(readout.body.empty() && readout.option == Strings::Current().slotEmpty && readout.help.empty(),
		"a stale pane supplies neither another quest's objective nor its actions");
}

int main() {
	Strings::Select(Strings::Language::esES);
	JournalDetails();
	Check(Clean("  Akara\r\n Bienvenido\tviajero | hola  ") == "Akara Bienvenido viajero hola", "line wrapping is normalized");
	Check(Clean("\xC3\xBF" "c4Diálogo \xFF" "c0español") == "Diálogo español", "color markup is removed without losing UTF-8");
	Check(Clean("¿Qué tal? 50% <G>") == "¿Qué tal? 50% <G>", "punctuation and literal control hints survive");
	auto snapshot = Conversation();
	auto readout = Describe(snapshot);
	Check(readout.body == "Akara", "NPC name is the menu title");
	Check(readout.option == "Hablar", "only the selected option is read");
	Reader reader;
	Check(reader.Update(readout, 0).text.empty(), "wait for population");
	Check(reader.Update(readout, 79).text.empty(), "settle across frames");
	Check(reader.Update(readout, 80).text == "Akara. Hablar", "title and first option are one utterance");
	Check(reader.Update(readout, 500).text.empty(), "stationary focus stays quiet");
	Check(reader.Repeat() == "Akara. Hablar", "repeat uses the current context");
	snapshot.focus = 6;
	readout = Describe(snapshot);
	Check(reader.Update(readout, 600).text == "Comerciar", "navigation reads the existing new option immediately");
	Check(reader.Update(readout, 680).text.empty(), "immediate menu navigation does not repeat on the next poll");
	snapshot.nodes[6].text = "Identificar objetos";
	readout = Describe(snapshot);
	(void)reader.Update(readout, 700);
	Check(reader.Update(readout, 780).text == "Identificar objetos", "recycled rows refresh without a focus move");
	snapshot.nodes[5].id = snapshot.focus = 60;
	readout = Describe(snapshot);
	Check(reader.Update(readout, 900).text.empty(), "reallocated identical rows do not repeat");
	snapshot.focus = 7;
	Check(Describe(snapshot).option == "Identificar objetos", "child label focus resolves its row");
	snapshot.focus = 3;
	Check(Describe(snapshot).option.empty(), "focusing an unresolved list never enumerates its items");
	snapshot.focus = 8;
	snapshot.nodes[7].enabled = false;
	Check(Describe(snapshot).option == "Salir. no disponible", "disabled button state accompanies its runtime label");
	snapshot.nodes[1].text.clear();
	Check(Describe(snapshot).body == "Conversación", "a menu without a visible heading has a localized title");

	Snapshot narrative{10, 0, "ScrollSpeechPanel", Surface::Narrative, {
		{10, -1, Kind::Other, "ScrollSpeechPanel", "ScrollSpeechPanel", ""},
		{11, 0, Kind::ScrollingText, "ScrollSpeechWidget", "ScrollingSpeechWidget", ""},
		{12, 1, Kind::Text, "ScrollingText", "TextBoxWidget", std::string(6000, 'a') + " final"},
	} };
	readout = Describe(narrative);
	Check(readout.body.size() == 6006, "long scrolling speeches remain complete");
	(void)reader.Update(readout, 1000);
	const auto speech = reader.Update(readout, 1080);
	Check(speech.interrupt && speech.text == readout.body, "entering a speech interrupts the previous menu");
	Check(reader.Update(readout, 1200).text.empty(), "scroll animation does not repeat speech");
	narrative.nodes[2].text = "Otra historia";
	readout = Describe(narrative);
	(void)reader.Update(readout, 1300);
	Check(reader.Update(readout, 1380).text == "Otra historia", "different story in same panel is read");
	readout = Describe(Conversation());
	(void)reader.Update(readout, 1400);
	Check(reader.Update(readout, 1480).text == "Akara. Hablar", "returning from speech restores title and focus");
	reader.Reset();
	Check(reader.Repeat().empty(), "closing or leaving a session forgets stale speech");
	(void)reader.Update(readout, 1500);
	Check(reader.Update(readout, 1580).text == "Akara. Hablar", "reopening the same NPC reads again");

	Snapshot tip{20, 23, "AnyFutureTip", Surface::Tip, {
		{20, -1, Kind::Other, "Root", "TutorialTooltip", ""},
		{21, 0, Kind::Text, "Title", "TextBoxWidget", "Gráficos"},
		{22, 0, Kind::Text, "Body", "TextBoxWidget", "Pulsa G para cambiar"},
		{23, 0, Kind::Button, "Confirm", "ButtonWidget", "Aceptar"},
		{24, 0, Kind::Button, "Dismiss", "ButtonWidget", "Omitir"},
	} };
	readout = Describe(tip);
	Check(readout.body == "Gráficos. Pulsa G para cambiar" && readout.option == "Aceptar", "tutorial includes full text and only focused button");
	(void)reader.Update(readout, 1600);
	Check(reader.Update(readout, 1680).text == "Gráficos. Pulsa G para cambiar. Aceptar", "tutorial reads as a single utterance");
	const auto transient = Readout{ "Tip", "half", "", "", "", Surface::Tip };
	(void)reader.Update(transient, 1700);
	(void)reader.Update({}, 1800);
	Check(reader.Update(transient, 1900).text.empty(), "empty snapshots restart the settling window");
	Check(reader.Update(transient, 1980).text == "half", "stable text after a gap is delivered");
	Check(Clean("\xEE\x80\x93").empty(), "controller-image tooltip is silent");
	// Monster plate, 2026-09-29: "Demonio 5• 0Invoca trinchantes" kept both font indices.
	Check(Clean("Demonio \xEE\x81\xBE" "5\xE2\x80\xA2\xEE\x81\xBE" "0Invoca") == "Demonio \xE2\x80\xA2 Invoca",
		"a font command drops its index character");
	Check(Clean("\xEE\x80\x93 Recoger. Daño: 2–6") == "Recoger. Daño: 2–6", "controller images do not hide real item text or Unicode punctuation");
	reader.Reset();
	const Readout item{"Inventory", "Inventario", "Poción", "0", "", Surface::Other};
	Check(reader.Update(item, 2000, 0).text == "Inventario. Poción", "already rendered descriptions need no additional settling delay");
	Check(reader.Update(item, 2001, 0).text.empty(), "immediate descriptions remain deduplicated");
	reader.Reset();
	Check(reader.Update(item, 2100, 20).text.empty(), "coordinate-only selection allows one poll for item capture");
	Check(reader.Update(item, 2119, 20).text.empty() && reader.Update(item, 2120, 20).text == "Inventario. Poción", "short inventory settle has an explicit latency bound");
	Snapshot journal{30, 31, "QuestLogPanelExpansion", Surface::Menu, {
		{30, -1, Kind::Other, "Root", "QuestLogPanel", ""},
		{31, 0, Kind::QuestButton, "Quest0", "QuestLogButtonWidget", "Guarida del mal"},
		{32, 0, Kind::Text, "ShortDescription", "TextBoxWidget", "Guarida del mal"},
		{33, 0, Kind::Text, "LongDescription", "TextBoxWidget", "Mata a todos los monstruos."},
		{34, 0, Kind::QuestButton, "Quest1", "QuestLogButtonWidget", "Cementerio"},
	}};
	reader.Reset();
	readout = Describe(journal);
	Check(readout.body == "Guarida del mal" && readout.option == "Mata a todos los monstruos.", "quest title and current objective are associated without duplicate titles");
	Check(reader.Update(readout, 3000).text.empty() && reader.Update(readout, 3149).text.empty(), "journal waits for the dependent detail pane");
	Check(reader.Update(readout, 3150).text == "Guarida del mal. Mata a todos los monstruos.", "quest description is read with its title once");
	journal.nodes[3].text = "Monstruos restantes: 3";
	readout = Describe(journal);
	(void)reader.Update(readout, 3200);
	const auto progress = reader.Update(readout, 3350);
	Check(progress.text == "Monstruos restantes: 3" && !progress.interrupt, "same quest progress reads the live counter without repeating its title or interrupting");
	Check(reader.Update(readout, 4000).text.empty(), "unchanged quest progress remains quiet");
	journal.focus = 34;
	readout = Describe(journal);
	Check(readout.option == Strings::Current().slotEmpty, "a pane still holding the previous quest names nothing, not even the new icon");
	(void)reader.Update(readout, 4010);
	Check(reader.Update(readout, 4060).text.empty(), "and stays silent while the pane is still settling");
	// A quest the player has not been given leaves the shared pane empty. Naming
	// the icon from the catalog read out every quest of the act, including ones
	// the character has never been offered; an empty socket is simply not spoken.
	journal.nodes[2].text.clear(); journal.nodes[3].text.clear();
	readout = Describe(journal);
	Check(readout.option == Strings::Current().slotEmpty, "an icon the game describes nothing for is never named from the catalog");
	(void)reader.Update(readout, 4100);
	Check(reader.Update(readout, 4250).text == Strings::Current().slotEmpty,
		"an empty quest socket says it is empty, rather than leaving the panel title to be read instead");
	// Only the pane's own heading decides it: other text the panel happens to
	// hold must not keep an empty socket alive as a quest.
	journal.nodes.push_back(Node{35, 0, Kind::Text, "Counter", "TextBoxWidget", "Monstruos restantes: 3"});
	Check(Describe(journal).option == Strings::Current().slotEmpty, "other panel text does not turn an empty socket into a quest");
	journal.nodes.pop_back();
	journal.nodes[2].text = "Cementerio";
	readout = Describe(journal);
	(void)reader.Update(readout, 4300);
	Check(reader.Update(readout, 4450).text == std::string("Cementerio. ") + Strings::Current().questNoObjective, "a blank objective on a quest the game does describe is still reported without inventing progress");
	journal.nodes[3].text = "Misión completada";
	readout = Describe(journal);
	(void)reader.Update(readout, 4500);
	Check(reader.Update(readout, 4650).text == "Misión completada", "game completion text is read as current progress");
	Passed();
}
