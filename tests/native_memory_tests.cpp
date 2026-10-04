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
	SYSTEM_INFO system{}; GetSystemInfo(&system);
	page = VirtualAlloc(nullptr, system.dwPageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	Check(page != nullptr, "scoped read fixture allocates two native pages");
	const auto address = reinterpret_cast<uintptr_t>(page);
	DWORD previous = 0;
	{
		const ReadScope pass;
		*static_cast<char*>(page) = 'a';
		Check(Copy(address, buffer.data(), 1) && buffer[0] == 'a', "first scoped copy reads current contents");
		*static_cast<char*>(page) = 'b';
		{ const ReadScope nested; Check(Copy(address, buffer.data(), 1) && buffer[0] == 'b', "nested reads never cache contents"); }
		VirtualProtect(page, system.dwPageSize, PAGE_NOACCESS, &previous);
		Check(!Copy(address, buffer.data(), 1), "SEH contains a page permission change within a read pass");
	}
	VirtualProtect(page, system.dwPageSize, PAGE_READWRITE, &previous);
	{ const ReadScope pass; Check(Copy(address, buffer.data(), 1) && buffer[0] == 'b', "a new pass rechecks restored permissions"); }
	VirtualProtect(reinterpret_cast<void*>(address + system.dwPageSize), system.dwPageSize, PAGE_NOACCESS, &previous);
	{
		const ReadScope pass;
		Check(Copy(address, buffer.data(), 1), "readable first page can be cached");
		Check(!Copy(address + system.dwPageSize - 1, buffer.data(), 2), "cached permission does not authorize a copy into an unreadable next page");
	}
	VirtualFree(page, 0, MEM_RELEASE);
	{ const ReadScope pass; Check(!Copy(address, buffer.data(), 1), "freed memory is revalidated on the next pass"); }
	// The stash's individual widgets can touch more than the old 64-region
	// cache. Alternating protection makes distinct VirtualQuery regions.
	constexpr size_t widgetPages = 96;
	page = VirtualAlloc(nullptr, system.dwPageSize * widgetPages, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	Check(page != nullptr, "advanced-stash permission working set allocated");
	const auto widgetMemory = reinterpret_cast<uintptr_t>(page);
	for (size_t i = 1; i < widgetPages; i += 2)
		Check(VirtualProtect(reinterpret_cast<void*>(widgetMemory + i * system.dwPageSize), system.dwPageSize, PAGE_READONLY, &previous) != 0,
			"fixture widget pages have distinct protections");
	const auto code = reinterpret_cast<uintptr_t>(&main);
	{
		const ReadScope pass;
		const auto before = Checks();
		for (size_t i = 0; i < widgetPages; ++i) {
			Check(Copy(widgetMemory + i * system.dwPageSize, buffer.data(), 1), "widget data remains readable");
			Check(Executable(code), "native getter code is executable");
		}
		Check(Checks().count - before.count == widgetPages + 1, "the shared getter's executable permission is queried once across every widget");
		const auto warm = Checks();
		{ const ReadScope nested;
			for (size_t i = 0; i < widgetPages; ++i) {
				Check(Copy(widgetMemory + i * system.dwPageSize, buffer.data(), 1), "nested widget read remains current");
				Check(Executable(code), "nested native getter remains executable");
			}
		}
		Check(Checks().count == warm.count, "a second rebuild in one update reuses the entire stash working set without Windows queries");
		Check(!Executable(widgetMemory), "cached readable widget memory is never mistaken for executable code");
	}
	VirtualProtect(page, system.dwPageSize, PAGE_EXECUTE_READ, &previous);
	{ const ReadScope pass;
		Check(Copy(widgetMemory, buffer.data(), 1) && Executable(widgetMemory), "read and execute permissions share one region with their actual protection flags");
	}
	VirtualProtect(page, system.dwPageSize, PAGE_READWRITE, &previous);
	{ const ReadScope pass; Check(!Executable(widgetMemory), "new UI passes revalidate changed executable permissions"); }
	VirtualFree(page, 0, MEM_RELEASE);
	Passed();
}
