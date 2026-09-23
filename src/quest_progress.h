#pragma once

#include "quest_log.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace D2RAccess::QuestProgress {

// Policy timings: allow the game's deferred text writes to settle, and collect
// one burst of quest UI events into one announcement. No native offsets.
inline constexpr uint64_t SettleMs = 150;
inline constexpr uint64_t AlertDelayMs = 800;
inline constexpr uint64_t DuplicateWindowMs = 1000;
inline constexpr uint64_t PacketWindowMs = 500; // Allow a packet burst to finish before selecting one quest row.

struct Reading {
	std::string quest, objective;
	bool open = false;
	bool operator==(const Reading&) const = default;
};

// Accept only a localized quest title paired with that quest's own objective
// strings. Counter templates must contain a real number, never a raw %d.
Reading Identify(std::string_view title, std::string_view shortText,
	std::string_view longText, const QuestLog::Resolver& resolve);

struct NativePacket {
	uint32_t row = 0;
	uint16_t titleId = 0;
	std::string objective;
	uint64_t time = 0;
};

// Correlates a quest-state packet's game-rendered objective with its UI alert.
// Journal rendering never enters this stream.
class PacketMatcher {
public:
	void Capture(NativePacket packet);
	void Alert(uint64_t now);
	std::optional<NativePacket> Take(uint64_t now);
	void Reset();
private:
	struct Entry { NativePacket packet; uint64_t serial; };
	std::vector<Entry> packets_;
	uint64_t serial_ = 0, consumed_ = 0, alertAt_ = 0;
	bool pending_ = false;
};

enum class Kind { None, Updated, GoalDone, Objective };
struct Announcement { Kind kind = Kind::None; std::string quest, objective; };

class Reader {
public:
	void Alert(uint64_t now);
	void Completed(uint32_t row, uint32_t difficulty, uint64_t now);
	void Navigate();
	void Correlate(Reading reading);
	Announcement Update(const Reading& reading, uint64_t now, bool canSpeak);
	void Reset();
private:
	void Schedule(uint64_t now);
	Reading candidate_, baseline_, detail_;
	Reading eventDetail_;
	bool sampled_ = false, baselined_ = false;
	bool updated_ = false, completed_ = false, signalled_ = false;
	uint64_t since_ = 0, due_ = 0, lastSignal_ = 0;
	std::set<uint64_t> completedRows_;
};

}
