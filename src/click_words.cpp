#include "click_words.h"

#include <array>
#include <atomic>
#include <span>

namespace D2RAccess::ClickWords {
namespace {
constexpr std::string_view Left = "A", Right = "D";
struct Word { std::string_view words, key; };

// How ui.json names the two clicks in each language (InventoryItemTooltip*,
// UseItem, OpenHoradricCube, ReadScroll, PCCycleComparisonTooltips). Where one
// naming begins another the longer comes first. Korean joins the click to what
// follows with a particle that depends on the sound before it: the key's letter
// ends in a vowel where the click's word ends in a consonant.
constexpr Word English[] = {{"Left Click", Left}, {"Right Click", Right}};
constexpr Word German[] = {{"Linksklick", Left}, {"Rechtsklick", Right}};
constexpr Word Spanish[] = {{"clic izquierdo", Left}, {"Haz clic derecho", Right}, {"clic derecho", Right}};
constexpr Word French[] = {{"clic gauche", Left}, {"clic droit", Right}};
constexpr Word Italian[] = {{"pulsante sinistro del mouse", Left}, {"Clicca col destro", Right}};
constexpr Word Japanese[] = {{"左クリック", Left}, {"右クリック", Right}};
constexpr Word Korean[] = {{"왼쪽 클릭으로", "A로"}, {"오른쪽 클릭으로", "D로"}, {"왼쪽 클릭", Left}, {"오른쪽 클릭", Right}};
constexpr Word Polish[] = {{"LPM", Left}, {"PPM", Right}};
constexpr Word Portuguese[] = {{"Clique com o botão direito", Right}, {"Clique Esquerdo", Left}, {"clique", Left}};
constexpr Word Russian[] = {{"ЛКМ", Left}, {"ПКМ", Right}};
constexpr Word SimplifiedChinese[] = {{"左键点击", Left}, {"右键点击", Right}};
constexpr Word TraditionalChinese[] = {{"左鍵點擊", Left}, {"點擊滑鼠右鍵", Right}, {"右鍵點擊", Right}, {"右鍵", Right}};

// Strings::Language order.
constexpr std::array<std::span<const Word>, Strings::LanguageCount> Words{
	English, German, Spanish, Spanish, French, Italian, Japanese, Korean, Polish, Portuguese, Russian,
	SimplifiedChinese, TraditionalChinese};

std::atomic<bool> g_keyboard{false};

char Lower(char letter) { return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter; }

// The game writes the same words with and without a capital ("Clic derecho",
// "Haz clic derecho"). Only the letters of ASCII are folded: the bytes of any
// other letter are compared as they are.
bool StartsWith(std::string_view text, std::string_view words) {
	if (text.size() < words.size()) return false;
	for (size_t i = 0; i < words.size(); ++i)
		if (Lower(text[i]) != Lower(words[i])) return false;
	return true;
}
}

std::string WithKeys(std::string_view text, Strings::Language language) {
	const auto index = static_cast<size_t>(language);
	if (index >= Words.size()) return std::string(text);
	std::string result;
	result.reserve(text.size());
	for (size_t at = 0; at < text.size();) {
		const Word* found = nullptr;
		for (const auto& word : Words[index]) {
			if (!StartsWith(text.substr(at), word.words)) continue;
			found = &word;
			break;
		}
		if (!found) { result += text[at++]; continue; }
		result += found->key;
		at += found->words.size();
	}
	return result;
}

std::string Spoken(std::string_view text) {
	return g_keyboard.load() ? WithKeys(text, Strings::Selected()) : std::string(text);
}

void SetKeyboard(bool keyboard) { g_keyboard = keyboard; }
}
