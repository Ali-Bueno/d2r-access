#include "native_memory.h"
#include "test_support.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <limits>

using namespace D2RAccess::NativeMemory;
struct EngineString {
	uintptr_t data;
	uint64_t size, capacity;
	char inlineBytes[16];
};

int main() {
	std::string output;
	EngineString value{};
	value.data = reinterpret_cast<uintptr_t>(value.inlineBytes);
	value.size = 6;
	value.capacity = (uint64_t{1} << 63) | 15;
	std::memcpy(value.inlineBytes, "¡Hola", 6);
	Check(ReadEngineString(reinterpret_cast<uintptr_t>(&value), output) && output == "¡Hola", "borrowed inline UTF-8 string");
	std::string longText(12000, 'a');
	longText += " final de la historia";
	value.data = reinterpret_cast<uintptr_t>(longText.data());
	value.size = value.capacity = longText.size();
	Check(ReadEngineString(reinterpret_cast<uintptr_t>(&value), output) && output == longText, "long game text is copied in full");
	value.size = 0;
	Check(ReadEngineString(reinterpret_cast<uintptr_t>(&value), output) && output.empty(), "empty string clears the result");
	value.size = value.capacity + 1;
	Check(!ReadEngineString(reinterpret_cast<uintptr_t>(&value), output), "size exceeding capacity is rejected");
	value.size = value.capacity = 65537;
	Check(!ReadEngineString(reinterpret_cast<uintptr_t>(&value), output), "unbounded text allocation is rejected");
	value.data = reinterpret_cast<uintptr_t>(value.inlineBytes);
	value.size = value.capacity = 16;
	Check(!ReadEngineString(reinterpret_cast<uintptr_t>(&value), output), "corrupt inline length is rejected");
	Check(!ReadEngineString(1, output), "invalid object pointer does not fault");
	value.data = 1;
	value.size = value.capacity = 12;
	Check(!ReadEngineString(reinterpret_cast<uintptr_t>(&value), output), "invalid string data does not fault");
	Check(!Readable(reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max() - 2), 12), "overflowing memory range is rejected");
	Check(CString(reinterpret_cast<uintptr_t>("TextBoxWidget"), 96) == "TextBoxWidget", "bounded native type name");
	Check(CString(reinterpret_cast<uintptr_t>("TextBoxWidget"), 4).empty(), "unterminated type name is rejected");
	Check(LooksLikeText("Diálogo\ncompleto") && !LooksLikeText(std::string("bad\x01", 4)), "engine text validation preserves newlines");
	void* page = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
	Check(page != nullptr, "unreadable test page allocated");
	std::array<char, 4> buffer{};
	Check(!Copy(reinterpret_cast<uintptr_t>(page), buffer.data(), buffer.size()), "no-access memory is never copied");
	VirtualFree(page, 0, MEM_RELEASE);
	Passed();
}
