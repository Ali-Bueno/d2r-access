#include "audio_description_script.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace D2RAccess::AudioDescription {

namespace {
std::string_view Trim(std::string_view s) {
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
	return s;
}

bool SameFolded(std::string_view a, std::string_view b) {
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
	});
}

std::optional<double> Seconds(std::string_view s) {
	double value = 0;
	const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
	if (error != std::errc{} || end != s.data() + s.size() || !std::isfinite(value) || value < 0) return std::nullopt;
	return value;
}
}

std::optional<size_t> VideoIndex(std::string_view name) {
	for (size_t i = 0; i < Videos.size(); ++i)
		if (SameFolded(Videos[i], name)) return i;
	return std::nullopt;
}

std::optional<size_t> InPlayVideo(int64_t progressionIndex, int act) {
	// The game's table has 8 names, one before the intro; its 5 is followed by 6 (RE), which fits
	// Terror's End then Search for Baal. A tester's Act I to II move read 2 (Desert Journey).
	constexpr int64_t GameTableOffset = 1;
	// The acts each video can start in: an act's opening one as the player leaves the act before or
	// arrives; Terror's End and Search for Baal after Diablo, in Act IV or on the way to Act V;
	// Destruction's End after Baal. The intro never plays in play.
	struct Acts { int first, last; };
	constexpr std::array<Acts, Videos.size()> InPlayActs{{
		{-1, -1}, {0, 1}, {1, 2}, {2, 3}, {3, 4}, {3, 4}, {4, 4}}};
	const int64_t video = progressionIndex - GameTableOffset;
	if (video < 0 || video >= static_cast<int64_t>(Videos.size())) return std::nullopt;
	const Acts acts = InPlayActs[static_cast<size_t>(video)];
	if (act < acts.first || act > acts.last || acts.first < 0) return std::nullopt;
	return static_cast<size_t>(video);
}

Parsed Parse(std::string_view text) {
	if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
	Parsed parsed;
	size_t number = 0;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		const std::string_view line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		++number;
		if (line.empty() || line.front() == '#') continue;

		const size_t tab1 = line.find('\t');
		const size_t tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
		const auto video = tab1 == std::string_view::npos ? std::nullopt : VideoIndex(Trim(line.substr(0, tab1)));
		const auto at = tab2 == std::string_view::npos ? std::nullopt : Seconds(Trim(line.substr(tab1 + 1, tab2 - tab1 - 1)));
		const std::string_view words = tab2 == std::string_view::npos ? std::string_view{} : Trim(line.substr(tab2 + 1));
		if (!video || !at || words.empty()) {
			parsed.rejected.push_back(number);
			continue;
		}
		parsed.script[std::string(Videos[*video])].push_back({*at, std::string(words)});
	}
	for (auto& [video, cues] : parsed.script)
		std::stable_sort(cues.begin(), cues.end(), [](const Cue& a, const Cue& b) { return a.at < b.at; });
	return parsed;
}

void Scheduler::Start(const std::vector<Cue>* cues) {
	cues_ = cues && !cues->empty() ? cues : nullptr;
	next_ = 0;
}

void Scheduler::Stop() {
	cues_ = nullptr;
	next_ = 0;
}

std::vector<const Cue*> Scheduler::Due(double clock, size_t& dropped) {
	std::vector<const Cue*> due;
	while (cues_ && next_ < cues_->size() && (*cues_)[next_].at <= clock) {
		const Cue& cue = (*cues_)[next_++];
		if (clock - cue.at > LateLimit) ++dropped;
		else due.push_back(&cue);
	}
	return due;
}

}
