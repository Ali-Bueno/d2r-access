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

int main() {
	Strings::Select(Strings::Language::esES);
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
	const auto transient = Readout{ "Tip", "half", "", "", Surface::Tip };
	(void)reader.Update(transient, 1700);
	(void)reader.Update({}, 1800);
	Check(reader.Update(transient, 1900).text.empty(), "empty snapshots restart the settling window");
	Check(reader.Update(transient, 1980).text == "half", "stable text after a gap is delivered");
	Check(Clean("\xEE\x80\x93").empty(), "controller-image tooltip is silent");
	Check(Clean("\xEE\x80\x93 Recoger. Daño: 2–6") == "Recoger. Daño: 2–6", "controller images do not hide real item text or Unicode punctuation");
	reader.Reset();
	const Readout item{"Inventory", "Inventario", "Poción", "0", Surface::Other};
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
	Check(readout.body == "Cementerio" && readout.option.empty(), "new focus retains ownership but never reads the previous quest objective");
	journal.nodes[2].text.clear(); journal.nodes[3].text.clear();
	readout = Describe(journal);
	(void)reader.Update(readout, 4100);
	Check(reader.Update(readout, 4250).text == std::string("Cementerio. ") + Strings::Current().questNoObjective, "a blank journal objective is explicitly reported without inventing progress");
	journal.nodes[2].text = "Cementerio"; journal.nodes[3].text = "Misión completada";
	readout = Describe(journal);
	(void)reader.Update(readout, 4300);
	Check(reader.Update(readout, 4450).text == "Misión completada", "game completion text is read as current progress");
	Passed();
}
