#pragma once

#include <memory>
#include <new>
#include <type_traits>

namespace D2RAccess {
// The loader normally calls service Stop/Shutdown before unloading the plugin.
// ExitProcess may bypass that path. Windows has already killed other threads
// when DLL static destructors run under the loader lock: joining them, or
// destroying a joinable std::thread, is unsafe there. Retain just the worker
// owner until process exit; Windows reclaims its storage/handles. Normal
// Stop/Shutdown still joins, flushes persistence and releases worker resources.
// No heap allocation and no destructor/loader-lock cleanup in this wrapper.
template<class T>
class ProcessLifetime {
public:
	ProcessLifetime() { ::new (static_cast<void*>(storage_)) T; }
	ProcessLifetime(const ProcessLifetime&) = delete;
	ProcessLifetime& operator=(const ProcessLifetime&) = delete;
	T& Get() noexcept { return *std::launder(reinterpret_cast<T*>(storage_)); }
	// Only from ordinary service shutdown, outside DllMain/process detach.
	// Recreate an empty owner so registration can be repeated without retaining
	// dynamic container memory after normal DLL unload.
	void Reset() { std::destroy_at(&Get()); ::new (static_cast<void*>(storage_)) T; }
private:
	alignas(T) unsigned char storage_[sizeof(T)];
};
static_assert(std::is_trivially_destructible_v<ProcessLifetime<int>>);
}
