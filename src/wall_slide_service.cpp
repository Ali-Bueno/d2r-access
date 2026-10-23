#include "wall_slide_service.h"
#include "native_wall_slide.h"
#include "async_log.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "wall_bump.h"

#include <windows.h>
#include <atomic>
#include <string_view>

namespace D2RAccess::WallSlideService {
namespace {
enum class Command { None, On, Off, Status };
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{false};
std::atomic<uint64_t> g_prevented{0}, g_attempts{0}, g_squeezed{0};
std::atomic<uint8_t> g_squeezes{0};
std::atomic<bool> g_narrowSlide{true};
std::atomic<float> g_pushX{0}, g_pushY{0};
const D2RL::PluginContext* g_context = nullptr;
std::atomic<bool> g_supported{false};
uint64_t g_reported = 0, g_reportAt = 0;
// Diagnostics are aggregated on the UI thread, at most once per five seconds.
constexpr uint64_t ReportIntervalMs = 5000;

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

void Register(const D2RL::PluginContext* context, bool nativeHooks) {
	g_context = context;
	g_supported = nativeHooks && NativeWallSlide::Initialize(context);
	g_enabled = g_supported && RadarConfig::ReadEnabled(context, "[movement]").value_or(true);
	(void)context->RegisterConsoleCommand("access-movement", Console, "Prevent directional wall sliding: on|off|status.");
	AsyncLog::InfoF(context, "[movement] prevention enabled=%u supported=%u", g_enabled ? 1u : 0u, g_supported ? 1u : 0u);
}

void Tick() {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	if (command == Command::On || command == Command::Off) g_enabled = g_supported && command == Command::On;
	const uint64_t prevented = g_prevented.load(std::memory_order_relaxed);
	const uint64_t now = GetTickCount64();
	if (command != Command::None || (prevented != g_reported && now >= g_reportAt)) {
		AsyncLog::InfoF(g_context, "[movement] prevention enabled=%u supported=%u blocked_alternatives=%llu narrow_slides=%llu",
			g_enabled ? 1u : 0u, g_supported ? 1u : 0u, static_cast<unsigned long long>(prevented),
			static_cast<unsigned long long>(g_squeezed.load(std::memory_order_relaxed)));
		g_reported = prevented;
		g_reportAt = now + ReportIntervalMs;
	}
	if (command != Command::None) {
		const auto& text = Strings::Current();
		ScreenReader::Speak(!g_supported ? text.movementUnavailable : g_enabled ? text.movementEnabled : text.movementDisabled, true);
	}
}

bool Prevent(uintptr_t pathContext, const float* push, bool retry) noexcept {
	if (!g_supported || !NativeWallSlide::IsLocalDirectionalAttempt(pathContext)) return false;
	if (push) {
		g_pushX.store(push[0], std::memory_order_relaxed);
		g_pushY.store(push[1], std::memory_order_relaxed);
	}
	g_attempts.fetch_add(1, std::memory_order_relaxed);
	if (!g_enabled) return false;
	if (retry && push && g_narrowSlide.load(std::memory_order_relaxed)
		&& (g_squeezes.load(std::memory_order_relaxed) >> WallBump::Way({push[0], push[1]}) & 1u)) {
		g_squeezed.fetch_add(1, std::memory_order_relaxed);
		return false;
	}
	g_prevented.fetch_add(1, std::memory_order_relaxed);
	return true;
}

Blocked LastBlocked() noexcept {
	return {g_attempts.load(std::memory_order_relaxed), g_pushX.load(std::memory_order_relaxed), g_pushY.load(std::memory_order_relaxed)};
}

bool Enabled() noexcept { return g_enabled; }
void SetSqueezes(uint8_t ways) noexcept { g_squeezes.store(ways, std::memory_order_relaxed); }
bool NarrowSlide() noexcept { return g_narrowSlide.load(std::memory_order_relaxed); }
void SetNarrowSlide(bool on) noexcept { g_narrowSlide.store(on, std::memory_order_relaxed); }

void Unregister() {
	g_enabled = false;
	g_command = Command::None;
	g_context = nullptr;
	g_supported = false;
	g_prevented = g_attempts = g_squeezed = 0;
	g_squeezes = 0;
	g_narrowSlide = true;
	g_pushX = g_pushY = 0;
	g_reported = g_reportAt = 0;
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
void SetEnabled(bool enabled) { g_command = enabled ? Command::On : Command::Off; }
}
