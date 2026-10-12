#include "async_log.h"
#include "ui_thread_pump.h"

#include <windows.h>

#include <atomic>

namespace D2RAccess::UiThreadPump {

namespace {

const D2RL::ThreadService* g_threads = nullptr;
TickFn                       g_tick    = nullptr;
RecoveryFn                   g_recover = nullptr;
CallbackBoundary             g_boundary;
std::atomic<bool>            g_enabled = false; // Start() called, Stop() not yet
std::atomic<bool>            g_queued  = false; // a tick is waiting in the UI queue

bool Queue(const D2RL::PluginContext* context);

// The SDK only promises that a queued callback runs "during a later
// root-widget update"; how often that is decides how quickly focus changes
// are noticed, so the tick rate and the longest gap are logged periodically.
constexpr unsigned long long StatsIntervalMs = 5000;

void LogTickStats(const D2RL::PluginContext* context) {
	static unsigned long long windowStart = 0, lastTick = 0, maxGap = 0;
	static unsigned           ticks = 0;
	const unsigned long long  now   = GetTickCount64();
	if (windowStart == 0) {
		windowStart = lastTick = now;
	}
	maxGap   = lastTick != now && now - lastTick > maxGap ? now - lastTick : maxGap;
	lastTick = now;
	++ticks;
	if (now - windowStart >= StatsIntervalMs) {
		D2RAccess::AsyncLog::InfoF(context, "[pump] %u ticks in %llu ms, longest gap %llu ms", ticks, now - windowStart, maxGap);
		windowStart = now;
		ticks       = 0;
		maxGap      = 0;
	}
}

void __cdecl OnUiTick(const D2RL::PluginContext* context, void*) noexcept {
	g_queued.store(false);
	if (!g_enabled.load() || context == nullptr) {
		return;
	}
	const auto failures = g_boundary.Failures();
	(void)g_boundary.Run(GetTickCount64(), [&] {
		LogTickStats(context);
		if (g_tick != nullptr) {
			const auto start = GetTickCount64();
			g_tick(context);
			const auto elapsed = GetTickCount64() - start;
			// Separate accessibility work from gaps between root-widget updates.
			// Limit reports so diagnostics cannot turn a slow tick into log spam.
			constexpr ULONGLONG SlowTickMs = 30;
			static ULONGLONG lastReport = 0;
			if (elapsed >= SlowTickMs && (!lastReport || start - lastReport >= StatsIntervalMs)) {
				D2RAccess::AsyncLog::WarnF(context, "[pump] slow accessibility tick: %llu ms", elapsed);
				lastReport = start;
			}
		}
	}, [context](CallbackFault fault) noexcept {
		if (g_recover) g_recover(context, fault);
	});
	if (g_boundary.Failures() != failures) {
		D2RAccess::AsyncLog::WarnF(context, "[pump] %s; pending mod actions cancelled, retry in one second (failure %llu).",
			g_boundary.Fault() == CallbackFault::Allocation ? "allocation failed" : "accessibility callback threw",
			static_cast<unsigned long long>(g_boundary.Failures()));
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
			D2RAccess::AsyncLog::Info(context, "UI pump: running.");
		} else {
			D2RAccess::AsyncLog::WarnF(context, "UI pump: runOnUiThread returned %u; will retry on the next UI message.", static_cast<unsigned>(result));
		}
	}
	if (result != D2RL::Threads::Result::Success) {
		return false;
	}
	g_queued.store(true);
	return true;
}

}

bool Start(const D2RL::PluginContext* context, TickFn tick, RecoveryFn recover) {
	if (context == nullptr || tick == nullptr || g_enabled.load()) {
		return false;
	}
	const D2RL::ServiceQueryResult query = context->QueryService(&g_threads);
	if (query != D2RL::ServiceQueryResult::Success || !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize)) {
		D2RAccess::AsyncLog::ErrorF(context, "UI pump: Thread service query failed (%u).", static_cast<unsigned>(query));
		g_threads = nullptr;
		return false;
	}
	g_tick = tick;
	g_recover = recover;
	g_boundary.Reset();
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
	g_recover = nullptr;
	g_threads = nullptr;
}

}
