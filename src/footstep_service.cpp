#include "footstep_service.h"
#include "native_footsteps.h"
#include "async_log.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <windows.h>
#include <atomic>
#include <mutex>
#include <string_view>

namespace D2RAccess::FootstepService {
namespace {
enum class Command { None, On, Off, Toggle, Status };
std::atomic<Command> g_command{Command::None};
std::atomic<bool> g_enabled{false};
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputServiceV1* g_input = nullptr;
D2RL::Input::ActionHandle g_action = D2RL::Input::InvalidHandle;
std::mutex g_mutex;
Footsteps::Filter g_filter;
bool g_supported = false, g_muted = false, g_seen = false;
uint64_t g_allowed = 0, g_suppressed = 0;

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
	if (context->QueryService(D2RL::ServiceId::Input, D2RL::InputServiceV1Version, &g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceV1Field(g_input, D2RL::InputServiceV1RequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "footsteps-toggle", .displayName = "Toggle stalled footstep filter", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::F, D2RL::Input::Modifier::Control}, .callback = Toggle};
		(void)g_input->registerAction(context, &action, &g_action);
	}
}
void Tick() {
	const auto command = g_command.exchange(Command::None);
	if (command == Command::None) return;
	std::lock_guard lock(g_mutex);
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
		const auto sample = NativeFootsteps::Read(entity);
		std::lock_guard lock(g_mutex);
		if (!g_enabled) return false;
		const bool muted = g_filter.Suppress(sample, GetTickCount64());
		if (sample.valid && sample.local) {
			if (muted) ++g_suppressed; else ++g_allowed;
			if (!g_seen || muted != g_muted) {
				AsyncLog::InfoF(g_context, "[footsteps] state=%s entity=%u position_raw=%016llX locomotion=%u allowed=%llu suppressed=%llu",
					muted ? "stalled" : "audible", entity, static_cast<unsigned long long>(sample.position), sample.locomotion ? 1u : 0u,
					static_cast<unsigned long long>(g_allowed), static_cast<unsigned long long>(g_suppressed));
				g_seen = true; g_muted = muted;
			}
		}
		return muted;
	} catch (...) { return false; } // uncertainty must preserve stock audio
}
void Unregister() {
	g_enabled = false;
	std::lock_guard lock(g_mutex);
	if (g_input && g_action != D2RL::Input::InvalidHandle) (void)g_input->unregisterAction(g_context, g_action);
	g_filter.Reset(); g_context = nullptr; g_input = nullptr; g_action = D2RL::Input::InvalidHandle;
}
}
