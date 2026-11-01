#include "descriptions.h"

#include "async_log.h"

#include <algorithm>
#include <atomic>
#include <mutex>

namespace D2RAccess::Descriptions {
namespace {
constexpr std::string_view Separator = "=";
constexpr char Note = '#';
constexpr std::string_view English = "enUS";

std::string_view Trim(std::string_view text) {
	constexpr std::string_view Blank = " \t\r\n";
	const auto first = text.find_first_not_of(Blank);
	if (first == std::string_view::npos) return {};
	return text.substr(first, text.find_last_not_of(Blank) - first + 1);
}

std::mutex g_mutex;
Book g_book;
SealedText::Texts g_texts;
std::string g_locale;                         // the language g_book was read in
std::string_view (*g_localeOf)() = nullptr;  // the game's language now
std::atomic<bool> g_enabled{true};
std::atomic<bool> g_hint{false};
const D2RL::PluginContext* g_context = nullptr;
int g_level = 0; // the level the player is in (NoteLevel)

// The book in the game's language, read again when it changed. Caller holds g_mutex.
void Follow() {
	if (!g_localeOf) return;
	const auto locale = g_localeOf();
	if (locale == g_locale) return;
	g_locale = locale;
	g_book.Replace(Layered(g_texts, locale));
	AsyncLog::InfoF(g_context, "[descriptions] %zu in %s (%s)", g_book.Size(), g_locale.c_str(),
		g_texts.contains(g_locale) ? "its own words, English under them" : "English only");
}
}

std::string Fold(std::string_view name) {
	std::string folded(Trim(name));
	for (char& c : folded) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	return folded;
}

std::string LevelKey(int level) { return "[" + std::to_string(level) + "]"; }

std::string Book::Keyed(std::string_view name) {
	std::string folded = Fold(name);
	const auto open = folded.rfind('[');
	if (open == std::string::npos || folded.empty() || folded.back() != ']' || open + 2 >= folded.size()
		|| (open > 0 && folded[open - 1] != ' ')) return folded;
	const std::string_view digits = std::string_view(folded).substr(open + 1, folded.size() - open - 2);
	if (digits.size() > 4 || !std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; })) return folded;
	return LevelKey(std::stoi(std::string(digits)));
}

size_t Book::Parse(std::string_view text) {
	size_t rejected = 0;
	while (!text.empty()) {
		const auto end = text.find('\n');
		const auto line = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (line.empty() || line.front() == Note) continue;
		const auto split = line.find(Separator);
		const auto key = split == std::string_view::npos ? std::string{} : Keyed(line.substr(0, split));
		const auto words = split == std::string_view::npos ? std::string_view{} : Trim(line.substr(split + Separator.size()));
		if (key.empty() || words.empty()) { ++rejected; continue; }
		entries_[key] = std::string(words);
	}
	return rejected;
}

void Book::Replace(std::string_view text) {
	Book fresh;
	fresh.Parse(text);
	entries_ = std::move(fresh.entries_);
}

std::string Book::Find(std::string_view name, std::string_view builtIn) const {
	const auto found = entries_.find(Fold(name));
	return found != entries_.end() ? found->second : std::string(builtIn);
}

std::string Book::FindArea(int level) const {
	const auto found = level > 0 ? entries_.find(LevelKey(level)) : entries_.end();
	return found != entries_.end() ? found->second : std::string{};
}

std::string Book::FirstTime(std::string_view name, std::string_view builtIn) {
	auto words = Find(name, builtIn);
	if (words.empty() || !told_.insert(Fold(name)).second) return {};
	return words;
}

bool Book::HintDue(int level) {
	return level > 0 && entries_.contains(LevelKey(level)) && hinted_.insert(LevelKey(level)).second;
}

std::string Layered(const SealedText::Texts& texts, std::string_view locale) {
	const auto english = texts.find(std::string(English));
	std::string text = english != texts.end() ? english->second : std::string{};
	if (const auto own = texts.find(std::string(locale)); locale != English && own != texts.end())
		text.append(1, '\n').append(own->second);
	return text;
}

void Load(const D2RL::PluginContext* context, SealedText::Texts texts, std::string_view (*locale)()) {
	std::lock_guard lock(g_mutex);
	g_context = context;
	g_texts = std::move(texts);
	g_localeOf = locale;
	g_locale.clear();
	g_book = {};
	AsyncLog::InfoF(context, "[descriptions] %zu languages come with the mod", g_texts.size());
	Follow();
}

std::string FindArea(int level) {
	std::lock_guard lock(g_mutex);
	Follow();
	return g_book.FindArea(level);
}
void NoteLevel(int level) {
	std::lock_guard lock(g_mutex);
	g_level = level;
}
std::string FirstTime(std::string_view name, std::string_view builtIn) {
	if (!g_enabled) return {};
	std::lock_guard lock(g_mutex);
	Follow();
	return g_book.FirstTime(name, builtIn);
}
bool HintDue() {
	if (!g_hint) return false;
	std::lock_guard lock(g_mutex);
	Follow();
	return g_book.HintDue(g_level);
}
bool Enabled() { return g_enabled; }
void SetEnabled(bool on) { g_enabled = on; }
bool HintEnabled() { return g_hint; }
void SetHintEnabled(bool on) { g_hint = on; }
}
