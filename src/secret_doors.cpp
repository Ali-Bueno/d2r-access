#include "secret_doors.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::SecretDoors {
namespace {
double Distance(Radar::Position a, Radar::Position b) { return std::hypot(a.x - b.x, a.y - b.y); }
bool Same(Radar::Position a, Radar::Position b) { return Distance(a, b) < 1; }
}

bool Draft::Felt(const std::vector<Radar::Position>& doors, Radar::Position player) {
	// One opened, or walked away from, may be felt again.
	std::erase_if(m_felt, [&](Radar::Position felt) {
		return Distance(felt, player) > FeltAgainBeyond
			|| std::none_of(doors.begin(), doors.end(), [&](Radar::Position door) { return Same(door, felt); });
	});
	bool news = false;
	for (const auto door : doors) {
		if (Distance(door, player) > FeltWithin) continue;
		if (std::any_of(m_felt.begin(), m_felt.end(), [&](Radar::Position felt) { return Same(door, felt); })) continue;
		m_felt.push_back(door);
		news = true;
	}
	return news;
}

}
