#pragma once

#include "cairn_stones.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

namespace D2RAccess::CairnStones {

// One deciphered-scroll packet is valid only for the lifecycle session that received it.
class OrderCache {
public:
	void Reset(uint64_t session, bool ready) {
		std::lock_guard lock(mutex_);
		++epoch_;
		session_ = session;
		ready_ = ready;
		order_.reset();
	}

	std::optional<uint64_t> BeginCapture() const {
		std::lock_guard lock(mutex_);
		if (!ready_ || session_ == 0) return {};
		return epoch_;
	}

	bool CompleteCapture(uint64_t epoch, const std::array<uint32_t, Count>& raw) {
		Order order{};
		for (size_t i = 0; i < Count; ++i) {
			if (raw[i] >= Count) return false;
			order[i] = static_cast<uint16_t>(raw[i]);
		}
		if (!Valid(order)) return false;
		std::lock_guard lock(mutex_);
		if (!ready_ || session_ == 0 || epoch != epoch_) return false;
		if (order_ == order) return false;
		order_ = order;
		return true;
	}

	std::optional<Order> Read(uint64_t session) const {
		std::lock_guard lock(mutex_);
		if (!ready_ || session == 0 || session != session_) return {};
		return order_;
	}

private:
	mutable std::mutex mutex_;
	uint64_t epoch_ = 0;
	uint64_t session_ = 0;
	bool ready_ = false;
	std::optional<Order> order_;
};

}
