#include "descriptions.h"

#include "async_log.h"

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
	"#\n"
	"# Rogue Encampment = A ramshackle collection of tents and wagons...\n"
	"# Hangman's Tree = A tall, withered tree stands in the middle of the Burial Grounds...\n";

std::string_view Trim(std::string_view text) {
	constexpr std::string_view Blank = " \t\r\n";
	const auto first = text.find_first_not_of(Blank);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(Blank) - first + 1);
}

std::mutex g_mutex;
Book g_book;
std::atomic<bool> g_enabled{true};
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

std::string Book::Find(std::string_view name, std::string_view builtIn) const {
	const auto found = entries_.find(Fold(name));
	return found != entries_.end() ? found->second : std::string(builtIn);
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
		return;
	}
	std::ifstream input(file, std::ios::binary);
	std::stringstream text;
	text << input.rdbuf();
	std::string content = text.str();
	if (content.starts_with("\xEF\xBB\xBF")) content.erase(0, 3); // Notepad's byte-order mark
	std::lock_guard lock(g_mutex);
	g_book = {};
	g_book.Parse(content);
	AsyncLog::InfoF(context, "[descriptions] %zu read from %s", g_book.Size(), file.filename().string().c_str());
}

std::string Find(std::string_view name, std::string_view builtIn) {
	std::lock_guard lock(g_mutex);
	return g_book.Find(name, builtIn);
}
std::string FirstTime(std::string_view name, std::string_view builtIn) {
	if (!g_enabled) return {};
	std::lock_guard lock(g_mutex);
	return g_book.FirstTime(name, builtIn);
}
bool HintDue(std::string_view area) {
	if (!g_enabled) return false;
	std::lock_guard lock(g_mutex);
	return g_book.HintDue(area);
}
bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }
}
