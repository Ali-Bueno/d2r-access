#include "screen_reader.h"
#include "process_lifetime.h"
#include "async_log.h"

#include <prism.h>
#include <windows.h>

#include <algorithm>
#include <atomic>

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace D2RAccess::ScreenReader {

namespace {

// prism.dll lives in a subfolder of the plugins directory: D2RLoader treats
// every DLL directly inside d2rloader/plugins as a plugin and rejects it.
constexpr const wchar_t* PrismRelativePath = L"d2r-access\\prism.dll";

// Function-pointer types taken from prism.h so a signature change in a new
// PRISM release is a compile error here instead of a silent ABI mismatch.
struct PrismApi {
	decltype(&prism_config_init)           configInit          = nullptr;
	decltype(&prism_init)                  init                = nullptr;
	decltype(&prism_shutdown)              shutdown            = nullptr;
	decltype(&prism_registry_acquire_best) registryAcquireBest = nullptr;
	decltype(&prism_backend_initialize)    backendInitialize   = nullptr;
	decltype(&prism_backend_name)          backendName         = nullptr;
	decltype(&prism_backend_speak)         backendSpeak        = nullptr;
	decltype(&prism_backend_stop)          backendStop         = nullptr;
	decltype(&prism_backend_free)          backendFree         = nullptr;
	decltype(&prism_error_string)          errorString         = nullptr;
};

// Everything PRISM-related is touched only by the speech thread: a speak
// request to NVDA is a synchronous call that measured 300-800 ms
// (2026-09-15), which stalled the game's UI thread on every announcement.
struct State {
	HMODULE       library = nullptr;
	PrismApi      api {};
	PrismContext* context = nullptr;
	PrismBackend* backend = nullptr;
	std::string   lastError;   // written by the speech thread during Initialize, read after
	std::string   backendName; // copied once so callers never touch PRISM
};

struct Request {
	std::string text;
	bool        interrupt = false;
	bool        stop      = false; // Stop(): silence without speaking
};

State                   g_state {};
ProcessLifetime<std::thread> g_threadOwner;
std::thread&            g_thread = g_threadOwner.Get();
std::mutex              g_mutex;
std::condition_variable g_wake;
std::deque<Request>     g_queue;
bool                    g_quit      = false;
std::atomic<unsigned long long> g_lastSpoken{0};
bool                    g_available = false; // backend acquired (set by Initialize, cleared by Shutdown)

template <typename Fn>
bool Resolve(HMODULE library, const char* name, Fn& out) {
	out = reinterpret_cast<Fn>(GetProcAddress(library, name));
	return out != nullptr;
}

bool ResolveApi(HMODULE library, PrismApi& api) {
	return Resolve(library, "prism_config_init", api.configInit)
		&& Resolve(library, "prism_init", api.init)
		&& Resolve(library, "prism_shutdown", api.shutdown)
		&& Resolve(library, "prism_registry_acquire_best", api.registryAcquireBest)
		&& Resolve(library, "prism_backend_initialize", api.backendInitialize)
		&& Resolve(library, "prism_backend_name", api.backendName)
		&& Resolve(library, "prism_backend_speak", api.backendSpeak)
		&& Resolve(library, "prism_backend_stop", api.backendStop)
		&& Resolve(library, "prism_backend_free", api.backendFree)
		&& Resolve(library, "prism_error_string", api.errorString);
}

std::string Narrow(const std::wstring& text) {
	std::string result;
	result.reserve(text.size());
	for (const wchar_t c : text) {
		result.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return result;
}

std::wstring BuildPrismPath(const wchar_t* pluginDirectory) {
	std::wstring path(pluginDirectory);
	if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
		path += L'\\';
	}
	return path + PrismRelativePath;
}

// Speech thread only.
void ReleasePrism() {
	if (g_state.backend != nullptr) {
		g_state.api.backendFree(g_state.backend);
		g_state.backend = nullptr;
	}
	if (g_state.context != nullptr) {
		g_state.api.shutdown(g_state.context);
		g_state.context = nullptr;
	}
	if (g_state.library != nullptr) {
		FreeLibrary(g_state.library);
		g_state.library = nullptr;
	}
	g_state.api = {};
}

// Speech thread only. Loads prism.dll and acquires the best backend.
bool AcquirePrism(const std::wstring& path) {
	g_state.library = LoadLibraryW(path.c_str());
	if (g_state.library == nullptr) {
		char detail[64] {};
		std::snprintf(detail, sizeof(detail), " (LoadLibrary error %lu)", GetLastError());
		g_state.lastError = "cannot load " + Narrow(path) + detail;
		return false;
	}
	if (!ResolveApi(g_state.library, g_state.api)) {
		g_state.lastError = "prism.dll is missing an expected export";
		ReleasePrism();
		return false;
	}
	PrismConfig config = g_state.api.configInit();
	g_state.context    = g_state.api.init(&config);
	if (g_state.context == nullptr) {
		g_state.lastError = "prism_init returned null";
		ReleasePrism();
		return false;
	}
	g_state.backend = g_state.api.registryAcquireBest(g_state.context);
	if (g_state.backend == nullptr) {
		g_state.lastError = "no screen reader backend available (is NVDA/JAWS running?)";
		ReleasePrism();
		return false;
	}
	// Since PRISM v0.18 acquire_best hands out a shared backend that is already
	// initialized, so ALREADY_INITIALIZED is the normal outcome here, not a failure.
	const PrismError error = g_state.api.backendInitialize(g_state.backend);
	if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
		const char* name  = g_state.api.backendName(g_state.backend);
		g_state.lastError = std::string("backend ") + (name != nullptr ? name : "?") + " failed to initialize: " + g_state.api.errorString(error);
		ReleasePrism();
		return false;
	}
	const char* name    = g_state.api.backendName(g_state.backend);
	g_state.backendName = name != nullptr ? name : "";
	g_state.lastError.clear();
	return true;
}

void SpeechThread(std::wstring path, std::promise<bool> ready) {
	ready.set_value(AcquirePrism(path));
	for (;;) {
		Request request;
		{
			std::unique_lock<std::mutex> lock(g_mutex);
			g_wake.wait(lock, [] { return g_quit || !g_queue.empty(); });
			if (g_quit && g_queue.empty()) {
				break;
			}
			request = std::move(g_queue.front());
			g_queue.pop_front();
		}
		if (g_state.backend == nullptr) {
			continue;
		}
		if (request.stop) {
			(void)g_state.api.backendStop(g_state.backend);
		} else {
			(void)g_state.api.backendSpeak(g_state.backend, request.text.c_str(), request.interrupt);
		}
	}
	ReleasePrism();
}

std::atomic<const D2RL::PluginContext*> g_log{nullptr};

}

bool Initialize(const wchar_t* pluginDirectory) {
	if (g_available) {
		return true;
	}
	if (pluginDirectory == nullptr) {
		g_state.lastError = "plugin directory unknown";
		return false;
	}
	Shutdown(); // a previous failed attempt may have left the thread running

	// Absolute path: the game's DLL search path must not decide which prism.dll loads.
	std::promise<bool> ready;
	std::future<bool>  result = ready.get_future();
	g_quit                    = false;
	g_thread                  = std::thread(SpeechThread, BuildPrismPath(pluginDirectory), std::move(ready));
	g_available               = result.get();
	if (!g_available) {
		Shutdown();
	}
	return g_available;
}

void Shutdown() {
	g_log = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_quit = true;
		g_queue.clear();
	}
	g_wake.notify_all();
	if (g_thread.joinable()) {
		g_thread.join();
	}
	g_available = false;
}

bool IsAvailable() {
	return g_available;
}

const char* LastError() {
	return g_state.lastError.c_str();
}

const char* BackendName() {
	return g_available ? g_state.backendName.c_str() : "";
}

unsigned long long LastSpoken() { return g_lastSpoken.load(); }

std::mutex g_swallowMutex;
std::vector<std::pair<std::string, unsigned long long>> g_swallow;

void LogTo(const D2RL::PluginContext* context) { g_log = context; }

void Swallow(std::string_view text, unsigned long long untilMs) {
	if (text.empty()) return;
	std::lock_guard<std::mutex> lock(g_swallowMutex);
	g_swallow.emplace_back(std::string(text), untilMs);
}

void Speak(std::string_view text, bool interrupt) {
	if (!text.empty()) {
		std::lock_guard<std::mutex> lock(g_swallowMutex);
		const unsigned long long now = GetTickCount64();
		std::erase_if(g_swallow, [&](const auto& entry) { return now > entry.second; });
		const auto found = std::find_if(g_swallow.begin(), g_swallow.end(), [&](const auto& entry) { return entry.first == text; });
		if (found != g_swallow.end()) {
			g_swallow.erase(found);
			if (const auto* log = g_log.load()) AsyncLog::InfoF(log, "[speech] held back: %.*s", static_cast<int>(text.size()), text.data());
			return;
		}
	}
	if (const auto* log = g_log.load(); log && !text.empty())
		AsyncLog::InfoF(log, "[speech] %s: %.*s", interrupt ? "interrupts" : "queued", static_cast<int>(text.size()), text.data());
	if (!text.empty()) g_lastSpoken = GetTickCount64();
	if (!g_available || text.empty()) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		if (interrupt) {
			g_queue.clear(); // what was waiting is now stale
		}
		g_queue.push_back({ std::string(text), interrupt, false });
	}
	g_wake.notify_one();
}

void Stop() {
	if (!g_available) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_queue.clear();
		g_queue.push_back({ {}, false, true });
	}
	g_wake.notify_one();
}

}
