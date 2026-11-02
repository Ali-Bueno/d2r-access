#include "tutorial_text.h"
#include "tutorial_strings/deDE.h"
#include "tutorial_strings/enUS.h"
#include "tutorial_strings/esES.h"
#include "tutorial_strings/frFR.h"
#include "tutorial_strings/itIT.h"
#include "tutorial_strings/jaJP.h"
#include "tutorial_strings/koKR.h"
#include "tutorial_strings/plPL.h"
#include "tutorial_strings/ptBR.h"
#include "tutorial_strings/ruRU.h"
#include "tutorial_strings/zhCN.h"
#include "tutorial_strings/zhTW.h"

#include <array>
#include <utility>

namespace D2RAccess::TutorialText {
namespace {
// The game's controller glyphs (docs/controller-glyphs.md): U+E000 onward in its font's order.
constexpr std::array<std::pair<std::string_view, uint32_t>, 12> Tokens{{
	{"{dpad}", 0xE000}, {"{up}", 0xE001}, {"{menu}", 0xE005}, {"{view}", 0xE006}, {"{ls}", 0xE007}, {"{rs}", 0xE009},
	{"{lb}", 0xE00B}, {"{rb}", 0xE00C}, {"{a}", 0xE00F}, {"{b}", 0xE010}, {"{x}", 0xE011}, {"{y}", 0xE012}}};

void ReplaceAll(std::string& text, std::string_view token, std::string_view with) {
	for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + with.size()))
		text.replace(at, token.size(), with);
}
}

const Texts& For(Strings::Language language) {
	using L = Strings::Language;
	switch (language) {
	case L::deDE: return Tables::deDE;
	case L::esES: case L::esMX: return Tables::esES;
	case L::frFR: return Tables::frFR;
	case L::itIT: return Tables::itIT;
	case L::jaJP: return Tables::jaJP;
	case L::koKR: return Tables::koKR;
	case L::plPL: return Tables::plPL;
	case L::ptBR: return Tables::ptBR;
	case L::ruRU: return Tables::ruRU;
	case L::zhCN: return Tables::zhCN;
	case L::zhTW: return Tables::zhTW;
	default: return Tables::enUS;
	}
}

std::string Render(std::string_view text, const Texts& texts, bool routeOnA, const Names& names,
	const std::function<std::string(uint32_t)>& button) {
	std::string result(text);
	const auto name = [&](std::string_view token, const std::string& game, const char* own) {
		ReplaceAll(result, token, game.empty() ? std::string_view(own) : std::string_view(game));
	};
	name("{den}", names.den, texts.den);
	name("{moor}", names.moor, texts.moor);
	name("{plains}", names.plains, texts.plains);
	name("{portaltome}", names.portalTome, texts.portalTome);
	name("{idtome}", names.identifyTome, texts.identifyTome);
	name("{general}", names.generalSkills, texts.generalSkills);
	name("{trade}", names.trade, texts.trade);
	ReplaceAll(result, "{portal}", names.portal);
	ReplaceAll(result, "{route}", routeOnA ? texts.routeButton : texts.routeStick);
	for (const auto& [token, glyph] : Tokens)
		if (result.find(token) != std::string::npos) ReplaceAll(result, token, button(glyph));
	return result;
}
}
