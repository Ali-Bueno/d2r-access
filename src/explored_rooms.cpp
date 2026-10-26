#include "explored_rooms.h"

#include <charconv>
#include <cstdio>

namespace D2RAccess::ExploredRooms {

bool Memory::Mark(uint32_t level, int tileX, int tileY) {
	const bool added = levels_[level].seen.insert({tileX, tileY}).second;
	dirty_ = dirty_ || added;
	return added;
}

bool Memory::Visit(uint32_t level, int tileX, int tileY) {
	Mark(level, tileX, tileY);
	const bool added = levels_[level].visited.insert({tileX, tileY}).second;
	dirty_ = dirty_ || added;
	return added;
}

bool Memory::Explored(uint32_t level, int tileX, int tileY) const {
	const auto found = levels_.find(level);
	return found != levels_.end() && found->second.seen.count({tileX, tileY}) != 0;
}

bool Memory::Visited(uint32_t level, int tileX, int tileY) const {
	const auto found = levels_.find(level);
	return found != levels_.end() && found->second.visited.count({tileX, tileY}) != 0;
}

bool Memory::Known(uint32_t level) const {
	const auto found = levels_.find(level);
	return found != levels_.end() && !found->second.seen.empty();
}

size_t Memory::Count(uint32_t level) const {
	const auto found = levels_.find(level);
	return found == levels_.end() ? 0 : found->second.seen.size();
}

size_t Memory::VisitedCount(uint32_t level) const {
	const auto found = levels_.find(level);
	return found == levels_.end() ? 0 : found->second.visited.size();
}

bool Memory::SetDrawable(uint32_t level, int tileX, int tileY, uint32_t count) {
	if (count == 0) return false;
	auto& stored = levels_[level].drawable[{tileX, tileY}];
	const bool changed = stored != count;
	stored = count;
	dirty_ = dirty_ || changed;
	return changed;
}

uint32_t Memory::Drawable(uint32_t level, int tileX, int tileY) const {
	const auto found = levels_.find(level);
	if (found == levels_.end()) return 0;
	const auto count = found->second.drawable.find({tileX, tileY});
	return count == found->second.drawable.end() ? 0 : count->second;
}

void Memory::Forget() { levels_.clear(); dirty_ = false; }

std::string Memory::Serialize(std::optional<uint32_t> only) const {
	std::string text;
	char line[64]{};
	for (const auto& [level, rooms] : levels_) {
		if (only && *only != level) continue;
		for (const auto& [x, y] : rooms.seen) {
			const bool visited = rooms.visited.count({x, y}) != 0;
			std::snprintf(line, sizeof line, "%u %d %d %c\n", level, x, y, visited ? 'v' : 's');
			text += line;
		}
	}
	return text;
}

std::vector<Room> Memory::Snapshot(uint32_t level) const {
	std::vector<Room> result;
	const auto found = levels_.find(level);
	if (found == levels_.end()) return result;
	result.reserve(found->second.seen.size());
	for (const auto& [x, y] : found->second.seen)
		result.push_back({x, y, found->second.visited.count({x, y}) != 0, Drawable(level, x, y)});
	return result;
}

void Memory::Merge(uint32_t level, const std::vector<Room>& rooms) {
	const bool wasDirty = dirty_;
	for (const auto& room : rooms) {
		if (room.visited) Visit(level, room.tileX, room.tileY);
		else Mark(level, room.tileX, room.tileY);
		// A count already measured this game wins over the stored one.
		if (room.drawable && !Drawable(level, room.tileX, room.tileY)) SetDrawable(level, room.tileX, room.tileY, room.drawable);
	}
	dirty_ = wasDirty;
}

void Memory::Merge(std::string_view text) {
	const bool wasDirty = dirty_;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
		unsigned level = 0;
		int x = 0, y = 0;
		char flag = 0;
		// A line that fails to parse is skipped, never trusted in part.
		const auto field = [&](auto& value) {
			while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
			const auto result = std::from_chars(line.data(), line.data() + line.size(), value);
			if (result.ec != std::errc{}) return false;
			line.remove_prefix(static_cast<size_t>(result.ptr - line.data()));
			return true;
		};
		if (!field(level) || !field(x) || !field(y)) continue;
		while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
		if (line.empty()) continue;
		flag = line.front();
		if (flag == 'v') Visit(level, x, y);
		else if (flag == 's') Mark(level, x, y);
	}
	// What was just read is not a change to write back.
	dirty_ = wasDirty;
}

}
