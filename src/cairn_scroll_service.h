#pragma once

#include "cairn_stones.h"

#include <D2RLPlugin/api.h>
#include <cstdint>
#include <optional>

namespace D2RAccess::CairnScrollService {

// One reading per actual opening; a late order packet can complete that reading.
class Reader {
public:
	std::optional<CairnStones::Order> Update(uint64_t session, bool open,
		std::optional<CairnStones::Order> order) {
		if (session != session_ || !open) {
			session_ = session;
			announced_ = false;
		}
		if (!open || announced_ || !order || !CairnStones::Valid(*order)) return {};
		announced_ = true;
		return order;
	}

private:
	uint64_t session_ = 0;
	bool announced_ = false;
};

void Tick(const D2RL::PluginContext* context, uint64_t session, bool ready, bool foreground);
void Reset();

}
