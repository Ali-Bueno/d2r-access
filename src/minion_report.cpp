#include "minion_report.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace D2RAccess::MinionReport {
namespace {
std::string Format(const char* format, const std::string& name, int value) {
	char text[200]{};
	std::snprintf(text, sizeof(text), format, name.c_str(), value);
	return text;
}
}

std::vector<Entry> Entries(const std::vector<Pet>& pets, const std::function<Kind(int type)>& kind,
	const std::function<std::string(uint16_t classId)>& className, const Words& words) {
	std::map<int, std::vector<int>> byType; // pet indexes, in PetType order
	for (size_t i = 0; i < pets.size(); ++i) byType[pets[i].type].push_back(static_cast<int>(i));
	// Each pet shown alone, by name; a name that repeats is numbered once all are known.
	struct Single { std::string name; int life, pet; };
	std::vector<Entry> entries;
	std::vector<Single> singles; // in the order of the entries with a pet
	for (const auto& [type, group] : byType) {
		const Kind shown = kind(type);
		if (shown.icon == IconType::None) continue;
		const auto& first = pets[static_cast<size_t>(group.front())];
		if (shown.icon == IconType::Counted) {
			const bool shared = std::all_of(group.begin(), group.end(), [&](int i) { return pets[static_cast<size_t>(i)].classId == first.classId; });
			std::string name = shared ? className(first.classId) : shown.name;
			if (name.empty()) name = shared ? shown.name : className(first.classId);
			if (!name.empty()) entries.push_back({Format(words.count, name, static_cast<int>(group.size())), -1});
			continue;
		}
		for (const int i : group) {
			const auto& pet = pets[static_cast<size_t>(i)];
			std::string name = className(pet.classId);
			if (name.empty()) name = shown.name;
			if (name.empty()) continue;
			singles.push_back({std::move(name), shown.drawHp ? pet.life : -1, i});
			entries.push_back({{}, i});
		}
	}
	std::map<std::string, int> total, seen;
	for (const auto& single : singles) ++total[single.name];
	size_t next = 0;
	for (auto& entry : entries) {
		if (entry.pet < 0) continue;
		const auto& single = singles[next++];
		std::string name = single.name;
		if (total[name] > 1) name = Format(words.numbered, name, ++seen[name]);
		entry.text = single.life >= 0 ? Format(words.life, name, single.life) : name;
	}
	return entries;
}

int Stepper::Next(size_t count, uint64_t now) {
	const bool fresh = at_ < 0 || now < last_ || now - last_ >= RestartMs;
	last_ = now;
	if (count == 0) { at_ = -1; return -1; }
	at_ = fresh ? 0 : (at_ + 1) % static_cast<int>(count);
	return at_;
}
}
