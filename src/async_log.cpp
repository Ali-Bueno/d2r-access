#include "async_log.h"
#include "process_lifetime.h"

#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace D2RAccess::AsyncLog {
namespace {
// Diagnostics may be dropped during a stalled disk or an explicit tree dump;
// they must neither grow without bound nor delay input or speech.
constexpr size_t MaxPendingMessages = 256;
struct Message { const D2RL::PluginContext* context; D2RL::LogFn write; std::string text; };
std::mutex g_mutex;
std::condition_variable g_wake;
std::deque<Message> g_pending;
ProcessLifetime<std::thread> g_threadOwner;
std::thread& g_thread = g_threadOwner.Get();
bool g_running = false;

void Run() {
	for (;;) {
		Message message;
		{
			std::unique_lock lock(g_mutex);
			g_wake.wait(lock, [] { return !g_running || !g_pending.empty(); });
			if (!g_running) return;
			message = std::move(g_pending.front());
			g_pending.pop_front();
		}
		// D2RCore 1.2.1-beta LogInfo -> 0x3D14F0 opens, writes, and calls
		// FlushFileBuffers for every line. Do not hold the producer lock here.
		message.write(message.context, message.text.c_str());
	}
}

void Queue(const D2RL::PluginContext* context, D2RL::LogFn write, const char* text) noexcept {
	if (!context || !write || !text) return;
	try {
		std::lock_guard lock(g_mutex);
		if (!g_running) return;
		if (g_pending.size() == MaxPendingMessages) g_pending.pop_front();
		g_pending.push_back({context, write, text});
	} catch (...) { return; } // diagnostic allocation must never kill a UI/worker callback
	g_wake.notify_one();
}

void Format(const D2RL::PluginContext* context, D2RL::LogFn write, const char* format, va_list args) {
	if (!format) return;
	char text[4096]{}; // bounded diagnostics; spoken text is not truncated
	std::vsnprintf(text, sizeof(text), format, args);
	Queue(context, write, text);
}
const D2RL::PluginApi* Api(const D2RL::PluginContext* context) { return context ? context->GetApi() : nullptr; }
}

bool Initialize() {
	std::lock_guard lock(g_mutex);
	if (g_running) return true;
	g_running = true;
	try { g_thread = std::thread(Run); }
	catch (...) { g_running = false; return false; }
	return true;
}

void Shutdown() {
	{
		std::lock_guard lock(g_mutex);
		g_running = false;
		g_pending.clear(); // no long diagnostic backlog on game exit
	}
	g_wake.notify_all();
	if (g_thread.joinable()) g_thread.join(); // context stays valid until the worker exits
}

void Info(const D2RL::PluginContext* context, const char* text) noexcept { Queue(context, D2RL::ApiLogInfo(Api(context)), text); }
void Warn(const D2RL::PluginContext* context, const char* text) noexcept { Queue(context, D2RL::ApiLogWarn(Api(context)), text); }
void Error(const D2RL::PluginContext* context, const char* text) noexcept { Queue(context, D2RL::ApiLogError(Api(context)), text); }
void InfoF(const D2RL::PluginContext* context, const char* format, ...) noexcept {
	va_list args; va_start(args, format); Format(context, D2RL::ApiLogInfo(Api(context)), format, args); va_end(args);
}
void WarnF(const D2RL::PluginContext* context, const char* format, ...) noexcept {
	va_list args; va_start(args, format); Format(context, D2RL::ApiLogWarn(Api(context)), format, args); va_end(args);
}
void ErrorF(const D2RL::PluginContext* context, const char* format, ...) noexcept {
	va_list args; va_start(args, format); Format(context, D2RL::ApiLogError(Api(context)), format, args); va_end(args);
}
}
