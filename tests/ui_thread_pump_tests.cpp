#include "allocation_failure.h"
#include "async_log.h"
#include "ui_thread_pump.h"
#include "test_support.h"
#include <windows.h>

namespace {
D2RL::Threads::Callback queued = nullptr;
void* queuedData = nullptr;
int ticks = 0, recoveries = 0;
bool failTick = true;
D2RAccess::CallbackFault fault = D2RAccess::CallbackFault::None;
D2RL::Threads::Result queueResult = D2RL::Threads::Result::Success;
auto Queue(const D2RL::PluginContext*, D2RL::Threads::Callback callback, void* data) noexcept -> D2RL::Threads::Result {
	if (queueResult == D2RL::Threads::Result::Success) { queued = callback; queuedData = data; }
	return queueResult;
}
const D2RL::ThreadService threads{ D2RL::ThreadServiceSize, D2RL::ThreadService::AbiVersion, Queue, Queue };
auto Query(const D2RL::PluginContext*, D2RL::ServiceId id, uint32_t, const void** service) noexcept -> D2RL::ServiceQueryResult {
	if (id != D2RL::ServiceId::Thread) return D2RL::ServiceQueryResult::Unavailable;
	*service = &threads;
	return D2RL::ServiceQueryResult::Success;
}
void Log(const D2RL::PluginContext*, const char*) noexcept {}
const D2RL::PluginApi api{ .apiSize = D2RL::PluginApiSize, .logInfo = Log, .logWarn = Log, .logError = Log, .queryService = Query };
const D2RL::PluginContext context{ .contextSize = sizeof(D2RL::PluginContext), .api = &api };
void Tick(const D2RL::PluginContext*) { ++ticks; if (failTick) throw std::bad_alloc(); }
void Recover(const D2RL::PluginContext*, D2RAccess::CallbackFault why) noexcept { ++recoveries; fault = why; }
void Dispatch() {
	Check(queued != nullptr, "pump has queued another UI update");
	const auto callback = queued; auto* data = queuedData;
	queued = nullptr;
	callback(&context, data);
}
}
int main() {
	using namespace D2RAccess;
	Check(AsyncLog::Initialize(), "diagnostic worker starts for real callback integration");
	Check(UiThreadPump::Start(&context, Tick, Recover), "UI pump starts");
	TestAllocation::fail = true; // includes the diagnostic allocation made in the catch path
	Dispatch();
	TestAllocation::fail = false;
	Check(ticks == 1 && recoveries == 1 && fault == CallbackFault::Allocation, "SDK noexcept callback contains allocation failure and invokes recovery");
	Dispatch();
	Check(ticks == 1 && recoveries == 1, "following UI frame skips the failed work during backoff");
	failTick = false;
	Sleep(static_cast<DWORD>(CallbackBoundary::RetryMs + 10));
	Dispatch();
	Check(ticks == 2, "accessibility tick resumes after memory becomes available");
	UiThreadPump::Stop();
	Dispatch();
	Check(ticks == 2 && !queued, "in-flight callback becomes harmless after stop");
	queueResult = D2RL::Threads::Result::Busy;
	Check(UiThreadPump::Start(&context, Tick, Recover) && !queued, "transient SDK queue failure allows a later retry");
	queueResult = D2RL::Threads::Result::Success;
	UiThreadPump::Kick(&context);
	Dispatch();
	Check(ticks == 3, "restarted pump is not held by the old recovery state");
	UiThreadPump::Stop();
	AsyncLog::Shutdown();
	Passed();
}
