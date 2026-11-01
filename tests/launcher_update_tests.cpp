#include "update_logic.h"
#include "test_support.h"

using namespace D2RAccess::Launcher::Update;

namespace {

void Tags() {
	const auto a = ParseReleaseTag("test-2026-11-01");
	Check(a && a->year == 2026 && a->month == 11 && a->day == 1 && a->number == 1, "a test tag parses, number defaults to 1");
	const auto b = ParseReleaseTag("test-2026-11-01-2");
	Check(b && b->number == 2, "the counter parses");
	const auto c = ParseReleaseTag("v2026.11.02");
	Check(c && c->year == 2026 && c->month == 11 && c->day == 2, "a v-tag parses");
	Check(ParseReleaseTag("v2026.11.02-3")->number == 3, "a v-tag counter parses");
	Check(IsNewer(*b, *a), "the second release of a day is newer");
	Check(!IsNewer(*a, *b) && !IsNewer(*a, *a), "older and equal are not offered");
	Check(IsNewer(*c, *b), "the next day is newer");
	Check(IsNewer(*ParseReleaseTag("v2026.11.02"), *ParseReleaseTag("test-2026-11-01-9")), "a v-tag beats an earlier test tag");
	Check(!IsNewer(*ParseReleaseTag("test-2026-10-31-5"), *a), "an earlier date with a big counter is older");
	for (const char* bad : {"", "test-", "test-2026-11", "test-2026-13-01", "test-2026-11-00", "test-2026-11-01-0", "test-2026-11-01-x",
			"test-2026-11-01-", "v2026-11-01", "2026-11-01", "test-2026-11-01x", "v2026.11.2", "latest"})
		Check(!ParseReleaseTag(bad), std::string("rejected tag: ") + bad);
}

void Extractor() {
	const std::string json = R"({"tag":"test-2026-11-01","asset":"d2r.zip","sha256":"x","notes":"a\"b\\c\ndé中😀 \/ \t"})";
	Check(ExtractJsonString(json, "tag") == "test-2026-11-01", "a plain string");
	Check(ExtractJsonString(json, "notes") == "a\"b\\c\nd\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80 / \t", "escapes and unicode decode to UTF-8");
	Check(!ExtractJsonString(json, "missing"), "a missing key gives nothing");
	Check(ExtractJsonString("  {\n \"a\" : 5, \"b\" : [1,{\"c\":\"no\"}], \"k\" : \"v\" }", "k") == "v", "other values are skipped");
	Check(!ExtractJsonString(R"({"a":{"k":"nested"}})", "k"), "only top-level members count");
	Check(ExtractJsonString(R"({"notes":"has \"tag\": \"fake\"","tag":"real"})", "tag") == "real", "a key inside a string is not a key");
	Check(!ExtractJsonString("", "k") && !ExtractJsonString("[]", "k") && !ExtractJsonString("not json", "k"), "non-objects give nothing");
	Check(!ExtractJsonString(R"({"k":"unterminated)", "k"), "an unterminated string gives nothing");
	Check(!ExtractJsonString(R"({"k":"bad \q escape"})", "k"), "an unknown escape gives nothing");
	Check(!ExtractJsonString(R"({"k":"\u12"})", "k"), "a short \\u escape gives nothing");
	Check(!ExtractJsonString(R"({"k":"\ud83d"})", "k"), "a lone surrogate gives nothing");
}

void Manifests() {
	const std::string hash(64, 'A');
	const std::string good = R"({"tag":"test-2026-11-01-2","asset":"d2r-access-completo.zip","sha256":")" + hash + R"(","notes":"  hello\n "})";
	const auto manifest = ParseManifest(good);
	Check(manifest && manifest->tag == "test-2026-11-01-2" && manifest->asset == "d2r-access-completo.zip" && manifest->sha256 == hash,
		"a good manifest parses");
	Check(manifest && manifest->notes == "hello", "notes are trimmed");
	Check(ParseManifest(R"({"tag":"v2026.11.01","asset":"a.zip","sha256":")" + hash + R"("})")->notes.empty(), "notes are optional");
	Check(!ParseManifest(R"({"tag":"nope","asset":"a.zip","sha256":")" + hash + R"("})"), "a bad tag is rejected");
	Check(!ParseManifest(R"({"tag":"v2026.11.01","asset":"..\\x.zip","sha256":")" + hash + R"("})"), "a path in the asset is rejected");
	Check(!ParseManifest(R"({"tag":"v2026.11.01","asset":"a.zip","sha256":"abc"})"), "a short checksum is rejected");
	Check(!ParseManifest(R"({"tag":"v2026.11.01","asset":"a.zip","sha256":")" + std::string(64, 'g') + R"("})"), "a non-hex checksum is rejected");
	Check(!ParseManifest(R"({"tag":"v2026.11.01"})"), "missing fields are rejected");
	Check(AssetUrl("test-2026-11-01", "a.zip") == "https://github.com/Ali-Bueno/d2r-access/releases/download/test-2026-11-01/a.zip", "the asset URL is pinned to the tag");
	Check(IsPlainFileName("a b.zip") && !IsPlainFileName("") && !IsPlainFileName("..") && !IsPlainFileName("a/b") && !IsPlainFileName("a\\b") &&
		!IsPlainFileName("c:x"), "plain file names");
}

void Switch() {
	Check(SwitchOn("", UpdateCheckKey), "missing means on");
	Check(SwitchOn("a=1\nb=0\n", UpdateCheckKey), "other keys do not matter");
	Check(!SwitchOn("a=1\nupdate-check=0\n", UpdateCheckKey), "0 turns it off");
	Check(!SwitchOn("update-check=0\r\nz=1\r\n", UpdateCheckKey), "CRLF lines are read");
	Check(SwitchOn("update-check=1\n", UpdateCheckKey), "1 is on");
	Check(!SwitchOn("  update-check = 0  ", UpdateCheckKey), "spaces are allowed");
	Check(SwitchOn("my-update-check=0\n", UpdateCheckKey), "a longer key is not the switch");
	Check(SwitchOn("update-check\n", UpdateCheckKey), "a line without = is passed over");
}

void Protected() {
	Check(IsProtectedPath(L"d2rloader\\config\\d2rloader.toml"), "any toml is protected");
	Check(IsProtectedPath(L"D2R-ACCESS.TOML"), "case does not matter");
	Check(IsProtectedPath(L"d2rloader\\plugins\\d2r-access\\settings.txt"), "settings.txt is protected");
	Check(IsProtectedPath(L"d2rloader/plugins/d2r-access/tutorial.txt"), "forward slashes work");
	Check(IsProtectedPath(L"d2rloader\\plugins\\d2r-access\\explored\\char.dat"), "explored files are protected");
	Check(!IsProtectedPath(L"d2rloader\\plugins\\d2r-access\\prism.dll"), "prism.dll is replaced");
	Check(!IsProtectedPath(L"D2RAccess.exe") && !IsProtectedPath(L"d2rloader\\plugins\\d2rl-d2r-access.dll"), "program files are replaced");
	Check(!IsProtectedPath(L"d2rloader\\plugins\\other\\settings.txt"), "another plugin's settings.txt is not ours");
	Check(!IsProtectedPath(L"d2rloader\\plugins\\d2r-access\\explored.dll"), "explored is a folder rule");
}

void Text() {
	Check(TruncateChars("short", 10) == "short", "short text is whole");
	Check(TruncateChars("abcdef", 3) == "abc...", "long text is cut with dots");
	Check(TruncateChars("abc", 3) == "abc", "exact length is whole");
	Check(TruncateChars("\xC3\xA9\xC3\xA9\xC3\xA9", 2) == "\xC3\xA9\xC3\xA9...", "the cut counts characters, not bytes");
}

}

int main() {
	Tags();
	Extractor();
	Manifests();
	Switch();
	Protected();
	Text();
	Passed();
	return 0;
}
