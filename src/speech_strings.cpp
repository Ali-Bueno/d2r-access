#include "speech_strings.h"

#include "strings/deDE.h"
#include "strings/enUS.h"
#include "strings/esES.h"
#include "strings/esMX.h"
#include "strings/frFR.h"
#include "strings/itIT.h"
#include "strings/jaJP.h"
#include "strings/koKR.h"
#include "strings/plPL.h"
#include "strings/ptBR.h"
#include "strings/ruRU.h"
#include "strings/zhCN.h"
#include "strings/zhTW.h"

namespace D2RAccess::Strings {

namespace {

// Language enum order.
constexpr std::array<const Table*, LanguageCount> AllTables{
	&Tables::enUS, &Tables::deDE, &Tables::esES, &Tables::esMX, &Tables::frFR, &Tables::itIT, &Tables::jaJP,
	&Tables::koKR, &Tables::plPL, &Tables::ptBR, &Tables::ruRU, &Tables::zhCN, &Tables::zhTW};

static_assert(Complete(Tables::enUS) && Complete(Tables::deDE) && Complete(Tables::esES) && Complete(Tables::esMX)
	&& Complete(Tables::frFR) && Complete(Tables::itIT) && Complete(Tables::jaJP) && Complete(Tables::koKR)
	&& Complete(Tables::plPL) && Complete(Tables::ptBR) && Complete(Tables::ruRU) && Complete(Tables::zhCN)
	&& Complete(Tables::zhTW), "every language table must initialize every field");

Language g_selected = Language::enUS;

constexpr std::size_t Index(Language language) { return static_cast<std::size_t>(language); }

}

// data/local/lng/strings/ui.json, id 4143: the one-word "close" label, whose
// translation differs in all thirteen locales (esES/esMX and ptBR share most
// other short labels, e.g. "strCancel" = "CANCELAR" in all three).
const LocaleProbe TextLocaleProbe{
	"close",
	{"close", "Schließen", "Cerrar", "cerrar", "fermer", "Chiudi", "閉じる", "닫기", "zamknij", "fechar", "закрыть", "关闭", "關閉"}};

const Table& Current() { return *AllTables[Index(g_selected)]; }
Language     Selected() { return g_selected; }
void         Select(Language language) { g_selected = language; }
const Table& For(Language language) { return *AllTables[Index(language)]; }

std::string_view LocaleCode(Language language) { return LocaleCodes[Index(language)]; }

std::optional<Language> FromLocale(std::string_view code) {
	for (std::size_t i = 0; i < LanguageCount; ++i) {
		if (LocaleCodes[i] == code) return static_cast<Language>(i);
	}
	return std::nullopt;
}

std::optional<Language> FromProbeValue(std::string_view value) {
	for (std::size_t i = 0; i < LanguageCount; ++i) {
		if (TextLocaleProbe.values[i] == value) return static_cast<Language>(i);
	}
	return std::nullopt;
}

}
