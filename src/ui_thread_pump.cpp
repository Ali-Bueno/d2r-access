#include "ui_thread_pump.h"

#include <atomic>

namespace D2RAccess::UiThreadPump {

namespace {

const D2RL::ThreadServiceV1* g_threads = nullptr;
TickFn                       g_tick    = nullptr;
std::atomic<bool>            g_enabled = false; // Start() called, Stop() not yet
std::atomic<bool>            g_queued  = false; // a tick is waiting in the UI queue

bool Queue(const D2RL::PluginContext* context);

void __cdecl OnUiTick(const D2RL::PluginContext* context, void*) noexcept {
	g_queued.store(false);
	if (!g_enabled.load() || context == nullptr) {
		return;
	}
	if (g_tick != nullptr) {
		g_tick(context);
	}
	(void)Queue(context);
}

// Busy/Unavailable are transient (queue full, UI root not created yet); the
// next Kick from a UI message re-arms the pump.
bool Queue(const D2RL::PluginContext* context) {
	if (!g_enabled.load() || g_queued.load()) {
		return g_queued.load();
	}
	const D2RL::Threads::Result result = g_threads->runOnUiThread(context, OnUiTick, nullptr);
	// Kick runs on every UI message, so report only transitions, never per call.
	static D2RL::Threads::Result lastLogged = D2RL::Threads::Result::Success;
	if (result != lastLogged) {
		lastLogged = result;
		if (result == D2RL::Threads::Result::Success) {
			context->LogInfo("UI pump: running.");
		} else {
			D2RL::LogWarnF(context, "UI pump: runOnUiThread returned %u; will retry on the next UI message.", static_cast<unsigned>(result));
		}
	}
	if (result != D2RL::Threads::Result::Success) {
		return false;
	}
	g_queued.store(true);
	return true;
}

}

bool Start(const D2RL::PluginContext* context, TickFn tick) {
	if (context == nullptr || tick == nullptr || g_enabled.load()) {
		return false;
	}
	const D2RL::ServiceQueryResult query = context->QueryService(D2RL::ServiceId::Thread, D2RL::ThreadServiceV1Version, &g_threads);
	if (query != D2RL::ServiceQueryResult::Success || !D2RL::HasThreadServiceV1Field(g_threads, D2RL::ThreadServiceV1RequiredSize)) {
		D2RL::LogErrorF(context, "UI pump: Thread service query failed (%u).", static_cast<unsigned>(query));
		g_threads = nullptr;
		return false;
	}
	g_tick = tick;
	g_enabled.store(true);
	(void)Queue(context);
	return true;
}

void Kick(const D2RL::PluginContext* context) {
	if (g_enabled.load() && context != nullptr) {
		(void)Queue(context);
	}
}

void Stop() {
	g_enabled.store(false);
	g_tick    = nullptr;
	g_threads = nullptr;
}

}
