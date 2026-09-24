#include "navigation_mode.h"

#include <atomic>

namespace D2RAccess::NavigationMode {
namespace { std::atomic<bool> fullAssistance{false}; }

bool FullAssistance() { return fullAssistance.load(std::memory_order_relaxed); }
void SetFullAssistance(bool enabled) { fullAssistance.store(enabled, std::memory_order_relaxed); }
}
