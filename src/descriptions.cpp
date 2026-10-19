#include "descriptions.h"

#include "async_log.h"

#include <windows.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>

namespace D2RAccess::Descriptions {
namespace {
constexpr std::string_view Separator = "=";
constexpr char Note = '#';
// Written once when the file is missing, so the player finds how to fill it.
constexpr std::string_view Template =
	"# D2R Access descriptions: your own words for an area or a landmark.\n"
	"# One per line: the name as the mod says it, an equals sign, then the words.\n"
	"# An area's words are read with Back and right stick up while you are there;\n"
	"# a landmark's replace its name the first time it comes into reach.\n"
	"# Lines starting with # are notes. Updates of the mod never replace this file.\n"
	"# A change is read a second after you save it, while the game runs.\n"
	"# Every name the mod answers to is in descriptions-names.txt, written as you play.\n"
	"#\n"
	"# Rogue Encampment = A ramshackle collection of tents and wagons...\n"
	"# Hangman's Tree = A tall, withered tree stands in the middle of the Burial Grounds...\n";

std::string_view Trim(std::string_view text) {
	constexpr std::string_view Blank = " \t\r\n";
	const auto first = text.find_first_not_of(Blank);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(Blank) - first + 1);
}

// Rewritten each game, in the game's language: the names the book answers to.
constexpr std::string_view NamesFile = "descriptions-names.txt";
constexpr std::string_view NamesHeader =
	"# D2R Access: every name descriptions.txt answers to, in the game's language.\n"
	"# Copy a name as it is, add an equals sign, then your words. Written anew each time you play.\n";
// How often a request for words looks for a saved change of the file.
constexpr uint64_t RecheckMs = 1000;

std::mutex g_mutex;
Book g_book;
std::atomic<bool> g_enabled{true};
const D2RL::PluginContext* g_context = nullptr;
std::filesystem::path g_file;
std::filesystem::file_time_type g_written{};
uint64_t g_checkedMs = 0;
std::string g_banner; // the last area banner, "Entering The Cave Level 1"

std::string ReadBook(const std::filesystem::path& file) {
	std::ifstream input(file, std::ios::binary);
	std::stringstream text;
	text << input.rdbuf();
	std::string content = text.str();
	if (content.starts_with("\xEF\xBB\xBF")) content.erase(0, 3); // Notepad's byte-order mark
	return content;
}

// The book again if the file was saved since it was read. Caller holds g_mutex.
void Refresh() {
	const uint64_t now = GetTickCount64();
	if (g_file.empty() || now - g_checkedMs < RecheckMs) return;
	g_checkedMs = now;
	std::error_code error;
	const auto written = std::filesystem::last_write_time(g_file, error);
	if (error || written == g_written) return;
	g_written = written;
	g_book.Replace(ReadBook(g_file));
	AsyncLog::InfoF(g_context, "[descriptions] %zu read again from %s, saved while playing", g_book.Size(),
		g_file.filename().string().c_str());
}
}

std::string Fold(std::string_view name) {
	std::string folded(Trim(name));
	for (char& c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	return folded;
}

void Book::Parse(std::string_view text) {
	while (!text.empty()) {
		const auto end = text.find('\n');
		const auto line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (line.empty() || line.front() == Note) continue;
		const auto split = line.find(Separator);
		if (split == std::string_view::npos) continue;
		const auto key = Fold(line.substr(0, split));
		const auto words = Trim(line.substr(split + Separator.size()));
		if (!key.empty() && !words.empty()) entries_[key] = std::string(words);
	}
}

void Book::Replace(std::string_view text) {
	Book fresh;
	fresh.Parse(text);
	for (const auto& [key, words] : fresh.entries_)
		if (const auto old = entries_.find(key); old == entries_.end() || old->second != words) { told_.erase(key); hinted_.erase(key); }
	entries_ = std::move(fresh.entries_);
}

std::string Book::Find(std::string_view name, std::string_view builtIn) const {
	const auto found = entries_.find(Fold(name));
	return found != entries_.end() ? found->second : std::string(builtIn);
}

namespace {
// `text` ends with the whole words of `tail`.
bool EndsWithWords(std::string_view text, std::string_view tail) {
	return !tail.empty() && text.ends_with(tail) && (text.size() == tail.size() || text[text.size() - tail.size() - 1] == ' ');
}
}

std::string Book::FindArea(std::string_view name, std::string_view banner) const {
	if (auto words = Find(name); !words.empty()) return words;
	const auto level = Fold(name), shown = Fold(banner);
	const std::string* best = nullptr;
	size_t bestLength = 0;
	for (const auto& [key, words] : entries_)
		if (EndsWithWords(key, level) && EndsWithWords(shown, key) && key.size() > bestLength) { best = &words; bestLength = key.size(); }
	return best ? *best : std::string{};
}

std::string Book::FirstTime(std::string_view name, std::string_view builtIn) {
	auto words = Find(name, builtIn);
	if (words.empty() || !told_.insert(Fold(name)).second) return {};
	return words;
}

bool Book::HintDue(std::string_view area) {
	return !Find(area).empty() && hinted_.insert(Fold(area)).second;
}

void Load(const D2RL::PluginContext* context, const std::filesystem::path& file) {
	std::error_code error;
	if (!std::filesystem::exists(file, error)) {
		std::ofstream(file, std::ios::binary) << Template;
		AsyncLog::InfoF(context, "[descriptions] %s written", file.filename().string().c_str());
	}
	std::lock_guard lock(g_mutex);
	g_context = context;
	g_file = file;
	g_written = std::filesystem::last_write_time(file, error);
	g_checkedMs = GetTickCount64();
	g_book = {};
	g_book.Parse(ReadBook(file));
	AsyncLog::InfoF(context, "[descriptions] %zu read from %s", g_book.Size(), file.filename().string().c_str());
}

void WriteNames(const std::vector<std::string>& areas, const std::vector<std::string>& landmarks) {
	std::filesystem::path path;
	{
		std::lock_guard lock(g_mutex);
		if (g_file.empty()) return;
		path = g_file.parent_path() / NamesFile;
	}
	std::ofstream out(path, std::ios::binary);
	out << NamesHeader << "\n# Areas\n";
	for (const auto& name : areas) out << name << '\n';
	out << "\n# Landmarks\n";
	for (const auto& name : landmarks) out << name << '\n';
	AsyncLog::InfoF(g_context, "[descriptions] %zu area and %zu landmark names written to %s", areas.size(), landmarks.size(),
		path.filename().string().c_str());
}

std::string Find(std::string_view name, std::string_view builtIn) {
	std::lock_guard lock(g_mutex);
	Refresh();
	return g_book.Find(name, builtIn);
}
std::string FindArea(std::string_view name) {
	std::lock_guard lock(g_mutex);
	Refresh();
	return g_book.FindArea(name, g_banner);
}
void NoteBanner(std::string_view banner) {
	std::lock_guard lock(g_mutex);
	g_banner = banner;
}
std::string FirstTime(std::string_view name, std::string_view builtIn) {
	if (!g_enabled) return {};
	std::lock_guard lock(g_mutex);
	Refresh();
	return g_book.FirstTime(name, builtIn);
}
bool HintDue(std::string_view area) {
	if (!g_enabled) return false;
	std::lock_guard lock(g_mutex);
	Refresh();
	return g_book.HintDue(area);
}
bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }
}
