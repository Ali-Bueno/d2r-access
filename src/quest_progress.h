#pragma once

#include "quest_log.h"

#include <cstdint>
#include <set>
#include <string>

namespace D2RAccess::QuestProgress {

// Policy timings: allow the game's deferred text writes to settle, and collect
// one burst of quest UI events into one announcement. No native offsets.
inline constexpr uint64_t SettleMs = 150;
inline constexpr uint64_t AlertDelayMs = 800;
inline constexpr uint64_t DuplicateWindowMs = 1000;

struct Reading {
	std::string quest, objective;
	bool open = false;
	bool operator==(const Reading&) const = default;
};

// Accept only a localized quest title paired with that quest's own objective
// strings. Counter templates must contain a real number, never a raw %d.
Reading Identify(std::string_view title, std::string_view shortText,
	std::string_view longText, const QuestLog::Resolver& resolve);

enum class Kind { None, Updated, GoalDone, Objective };
struct Announcement { Kind kind = Kind::None; std::string quest, objective; };

class Reader {
public:
	void Alert(uint64_t now);
	void Completed(uint32_t row, uint32_t difficulty, uint64_t now);
	void Navigate();
	Announcement Update(const Reading& reading, uint64_t now, bool canSpeak);
	void Reset();
private:
	void Schedule(uint64_t now);
	Reading candidate_, baseline_, detail_;
	bool sampled_ = false, baselined_ = false;
	bool updated_ = false, completed_ = false, signalled_ = false;
	uint64_t since_ = 0, due_ = 0, lastSignal_ = 0;
	std::set<uint64_t> completedRows_;
};

}
