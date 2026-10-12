#include "allocation_failure.h"
#include <cstdlib>
#include <new>

namespace TestAllocation {
thread_local bool fail = false;
thread_local size_t minimum = 0, rejected = 0;
}
void* operator new(size_t bytes) {
	if (TestAllocation::fail && bytes >= TestAllocation::minimum) {
		TestAllocation::rejected = bytes;
		throw std::bad_alloc();
	}
	if (void* result = std::malloc(bytes ? bytes : 1)) return result;
	throw std::bad_alloc();
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* address) noexcept { std::free(address); }
void operator delete[](void* address) noexcept { std::free(address); }
void operator delete(void* address, size_t) noexcept { std::free(address); }
void operator delete[](void* address, size_t) noexcept { std::free(address); }
