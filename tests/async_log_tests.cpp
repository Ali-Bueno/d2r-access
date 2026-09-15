#include "async_log.h"
#include "test_support.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
std::promise<void> entered, release, completed;
std::shared_future<void> unblock = release.get_future().share();
std::mutex mutex;
std::vector<std::string> messages;
std::vector<std::thread::id> threads;

void Write(const char* level, const char* text) noexcept {
	if (std::string_view(text) == "block") { entered.set_value(); unblock.wait(); }
	{
		std::lock_guard lock(mutex);
		messages.emplace_back(std::string(level) + text);
		threads.push_back(std::this_thread::get_id());
	}
	if (std::string_view(text) == "barrier") completed.set_value();
}
void Info(const D2RL::PluginContext*, const char* text) noexcept { Write("info:", text); }
void Warn(const D2RL::PluginContext*, const char* text) noexcept { Write("warn:", text); }
void Error(const D2RL::PluginContext*, const char* text) noexcept { Write("error:", text); }
const D2RL::PluginApi api{ .apiSize = D2RL::PluginApiSize, .logInfo = Info, .logWarn = Warn, .logError = Error };
const D2RL::PluginContext context{ .contextSize = sizeof(D2RL::PluginContext), .api = &api };
}

int main() {
	using namespace D2RAccess;
	Check(AsyncLog::Initialize(), "diagnostic worker starts");
	Check(AsyncLog::Initialize(), "initialization is idempotent");
	AsyncLog::Info(&context, "block");
	Check(entered.get_future().wait_for(2s) == std::future_status::ready, "SDK sink is blocked in the worker");
	const auto producerId = std::this_thread::get_id();
	auto producer = std::async(std::launch::async, [] {
		for (int i = 0; i < 1000; ++i) AsyncLog::InfoF(&context, "item %d", i);
		std::string owned = "owned text";
		AsyncLog::Info(&context, owned.c_str());
		owned = "changed";
		AsyncLog::WarnF(&context, "warning %d", 42);
		AsyncLog::Error(&context, "failure");
		AsyncLog::Info(nullptr, "ignored");
		AsyncLog::Info(&context, nullptr);
		AsyncLog::InfoF(&context, nullptr);
		AsyncLog::Info(&context, "barrier");
	});
	Check(producer.wait_for(2s) == std::future_status::ready, "producers finish while the SDK sink remains blocked");
	producer.get();
	release.set_value();
	Check(completed.get_future().wait_for(2s) == std::future_status::ready, "latest diagnostics survive a saturated queue");
	AsyncLog::Shutdown();
	Check(messages.size() == 257, "blocked disk work has a bounded 256-message backlog");
	Check(messages.front() == "info:block" && messages.back() == "info:barrier", "SDK sink retains message order");
	Check(std::find(messages.begin(), messages.end(), "info:owned text") != messages.end(), "queued text is copied before the caller changes it");
	Check(std::find(messages.begin(), messages.end(), "warn:warning 42") != messages.end()
		&& std::find(messages.begin(), messages.end(), "error:failure") != messages.end(), "formatting and severity reach the framework logger");
	Check(std::none_of(threads.begin(), threads.end(), [=](auto id) { return id == producerId; }), "SDK logging never runs on the caller thread");
	AsyncLog::Info(&context, "after shutdown");
	Check(messages.size() == 257, "shutdown rejects callbacks before their context can become invalid");
	Check(AsyncLog::Initialize(), "diagnostic worker can restart");
	AsyncLog::Shutdown();
	Passed();
}
