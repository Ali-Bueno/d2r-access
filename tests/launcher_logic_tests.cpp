#include "launcher_logic.h"
#include "test_support.h"

using namespace D2RAccess::Launcher;

namespace {

constexpr std::uint64_t Now = 1'800'000'000ULL * NanosPerSecond;

std::uint64_t DaysAgo(std::uint64_t days) { return Now - days * NanosPerDay; }

void SettingsDate() {
	Check(ParseLastOnline(R"({"Foo": 1, "User Last Online": 1792163632624867200, "Bar": 2})") == 1792163632624867200ULL, "the date is read");
	Check(ParseLastOnline("{\n\t\"User Last Online\" :\t42\n}") == 42, "spacing around the colon is allowed");
	Check(ParseLastOnline(R"({"User Last Online": "77"})") == 77, "a quoted number is read");
	Check(!ParseLastOnline(R"({"Foo": 1})"), "a missing key gives nothing");
	Check(!ParseLastOnline(""), "an empty file gives nothing");
	Check(!ParseLastOnline(R"({"User Last Online": abc})"), "a non-number gives nothing");
	Check(!ParseLastOnline(R"({"User Last Online" 5})"), "a missing colon gives nothing");
	Check(!ParseLastOnline(R"({"User Last Online": 99999999999999999999})"), "an overflowing number gives nothing");
	Check(!ParseLastOnline(R"({"User Last Online":)"), "a truncated file gives nothing");
	Check(!ParseLastOnline(FirstRunSettings), "the first-run file has no date, so the license is still validated");
}

void Decision() {
	Check(Classify(DaysAgo(0), Now) == Verdict::Fresh, "today is fresh");
	Check(Classify(DaysAgo(ValidateAfterDays - 1), Now) == Verdict::Fresh, "just under the threshold is fresh");
	Check(Classify(DaysAgo(ValidateAfterDays), Now) == Verdict::Stale, "the threshold validates");
	Check(Classify(DaysAgo(OfflineLimitDays - 1), Now) == Verdict::Stale, "inside the offline limit is stale");
	Check(Classify(DaysAgo(OfflineLimitDays), Now) == Verdict::Expired, "the offline limit is expired");
	Check(Classify(std::nullopt, Now) == Verdict::Expired, "no date is expired");
	Check(Classify(Now + NanosPerDay, Now) == Verdict::Fresh, "a date in the future is fresh");
	Check(NeedsValidation(Verdict::Stale) && NeedsValidation(Verdict::Expired) && !NeedsValidation(Verdict::Fresh), "only stale and expired validate");
	Check(AgeDays(DaysAgo(3) - 1, Now) == 3 && AgeDays(Now + 1, Now) == 0, "age counts whole days and never goes negative");
}

void DateChange() {
	Check(DateChanged(5, 6), "a new date is a change");
	Check(!DateChanged(5, 5), "the same date is no change");
	Check(!DateChanged(5, std::nullopt), "an unreadable file is no change");
	Check(DateChanged(std::nullopt, 6), "a date appearing is a change");
}

void Locale() {
	Check(ParseTextLocale("[d2rloader]\ntext_locale = \"esES\"\naudio_locale = \"\"\n") == "esES", "the locale is read");
	Check(ParseTextLocale("# c\r\n[d2rloader]\r\n  text_locale=\"frFR\"  # note\r\n") == "frFR", "CRLF, spacing and comments are tolerated");
	Check(!ParseTextLocale("[d2rloader]\ntext_locale = \"\"\n"), "an empty locale gives nothing");
	Check(!ParseTextLocale("[d2rloader]\naudio_locale = \"esES\"\n"), "the key must be text_locale");
	Check(!ParseTextLocale("[other]\ntext_locale = \"esES\"\n"), "another section is ignored");
	Check(!ParseTextLocale("[d2rloader]\ntext_locale = esES\n"), "an unquoted value gives nothing");
	Check(!ParseTextLocale(""), "an empty file gives nothing");
}

void Arguments() {
	Check(ArgumentsAfterProgram(LR"("C:\a b\D2RAccess.exe" "C:\g\D2R.exe" -x)") == LR"("C:\g\D2R.exe" -x)", "quoted program, arguments kept as typed");
	Check(ArgumentsAfterProgram(LR"(D2RAccess.exe  -a  b)") == L"-a  b", "plain program name");
	Check(ArgumentsAfterProgram(LR"("C:\a\D2RAccess.exe")").empty(), "no arguments");
	Check(WithoutLeadingFlag(L"--validate", ValidateFlag) == L"", "the flag alone leaves nothing to forward");
	Check(WithoutLeadingFlag(LR"(--validate  "C:\g\D2R.exe")", ValidateFlag) == LR"("C:\g\D2R.exe")", "the rest is forwarded");
	Check(!WithoutLeadingFlag(LR"("C:\g\D2R.exe" --validate)", ValidateFlag), "only a leading flag counts");
	Check(!WithoutLeadingFlag(L"--validated", ValidateFlag), "a longer word is not the flag");
	Check(ArgumentsAfterProgram(L"").empty(), "empty command line");
}

void KeySchedule() {
	Check(NextKeyAction(0, std::nullopt, false) == KeyAction::Press, "the first press is immediate");
	Check(NextKeyAction(EscapeHoldMs - 1, 0, true) == KeyAction::None, "the key stays down for the hold time");
	Check(NextKeyAction(EscapeHoldMs, 0, true) == KeyAction::Release, "the key is released after the hold time");
	Check(NextKeyAction(EscapePeriodMs - 1, 0, false) == KeyAction::None, "no press before the period");
	Check(NextKeyAction(EscapePeriodMs, 0, false) == KeyAction::Press, "a press every period");
	Check(EscapeHoldMs >= 500 && EscapeHoldMs < EscapePeriodMs, "the hold skips videos and fits the period");
}

}

int main() {
	SettingsDate();
	Decision();
	DateChange();
	Locale();
	Arguments();
	KeySchedule();
	Passed();
	return 0;
}
