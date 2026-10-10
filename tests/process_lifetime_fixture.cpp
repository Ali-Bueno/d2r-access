#include "process_lifetime.h"
#include <thread>
#include <windows.h>

namespace {
struct Owner {
	std::thread worker;
	HANDLE stop = nullptr;
	// Models the exploration store's normal RAII destructor. It must never
	// execute during process detach with a killed/joinable worker.
	~Owner() { if (worker.joinable()) worker.join(); }
};
D2RAccess::ProcessLifetime<Owner> retained;
}
extern "C" __declspec(dllexport) bool StartWorker() noexcept {
	auto& owner = retained.Get();
	if (owner.worker.joinable()) return false;
	owner.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!owner.stop) return false;
	try { owner.worker = std::thread([stop = owner.stop] { WaitForSingleObject(stop, INFINITE); }); }
	catch (...) { CloseHandle(owner.stop); owner.stop = nullptr; return false; }
	return true;
}
extern "C" __declspec(dllexport) bool StopWorker() noexcept {
	auto& owner = retained.Get();
	if (!owner.worker.joinable()) return true;
	SetEvent(owner.stop);
	try { owner.worker.join(); } catch (...) { return false; }
	CloseHandle(owner.stop); owner.stop = nullptr;
	retained.Reset(); // exercising ordinary owner destruction outside loader lock
	return true;
}
