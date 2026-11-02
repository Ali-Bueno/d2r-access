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

double Pace(std::string_view locale) {
	if (locale == "jaJP") return 10.0;
	if (locale == "zhCN" || locale == "zhTW") return 6.4;
	if (locale == "koKR") return 9.3;
	return 20.0;
}

double EstimatedSeconds(std::string_view text, std::string_view locale) {
	size_t codePoints = 0;
	for (const char c : text) codePoints += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
	return static_cast<double>(codePoints) / Pace(locale);
}

SpeechStarts ParseSpeech(std::string_view text) {
	SpeechStarts starts;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		const std::string_view line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (line.empty() || line.front() == '#') continue;
		const size_t tab1 = line.find('\t');
		const size_t tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
		if (tab2 == std::string_view::npos) continue;
		const auto video = VideoIndex(Trim(line.substr(0, tab1)));
		const auto start = Seconds(Trim(line.substr(tab1 + 1, tab2 - tab1 - 1)));
		if (video && start) starts[std::string(Videos[*video])].push_back(*start);
	}
	for (auto& [video, list] : starts) std::sort(list.begin(), list.end());
	return starts;
}

SubtitleStarts ParseSubtitles(std::string_view text) {
	SubtitleStarts starts;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		const std::string_view line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (line.empty() || line.front() == '#') continue;
		const size_t tab1 = line.find('\t');
		const size_t tab2 = tab1 == std::string_view::npos ? tab1 : line.find('\t', tab1 + 1);
		if (tab2 == std::string_view::npos) continue;
		const auto video = VideoIndex(Trim(line.substr(0, tab1)));
		const std::string_view locale = Trim(line.substr(tab1 + 1, tab2 - tab1 - 1));
		if (!video || locale.empty()) continue;
		std::vector<double> list;
		std::string_view times = Trim(line.substr(tab2 + 1));
		while (!times.empty()) {
			const size_t comma = times.find(',');
			if (const auto at = Seconds(Trim(times.substr(0, comma)))) list.push_back(*at);
			times = comma == std::string_view::npos ? std::string_view{} : times.substr(comma + 1);
		}
		std::sort(list.begin(), list.end());
		if (!list.empty()) starts[std::string(Videos[*video])][std::string(locale)] = std::move(list);
	}
	return starts;
}

void SubtitleSync::Start(const std::vector<double>* starts) {
	starts_ = starts && !starts->empty() ? starts : nullptr;
	seen_.clear();
}

bool SubtitleSync::Near(double t) const {
	const auto at = std::lower_bound(starts_->begin(), starts_->end(), t - Match);
	return at != starts_->end() && *at <= t + Match;
}

std::optional<double> SubtitleSync::Seen(double raw) {
	if (!starts_) return std::nullopt;
	seen_.push_back(raw);
	if (seen_.size() > Recent) seen_.erase(seen_.begin());
	// Every offset that puts the newest line on a start is a candidate; the one most recent lines
	// agree with wins, the smaller on a tie.
	std::optional<double> best;
	size_t bestVotes = 0;
	for (const double start : *starts_) {
		const double offset = start - raw;
		if (offset < -Behind) continue;
		if (offset > Ahead) break;
		size_t votes = 0;
		for (const double t : seen_) votes += Near(t + offset);
		if (votes > bestVotes || (votes == bestVotes && best && std::abs(offset) < std::abs(*best))) {
			best = offset;
			bestVotes = votes;
		}
	}
	return bestVotes >= 2 ? best : std::nullopt;
}

void DialogueGate::Start(const std::vector<double>* starts) {
	starts_ = starts;
	next_ = 0;
	cueAt_ = 0;
	speakingUntil_ = -1;
}

void DialogueGate::Said(double clock, double seconds) {
	if (clock >= speakingUntil_) { // the reader is done: a new run of speech starts here
		cueAt_ = clock;
		speakingUntil_ = clock;
	}
	speakingUntil_ += seconds * SlowFactor; // a slow reader queues it after the ones still talking
}

bool DialogueGate::CutDue(double clock) {
	bool cut = false;
	while (starts_ && next_ < starts_->size() && (*starts_)[next_] <= clock) {
		const double start = (*starts_)[next_++];
		if (start >= cueAt_ && start < speakingUntil_) cut = true;
	}
	if (cut) speakingUntil_ = -1;
	return cut;
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
