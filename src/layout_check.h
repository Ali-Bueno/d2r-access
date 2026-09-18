#pragma once

#include "async_log.h"
#include "core_slots.h"
#include "native_memory.h"

#include <D2RLPlugin/api.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

// A native reader's field offsets were derived from specific game functions, so
// it has to refuse an image whose code differs rather than read the wrong bytes.
// Each entry hashes one complete function; a game patch changes it, and so does
// a loader build that detours or rebuilds that function, which is why a failure
// names the loader as well as the function.
namespace D2RAccess::LayoutCheck {

// The loader redirects some of the game's own calls into code it allocates at
// run time — on 1.3.0-beta that block sits past the image's SizeOfImage, so its
// address is not a property of the game's code and need not even be the same on
// the next launch. Hashing it would make the fingerprint non-deterministic, so
// the 4-byte operand of every relative call or jump is zeroed first. What the
// fingerprint still proves is the instruction sequence the offsets were read
// from; a redirected call target was never part of that evidence.
inline uint64_t Hash(std::span<const uint8_t> bytes) {
	constexpr uint64_t Prime = 1099511628211ULL;
	constexpr uint8_t  RelativeCall = 0xE8, RelativeJump = 0xE9;
	uint64_t           hash = 14695981039346656037ULL; // FNV-1a, 64-bit
	for (size_t at = 0; at < bytes.size(); ++at) {
		const uint8_t byte = bytes[at];
		hash = (hash ^ byte) * Prime;
		if (byte != RelativeCall && byte != RelativeJump) continue;
		const size_t operand = (std::min)(size_t {4}, bytes.size() - at - 1);
		for (size_t skipped = 0; skipped < operand; ++skipped) hash = hash * Prime;
		at += operand;
	}
	return hash;
}

namespace Detail {
// A function whose first instruction jumps elsewhere: `jmp rel32` or the
// `jmp [rip+disp32]` thunk D2RLoader 1.3.0-beta leaves at RVA 0x2F5020.
inline bool IsJumpThunk(std::span<const uint8_t> bytes) {
	return bytes.size() >= 6 && (bytes[0] == 0xE9 || (bytes[0] == 0xFF && bytes[1] == 0x25));
}

inline void Append(std::string& list, uintptr_t rva) {
	char text[20]{};
	std::snprintf(text, sizeof(text), "%llX", static_cast<unsigned long long>(rva));
	if (!list.empty()) list += ", ";
	list += text;
}
}

// True when every entry still matches. Otherwise the warning names *every*
// function that changed, not just the first, together with the loader build —
// the three things a report from an unfamiliar build otherwise costs a debugger
// session to establish. `tag` is the log prefix ("scan") and `consequence` what
// the reader stops doing.
// `Evidence` is any table entry with `rva`, `size` and `hash` — each reader
// keeps its own, next to the offsets it guards.
template <typename Evidence, size_t Count>
bool Verify(const D2RL::PluginContext* context, uintptr_t base, const Evidence (&checks)[Count], const char* tag, const char* consequence) {
	std::string changed;
	std::string unreadable;
	size_t      changedCount = 0, unreadableCount = 0;
	for (const Evidence& check : checks) {
		std::vector<uint8_t> bytes(check.size);
		if (!NativeMemory::Copy(base + check.rva, bytes.data(), bytes.size())) {
			Detail::Append(unreadable, check.rva);
			++unreadableCount;
		} else if (Hash(bytes) != check.hash) {
			// An entry the reader only ever *calls* may legitimately have been
			// replaced by the loader with a jump into its own code: the call
			// still reaches an implementation of the same function, and the
			// loader's body is not ours to fingerprint. Anything else is a
			// change we have to refuse.
			if constexpr (requires { check.mayBeDetoured; }) {
				if (check.mayBeDetoured && Detail::IsJumpThunk(bytes)) {
					AsyncLog::InfoF(context, "[%s] The loader detours the function at RVA %llX; calling its replacement.", tag, static_cast<unsigned long long>(check.rva));
					continue;
				}
			}
			Detail::Append(changed, check.rva);
			++changedCount;
		}
	}
	if (changedCount == 0 && unreadableCount == 0) return true;

	char        counts[96]{};
	const char* build = D2RL::GetBuildVersion(context);
	std::string detail;
	// Both builds matter and neither implies the other: the game decides what the
	// code should be, the loader decides what it patches on top.
	if (changedCount != 0) {
		std::snprintf(counts, sizeof(counts), " %zu of %zu checks changed at RVA ", changedCount, Count);
		detail += counts;
		detail += changed;
		detail += '.';
	}
	if (unreadableCount != 0) {
		std::snprintf(counts, sizeof(counts), " %zu unreadable at RVA ", unreadableCount);
		detail += counts;
		detail += unreadable;
		detail += '.';
	}
	AsyncLog::WarnF(context, "[%s] Unsupported layout on game %s, loader %s; %s.%s", tag, build != nullptr ? build : "unknown", CoreSlots::LoaderVersion(), consequence, detail.c_str());
	return false;
}

}
