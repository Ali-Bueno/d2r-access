#include "audio_description_script.h"
#include "test_support.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace D2RAccess;
using namespace D2RAccess::AudioDescription;

namespace {
// Estimated speaking pace at a screen reader's default rate, in characters a second. English at
// 2.8 words a second (measured narration, docs/cinematics-audio-description.md) is about 15
// characters with spaces; 14 leaves room. Japanese, Chinese and Korean are counted per character.
constexpr double AlphabeticPace = 14.0;
constexpr double JapanesePace   = 7.0;
constexpr double ChinesePace    = 4.5;
constexpr double KoreanPace     = 6.5;
// A cue must end this long before the next speech line starts.
constexpr double SpeechMargin = 0.2;

struct Window { double start = 0, end = 0; };
struct Timeline { double length = 0; std::vector<Window> speech; };

std::string ReadFile(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	std::stringstream text;
	text << input.rdbuf();
	return text.str();
}

size_t CodePoints(std::string_view text) {
	size_t count = 0;
	for (const char c : text) count += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
	return count;
}

double EstimatedSeconds(std::string_view text, std::string_view locale) {
	double pace = AlphabeticPace;
	if (locale == "jaJP") pace = JapanesePace;
	else if (locale == "zhCN" || locale == "zhTW") pace = ChinesePace;
	else if (locale == "koKR") pace = KoreanPace;
	return static_cast<double>(CodePoints(text)) / pace;
}

std::map<std::string, Timeline> ReadSpeech() {
	std::map<std::string, Timeline> timelines;
	std::istringstream lines(ReadFile(std::filesystem::path(D2R_AUDIODESC_DIR) / "speech.tsv"));
	std::string line;
	while (std::getline(lines, line)) {
		if (line.empty() || line.front() == '#') continue;
		std::istringstream fields(line);
		std::string video, first, second;
		if (!std::getline(fields, video, '\t') || !std::getline(fields, first, '\t') || !std::getline(fields, second)) continue;
		if (first == "length") timelines[video].length = std::stod(second);
		else timelines[video].speech.push_back({std::stod(first), std::stod(second)});
	}
	return timelines;
}

void CheckParse() {
	const Parsed parsed = Parse("\xEF\xBB\xBF# a note\r\n"
		"d2intro\t12.5\tA tavern at night.\r\n"
		"D2INTRO\t3\t  Rain falls.  \n"
		"act02start\t1\tSand.\n"
		"nowhere\t1\tUnknown video.\n"
		"d2intro\tsoon\tBad time.\n"
		"d2intro\t-1\tNegative time.\n"
		"d2intro\t4\t\n"
		"d2intro 5 no tabs\n");
	Check(parsed.rejected == std::vector<size_t>{5, 6, 7, 8, 9}, "bad lines are rejected by number");
	Check(parsed.script.size() == 2, "two videos, any case");
	const auto& intro = parsed.script.at("d2intro");
	Check(intro.size() == 2 && intro[0].at == 3 && intro[0].text == "Rain falls." && intro[1].at == 12.5,
		"cues in time order, trimmed");
	Check(parsed.script.contains("Act02start"), "a video is kept under its own spelling");
	Check(VideoIndex("act04END") == 4 && !VideoIndex("intro"), "the Videos list's index by name");
}

void CheckInPlayVideo() {
	Check(InPlayVideo(2, 0) == 1u && InPlayVideo(2, 1) == 1u,
		"index 2 is Act II's opening, leaving Act I (a tester's log) or arriving in Act II");
	Check(InPlayVideo(3, 1) == 2u && InPlayVideo(4, 2) == 3u && InPlayVideo(4, 3) == 3u, "Acts III and IV's openings");
	Check(InPlayVideo(5, 3) == 4u && InPlayVideo(6, 3) == 5u && InPlayVideo(6, 4) == 5u && InPlayVideo(7, 4) == 6u,
		"Act IV's end, Act V's opening and its end");
	Check(!InPlayVideo(0, 0) && !InPlayVideo(1, 0), "the logos and the intro never play in play");
	Check(!InPlayVideo(3, 0) && !InPlayVideo(2, 2) && !InPlayVideo(7, 3) && !InPlayVideo(5, 2),
		"a video of another act is refused");
	Check(!InPlayVideo(2, -1) && !InPlayVideo(8, 4) && !InPlayVideo(-1, 0), "an unknown act or index is refused");
}

void CheckScheduler() {
	const std::vector<Cue> cues{{1, "one"}, {2, "two"}, {6, "six"}, {7, "seven"}};
	Scheduler scheduler;
	size_t dropped = 0;
	Check(scheduler.Due(10, dropped).empty() && !scheduler.Running(), "nothing before a start");
	scheduler.Start(&cues);
	Check(scheduler.Due(0.5, dropped).empty(), "nothing before the first cue");
	auto due = scheduler.Due(2.1, dropped);
	Check(due.size() == 2 && due[0]->text == "one" && due[1]->text == "two" && dropped == 0, "due cues in order");
	Check(scheduler.Due(2.2, dropped).empty(), "each cue once");
	due = scheduler.Due(8.5, dropped);
	Check(due.size() == 1 && due[0]->text == "seven" && dropped == 1, "a cue more than 2 s late is dropped");
	scheduler.Start(&cues);
	Check(scheduler.Due(1, dropped).size() == 1, "a new start says the cues again");
	scheduler.Stop();
	Check(scheduler.Due(100, dropped).empty() && !scheduler.Running(), "nothing after a stop");
}

// Every file under data/audiodesc/<edition>/: parses, and no cue is said over speech or another cue.
void CheckScripts() {
	const auto timelines = ReadSpeech();
	Check(timelines.size() == Videos.size(), "speech.tsv has every video");
	size_t problems = 0, cues = 0;
	auto problem = [&](const std::string& where, const std::string& what) {
		std::fprintf(stderr, "%s: %s\n", where.c_str(), what.c_str());
		++problems;
	};
	for (const char* edition : {"hd", "classic"}) {
		const auto folder = std::filesystem::path(D2R_AUDIODESC_DIR) / edition;
		if (!std::filesystem::exists(folder / "enUS.txt")) problem(std::string(edition), "enUS.txt is missing");
		if (!std::filesystem::exists(folder)) continue;
		for (const auto& entry : std::filesystem::directory_iterator(folder)) {
			if (entry.path().extension() != ".txt") continue;
			const std::string locale = entry.path().stem().string();
			const std::string file = std::string(edition) + "/" + locale;
			const Parsed parsed = Parse(ReadFile(entry.path()));
			for (const size_t line : parsed.rejected) problem(file + ":" + std::to_string(line), "not <video> TAB <seconds> TAB <text>");
			if (locale == "enUS")
				for (const auto video : Videos)
					if (!parsed.script.contains(video)) problem(file, std::string(video) + " has no cues");
			for (const auto& [video, list] : parsed.script) {
				const Timeline& timeline = timelines.at(video);
				for (size_t i = 0; i < list.size(); ++i) {
					++cues;
					char where[160];
					std::snprintf(where, sizeof where, "%s %s %.2f", file.c_str(), video.c_str(), list[i].at);
					const double end = list[i].at + EstimatedSeconds(list[i].text, locale);
					if (end > timeline.length) problem(where, "ends after the video");
					if (i + 1 < list.size() && end > list[i + 1].at) {
						char what[96];
						std::snprintf(what, sizeof what, "runs into the next cue (ends about %.2f)", end);
						problem(where, what);
					}
					for (const Window& speech : timeline.speech) {
						if (list[i].at >= speech.start && list[i].at < speech.end) {
							problem(where, "starts while someone speaks");
							break;
						}
						if (speech.start > list[i].at && end > speech.start - SpeechMargin) {
							char what[96];
							std::snprintf(what, sizeof what, "ends about %.2f, over the speech at %.2f", end, speech.start);
							problem(where, what);
							break;
						}
					}
				}
			}
		}
	}
	std::printf("%zu cues checked, %zu problems\n", cues, problems);
	Check(problems == 0, "every cue fits a stretch free of speech");
}
}

int main() {
	CheckParse();
	CheckInPlayVideo();
	CheckScheduler();
	CheckScripts();
	Passed();
	return 0;
}
