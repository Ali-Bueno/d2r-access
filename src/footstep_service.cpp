#include "footstep_service.h"
#include "native_footsteps.h"
#include "wall_slide_service.h"
#include "async_log.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <string_view>

namespace D2RAccess::FootstepService {
namespace {
enum class Command { None, On, Off, Toggle, Status };
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{false};
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService* g_input = nullptr;
D2RL::Input::ActionHandle g_action = D2RL::Input::InvalidHandle;
std::mutex g_mutex;
Footsteps::Filter g_filter;
bool g_supported = false, g_muted = false, g_seen = false;
uint64_t g_allowed = 0, g_suppressed = 0;
// Aggregate active sound events, at most once per five seconds; never per frame.
constexpr uint64_t ReportIntervalMs = 5000;
constexpr double PositionScale = uint64_t{1} << std::numeric_limits<uint16_t>::digits;
std::array<uint64_t, static_cast<size_t>(Footsteps::Reason::Count)> g_reasons{};
Footsteps::Sample g_lastSample{};
Footsteps::Decision g_lastDecision{};
uint64_t g_reportAt = 0, g_maxStationary = 0, g_unresolvedForeign = 0;
uint64_t g_refusedSeen = 0; // the wall-slide hook's refused pushes at the last local footfall
double g_travel = 0, g_netX = 0, g_netY = 0;
bool g_dirty = false;
// The player's own footfall, scaled once the caller has set its volume (a tester, 2026-10-13).
// A sound handle is an id the game looks up among its instances, so a step that
// has ended is simply not found; one older than this is let go all the same.
constexpr uint64_t PendingSoundMs = 250;
std::atomic<int> g_volumeLevel{Footsteps::NeutralVolumeLevel};
std::atomic<uintptr_t> g_pendingSound{0};
std::atomic<uint64_t> g_pendingAt{0};
constexpr int VolumeLogs = 3; // the first scalings after a change are logged, for the live check
int g_volumeLogs = 0;

void Report(uint64_t now, bool force = false) {
	if (!g_dirty || (!force && now < g_reportAt)) return;
	const auto count = [](Footsteps::Reason reason) { return static_cast<unsigned long long>(g_reasons[static_cast<size_t>(reason)]); };
	AsyncLog::InfoF(g_context, "[footsteps] summary mode=%u last_reason=%s position_raw=%016llX travel=%.6f net=%.6f,%.6f max_stationary_ms=%llu baseline=%llu moved=%llu settling=%llu stalled=%llu pushing=%llu animation=%llu unavailable=%llu unrelated=%llu unresolved_foreign=%llu allowed=%llu suppressed=%llu",
		g_lastSample.mode, Footsteps::ReasonName(g_lastDecision.reason), static_cast<unsigned long long>(g_lastSample.position),
		g_travel, g_netX, g_netY, static_cast<unsigned long long>(g_maxStationary),
		count(Footsteps::Reason::Baseline), count(Footsteps::Reason::Moved), count(Footsteps::Reason::Settling), count(Footsteps::Reason::Stalled),
		count(Footsteps::Reason::Pushing),
		count(Footsteps::Reason::Animation), count(Footsteps::Reason::Unavailable), count(Footsteps::Reason::Unrelated),
		static_cast<unsigned long long>(g_unresolvedForeign), static_cast<unsigned long long>(g_allowed), static_cast<unsigned long long>(g_suppressed));
	g_reasons = {}; g_maxStationary = g_unresolvedForeign = 0;
	g_travel = g_netX = g_netY = 0; g_dirty = false;
	g_reportAt = now + ReportIntervalMs;
}

void Record(const Footsteps::Sample& sample, const Footsteps::Decision& decision, uint64_t now) {
	++g_reasons[static_cast<size_t>(decision.reason)];
	if (decision.reason == Footsteps::Reason::Unrelated) {
		if (!sample.valid) ++g_unresolvedForeign;
		return;
	}
	if (!g_reportAt) g_reportAt = now + ReportIntervalMs;
	g_dirty = true; g_lastSample = sample; g_lastDecision = decision;
	g_maxStationary = std::max(g_maxStationary, decision.stationaryMs);
	const double dx = decision.dx / PositionScale, dy = decision.dy / PositionScale;
	g_travel += std::hypot(dx, dy); g_netX += dx; g_netY += dy;
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") g_command = Command::On;
	else if (arg == "off") g_command = Command::Off;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
auto Toggle(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)
		|| event->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	g_command = Command::Toggle;
	return D2RL::Input::ActionResult::Handled;
}
}
void Register(const D2RL::PluginContext* context) {
	g_context = context;
	g_supported = NativeFootsteps::Initialize(context);
	g_enabled = g_supported;
	(void)context->RegisterConsoleCommand("access-footsteps", Console, "Stalled local footsteps: on|off|status.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "footsteps-toggle", .displayName = "Toggle stalled footstep filter", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::F, D2RL::Input::Modifier::Control}, .callback = Toggle};
		(void)g_input->registerAction(context, &action, &g_action);
	}
}
void ScalePending() {
	const auto sound = g_pendingSound.exchange(0);
	const int percent = Footsteps::VolumePercents[static_cast<size_t>(g_volumeLevel.load())];
	if (!sound || percent == Footsteps::NeutralVolumePercent || GetTickCount64() - g_pendingAt.load() > PendingSoundMs) return;
	const int stock = NativeFootsteps::Volume(sound);
	if (stock <= 0) return;
	const int scaled = Footsteps::ScaleVolume(stock, percent);
	NativeFootsteps::SetVolume(sound, scaled);
	if (g_volumeLogs < VolumeLogs) {
		++g_volumeLogs;
		AsyncLog::InfoF(g_context, "[footsteps] volume stock=%d percent=%d set=%d read_back=%d", stock, percent, scaled, NativeFootsteps::Volume(sound));
	}
}
void Tick() {
	ScalePending();
	const auto command = g_command.exchange(Command::None);
	std::lock_guard lock(g_mutex);
	Report(GetTickCount64(), command != Command::None);
	if (command == Command::None) return;
	if (command != Command::Status) {
		g_enabled = g_supported && (command == Command::On || (command == Command::Toggle && !g_enabled));
		g_filter.Reset(); g_seen = g_muted = false;
	}
	const auto& text = Strings::Current();
	ScreenReader::Speak(!g_supported ? text.footstepsUnavailable : g_enabled ? text.footstepsEnabled : text.footstepsDisabled, true);
	AsyncLog::InfoF(g_context, "[footsteps] enabled=%u supported=%u allowed=%llu suppressed=%llu",
		g_enabled ? 1u : 0u, g_supported ? 1u : 0u, static_cast<unsigned long long>(g_allowed), static_cast<unsigned long long>(g_suppressed));
}
bool Suppress(uint32_t entity) noexcept {
	if (!g_enabled) return false;
	try {
		auto sample = NativeFootsteps::Read(entity);
		std::lock_guard lock(g_mutex);
		if (!g_enabled) return false;
		if (sample.valid && sample.local) {
			const uint64_t refused = WallSlideService::LastBlocked().attempts;
			sample.pushing = refused != g_refusedSeen;
			g_refusedSeen = refused;
		}
		const uint64_t now = GetTickCount64();
		const bool muted = g_filter.Suppress(sample, now);
		const auto& decision = g_filter.LastDecision();
		Record(sample, decision, now);
		if (sample.valid && sample.local) {
			if (muted) ++g_suppressed; else ++g_allowed;
			if (!g_seen || muted != g_muted) {
				AsyncLog::InfoF(g_context, "[footsteps] state=%s entity=%u position_raw=%016llX locomotion=%u allowed=%llu suppressed=%llu mode=%u reason=%s stationary_ms=%llu delta=%.6f,%.6f",
					muted ? "stalled" : "audible", entity, static_cast<unsigned long long>(sample.position), sample.locomotion ? 1u : 0u,
					static_cast<unsigned long long>(g_allowed), static_cast<unsigned long long>(g_suppressed), sample.mode, Footsteps::ReasonName(decision.reason),
					static_cast<unsigned long long>(decision.stationaryMs), decision.dx / PositionScale, decision.dy / PositionScale);
				g_seen = true; g_muted = muted;
			}
		}
		return muted;
	} catch (...) { return false; } // uncertainty must preserve stock audio
}
void Unregister() {
	g_enabled = false;
	std::lock_guard lock(g_mutex);
	Report(GetTickCount64(), true);
	if (g_input && g_action != D2RL::Input::InvalidHandle) (void)g_input->unregisterAction(g_context, g_action);
	g_filter.Reset(); g_context = nullptr; g_input = nullptr; g_action = D2RL::Input::InvalidHandle;
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) { g_command = enabled ? Command::On : Command::Off; }
Footsteps::Extras Played(uint32_t entity, uintptr_t sound) noexcept {
	if (g_volumeLevel.load() == Footsteps::NeutralVolumeLevel || !NativeFootsteps::VolumeSupported()) return {};
	const auto sample = NativeFootsteps::Read(entity);
	if (!sample.valid || !sample.local) return {};
	g_pendingAt = GetTickCount64();
	const int percent = Footsteps::VolumePercents[static_cast<size_t>(g_volumeLevel.load())];
	if (percent > Footsteps::NeutralVolumePercent) return Footsteps::ExtraVoices(percent);
	g_pendingSound = sound;
	return {};
}
int VolumeLevel() { return g_volumeLevel.load(); }
void SetVolumeLevel(int level) {
	g_volumeLevel = std::clamp(level, 0, static_cast<int>(Footsteps::VolumePercents.size()) - 1);
	g_volumeLogs = 0;
}
}
