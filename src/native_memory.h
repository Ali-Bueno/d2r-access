#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace D2RAccess::NativeMemory {

// Bounded, fault-contained copies. Never retain a pointer into a game string.
bool Copy(uintptr_t address, void* output, size_t size) noexcept;
bool Executable(uintptr_t address) noexcept;
bool Readable(const void* address, size_t size) noexcept;
template <typename T> bool Peek(uintptr_t address, T& output) noexcept {
	return Copy(address, &output, sizeof(T));
}
bool ReadEngineString(uintptr_t address, std::string& output);
std::string CString(uintptr_t address, size_t limit);
bool LooksLikeText(const std::string& text);

}
