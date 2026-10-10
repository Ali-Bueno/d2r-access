#pragma once

#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace D2RAccess {
enum class CallbackFault { None, Allocation, Unexpected };

// A failed snapshot is unavailable, never an empty/complete map. Let its RAII
// state unwind, cancel pending actions without allocation, then retry later.
// This helper itself does not allocate, log, speak, or invoke an SDK service.
class CallbackBoundary {
public:

	template<class Work, class Recover>
	bool Run(uint64_t now, Work&& work, Recover&& recover) noexcept {
		static_assert(std::is_nothrow_invocable_v<Recover, CallbackFault>);
		if (now < retryAt_) return false;
		try {
			std::forward<Work>(work)();
			return true;
		} catch (const std::bad_alloc&) {
			fault_ = CallbackFault::Allocation;
		} catch (...) {
			fault_ = CallbackFault::Unexpected;
		}
		++failures_;
		retryAt_ = now + RetryMs;
		std::forward<Recover>(recover)(fault_);
		return false;
	}
	CallbackFault Fault() const noexcept { return fault_; }
	uint64_t Failures() const noexcept { return failures_; }
	void Reset() noexcept { *this = {}; }
	static constexpr uint64_t RetryMs = 1000;
private:
	uint64_t retryAt_ = 0, failures_ = 0;
	CallbackFault fault_ = CallbackFault::None;
};
}
