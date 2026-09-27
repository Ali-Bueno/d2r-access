#include "click_words.h"
#include "test_support.h"

using namespace D2RAccess;
using Strings::Language;

int main() {
	// Every line below is the game's own, from ui.json.
	Check(ClickWords::WithKeys("Ctrl + Left Click to Sell", Language::enUS) == "Ctrl + A to Sell", "a left click is A");
	Check(ClickWords::WithKeys("Shift + Left Click to Equip", Language::enUS) == "Shift + A to Equip", "with Shift");
	Check(ClickWords::WithKeys("Ctrl + Shift + Left Click to Move to Horadric Cube", Language::enUS)
		== "Ctrl + Shift + A to Move to Horadric Cube", "with both");
	Check(ClickWords::WithKeys("Right Click to Use", Language::enUS) == "D to Use", "a right click is D");
	Check(ClickWords::WithKeys("Ctrl + Left Click to Move (Right Click to Move 5)", Language::enUS)
		== "Ctrl + A to Move (D to Move 5)", "both clicks in one line");
	Check(ClickWords::WithKeys("Defense: 15", Language::enUS) == "Defense: 15", "a line that names no click is left as it is");
	Check(ClickWords::WithKeys("", Language::enUS).empty(), "nothing stays nothing");

	Check(ClickWords::WithKeys("Mit STRG + Linksklick verkaufen", Language::deDE) == "Mit STRG + A verkaufen", "German, left");
	Check(ClickWords::WithKeys("Mit Rechtsklick benutzen", Language::deDE) == "Mit D benutzen", "German, right");
	Check(ClickWords::WithKeys("Haz clic derecho para utilizar", Language::esES) == "D para utilizar", "Spanish: the longer naming first");
	Check(ClickWords::WithKeys("Clic derecho para comprar", Language::esES) == "D para comprar", "Spanish: with a capital");
	Check(ClickWords::WithKeys("Ctrl + clic izquierdo para vender", Language::esMX) == "Ctrl + A para vender", "Mexican Spanish");
	Check(ClickWords::WithKeys("Maj + clic gauche pour équiper", Language::frFR) == "Maj + A pour équiper", "French, left");
	Check(ClickWords::WithKeys("Clic droit pour utiliser", Language::frFR) == "D pour utiliser", "French, right");
	Check(ClickWords::WithKeys("Ctrl + pulsante sinistro del mouse per vendere", Language::itIT) == "Ctrl + A per vendere", "Italian, left");
	Check(ClickWords::WithKeys("Clicca col destro per usare", Language::itIT) == "D per usare", "Italian, right");
	Check(ClickWords::WithKeys("Shift+左クリックで装備", Language::jaJP) == "Shift+Aで装備", "Japanese, left");
	Check(ClickWords::WithKeys("右クリックで使う", Language::jaJP) == "Dで使う", "Japanese, right");
	Check(ClickWords::WithKeys("Ctrl + 왼쪽 클릭으로 판매", Language::koKR) == "Ctrl + A로 판매", "Korean: the particle follows the key");
	Check(ClickWords::WithKeys("Ctrl + 왼쪽 클릭", Language::koKR) == "Ctrl + A", "Korean: the click alone");
	Check(ClickWords::WithKeys("오른쪽 클릭으로 사용", Language::koKR) == "D로 사용", "Korean, right");
	Check(ClickWords::WithKeys("Ctrl + LPM, aby sprzedać", Language::plPL) == "Ctrl + A, aby sprzedać", "Polish, left");
	Check(ClickWords::WithKeys("Naciśnij PPM, aby użyć", Language::plPL) == "Naciśnij D, aby użyć", "Polish, right");
	Check(ClickWords::WithKeys("Shift + clique para equipar", Language::ptBR) == "Shift + A para equipar", "Portuguese: a click unnamed is the left");
	Check(ClickWords::WithKeys("Ctrl + Clique Esquerdo", Language::ptBR) == "Ctrl + A", "Portuguese, left");
	Check(ClickWords::WithKeys("Clique com o botão direito para usar", Language::ptBR) == "D para usar", "Portuguese, right");
	Check(ClickWords::WithKeys("Продать – Ctrl + ЛКМ", Language::ruRU) == "Продать – Ctrl + A", "Russian, left");
	Check(ClickWords::WithKeys("Использовать – ПКМ", Language::ruRU) == "Использовать – D", "Russian, right");
	Check(ClickWords::WithKeys("Ctrl+左键点击进行出售", Language::zhCN) == "Ctrl+A进行出售", "Simplified Chinese, left");
	Check(ClickWords::WithKeys("右键点击使用", Language::zhCN) == "D使用", "Simplified Chinese, right");
	Check(ClickWords::WithKeys("Ctrl + 左鍵點擊以販售", Language::zhTW) == "Ctrl + A以販售", "Traditional Chinese, left");
	Check(ClickWords::WithKeys("點擊滑鼠右鍵使用", Language::zhTW) == "D使用", "Traditional Chinese, right, the long naming");
	Check(ClickWords::WithKeys("右鍵可切換", Language::zhTW) == "D可切換", "Traditional Chinese, right, the short naming");

	// With a pad, or with the mouse itself, the game's words stand.
	ClickWords::SetKeyboard(false);
	Check(ClickWords::Spoken("Right Click to Use") == "Right Click to Use", "the game's words stand while the keyboard does not have the menu");
	ClickWords::SetKeyboard(true);
	Strings::Select(Language::enUS);
	Check(ClickWords::Spoken("Right Click to Use") == "D to Use", "the key is named while it does");
	Passed();
	return 0;
}
