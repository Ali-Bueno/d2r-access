#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// The Assassin's martial-arts charges as they change (a tester, 2026-11-02). docs/assassin-charges.md
namespace D2RAccess::ChargeTracker {

// How long after a swing charges falling to none count as released by it, not run out: the
// game drops them as the blow lands, while the swing may already be over at the next read.
inline constexpr uint64_t ReleaseWindowMs = 500;

// A skill's charges now; 0 means they ran out (a release is not news: the player made it).
struct Change {
	size_t index;
	int count;
};

class Tracker {
public:
	// `counts` per charge-up skill and whether the player is swinging at this read. The first
	// read after Reset is the baseline and says nothing.
	std::vector<Change> Update(std::span<const int> counts, bool swinging, uint64_t now) {
		std::vector<Change> changes;
		if (swinging) m_swungAt = now, m_swung = true;
		const bool released = m_swung && now - m_swungAt <= ReleaseWindowMs;
		if (m_known && m_counts.size() == counts.size())
			for (size_t i = 0; i < counts.size(); ++i) {
				const int was = m_counts[i], count = counts[i] < 0 ? 0 : counts[i];
				if (count == was || (count == 0 && released)) continue;
				changes.push_back({i, count});
			}
		m_counts.assign(counts.begin(), counts.end());
		for (auto& count : m_counts) if (count < 0) count = 0;
		m_known = true;
		return changes;
	}
	void Reset() {
		m_counts.clear();
		m_known = m_swung = false;
		m_swungAt = 0;
	}

private:
	std::vector<int> m_counts;
	bool m_known = false, m_swung = false;
	uint64_t m_swungAt = 0;
};

}
