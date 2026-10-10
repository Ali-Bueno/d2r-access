#pragma once
#include <cstddef>

namespace TestAllocation {
// Thread-local injection leaves worker/test framework allocations unaffected.
extern thread_local bool fail;
extern thread_local size_t minimum;
extern thread_local size_t rejected;
}
