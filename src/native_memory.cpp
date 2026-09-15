#include "native_memory.h"

#include <windows.h>
#include <cstring>
#include <limits>

namespace D2RAccess::NativeMemory {
namespace {
// D2RCore 0x699509 / 0x314622: engine string = data, size, capacity|borrowed,
// then 16 inline bytes. The larger limit accommodates complete NPC speeches.
constexpr uint64_t BorrowedBit = uint64_t{1} << 63;
constexpr size_t InlineOffset = 0x18;
constexpr size_t InlineCapacity = 15;
constexpr size_t MaxTextBytes = 64 * 1024; // corruption guard, not a truncation limit
}

bool Readable(const void* address, size_t size) noexcept {
	uintptr_t cursor = reinterpret_cast<uintptr_t>(address);
	if (cursor == 0 || size == 0 || size > std::numeric_limits<uintptr_t>::max() - cursor) return false;
	const uintptr_t end = cursor + size;
	while (cursor < end) {
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) != sizeof(info)
			|| info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
		constexpr DWORD flags = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
			| PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		if ((info.Protect & flags) == 0) return false;
		const uintptr_t next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
		if (next <= cursor) return false;
		cursor = next;
	}
	return true;
}

bool Copy(uintptr_t address, void* output, size_t size) noexcept {
	if (!Readable(reinterpret_cast<void*>(address), size)) return false;
	__try {
		std::memcpy(output, reinterpret_cast<void*>(address), size);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool Executable(uintptr_t address) noexcept {
	MEMORY_BASIC_INFORMATION info{};
	return address != 0 && VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == sizeof(info)
		&& info.State == MEM_COMMIT && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0
		&& (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

bool ReadEngineString(uintptr_t address, std::string& output) {
	struct { uint64_t data, size, capacity; } header{};
	if (!Peek(address, header) || header.size > (header.capacity & ~BorrowedBit)
		|| header.size > MaxTextBytes || header.data == 0) return false;
	if (header.data == address + InlineOffset && header.size > InlineCapacity) return false;
	std::string result(static_cast<size_t>(header.size), '\0');
	if (header.size != 0 && !Copy(header.data, result.data(), result.size())) return false;
	output = std::move(result);
	return true;
}

std::string CString(uintptr_t address, size_t limit) {
	std::string result;
	for (size_t i = 0; i < limit; ++i) {
		char ch = 0;
		if (!Peek(address + i, ch)) return {};
		if (ch == '\0') return result;
		result += ch;
	}
	return {};
}

bool LooksLikeText(const std::string& text) {
	if (text.empty()) return false;
	for (const unsigned char ch : text) {
		if (ch < 0x20 && ch != '\t' && ch != '\r' && ch != '\n') return false;
	}
	return true;
}
}
