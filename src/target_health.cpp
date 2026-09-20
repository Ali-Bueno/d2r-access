#include "target_health.h"
#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <D2RLPlugin/context.h>
#include <D2RLPlugin/input.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace D2RAccess::TargetHealth {
namespace {
enum class Command { None, Report, Status };

// HUDMonsterHealthHD.json: a MonsterHealthPanel named HUDMonsterHealth whose
// only health element is a HealthBarWidget named Health, with the target's
// name in a TextBoxWidget inside it. There is no ValueDisplay and no other
// text box carrying a number, which is why the percentage cannot be read the
// way the player's own orbs are.
constexpr const char* Panel = "HUDMonsterHealth";
const std::string HealthPath[] = {"Health"};
const std::string NamePath[]   = {"Health", "Name"};

// HealthBarWidget, MEASURED offline against build 3.3.93787 (image base
// 0x140000000, re/D2R_image.exe):
//
//   0x1402ac890  HealthBarWidget::SetHealthPercent(int)
//       clamps its argument to [0, 100], stores it as an int at this+0x194,
//       then calls 0x14085a190 with percent / 100.0f;
//   0x1402ac800  re-applies the stored percent the same way;
//   0x14085a190  ProgressBarWidget's fill setter: clamps to [0.0, 1.0] and
//       stores the float at this+0x154.
//
// Both callers also pick the bar's colour by comparing +0x194 against +0x15c
// and +0x158, which are the criticalHealthPercent and warningHealthPercent
// fields the layout declares -- in the order the comparison requires. That
// cross-check is what makes +0x194 the health percentage rather than some
// other clamped integer.
//
// READ, not measured live: that the game keeps these current while a target
// is held. Fill is checked against Percent below so a build that moves either
// field is caught instead of speaking a number out of the wrong memory.
constexpr uintptr_t PercentOffset = 0x194;  // int, 0..100
constexpr uintptr_t FillOffset    = 0x154;  // float, 0..1, percent / 100

const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input = nullptr;
std::vector<D2RL::Input::ActionHandle> g_actions;
std::atomic<Command> g_command{Command::None};

// The percentage the bar is drawing, or -1 when it cannot be trusted. The two
// fields are written together from one number, so disagreement means this
// build does not lay the widget out the way the offsets above say. A blind
// player has no way to catch a wrong number, so say nothing rather than guess.
int Percent(uintptr_t bar) {
	int percent = 0;
	float fill = 0;
	if (!NativeMemory::Peek(bar + PercentOffset, percent)
		|| !NativeMemory::Peek(bar + FillOffset, fill)) return -1;
	if (percent < 0 || percent > 100) return -1;
	if (!std::isfinite(fill) || std::fabs(fill * 100.0f - static_cast<float>(percent)) > 1.0f) return -1;
	return percent;
}

void Report(const D2RL::PluginContext* context) {
	const auto& strings = Strings::Current();
	const NativeMemory::ReadScope scope;
	const uintptr_t panel = NativeWidgets::IsAvailable() ? NativeWidgets::FindPanel(Panel) : 0;
	// The panel exists for the whole session and is simply hidden while
	// nothing is targeted, so visibility is what says there is a target.
	const uintptr_t bar = panel && NativeUi::Visible(panel) ? NativeWidgets::Find(Panel, HealthPath) : 0;
	const int percent = bar ? Percent(bar) : -1;
	if (percent < 0) {
		AsyncLog::InfoF(context, "[target] no reading (panel=%p bar=%p)",
			reinterpret_cast<void*>(panel), reinterpret_cast<void*>(bar));
		ScreenReader::Speak(strings.targetNone, true);
		return;
	}
	std::string name = NativeWidgets::TextAt(Panel, NamePath);
	// A name is what the bar is labelled with; without one the percentage is
	// still the answer to the question that was asked.
	if (name.empty()) name = strings.targetNone;
	char phrase[200]{};
	std::snprintf(phrase, sizeof(phrase), strings.targetHealth, name.c_str(), percent);
	AsyncLog::InfoF(context, "[target] \"%s\" %d percent", name.c_str(), percent);
	// Asked for, so it interrupts: the player is waiting for this answer.
	ScreenReader::Speak(phrase, true);
}

auto __cdecl OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept
	-> D2RL::Input::ActionResult {
	if (event == nullptr || event->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	g_command = Command::Report;
	return D2RL::Input::ActionResult::Handled;
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "report") g_command = Command::Report;
	else if (arg == "status") g_command = Command::Status;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_context = context;
	(void)context->RegisterConsoleCommand("access-target", Console,
		"Target health: report|status. Also bound to F.");
	if (context->QueryService(&g_input) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize,
			.logicalId = "target-health", .displayName = "Say the target's health", .category = "Accessibility",
			.defaultPrimary = {D2RL::Input::Key::F, D2RL::Input::Modifier::None}, .callback = OnAction,
			.userData = nullptr};
		D2RL::Input::ActionHandle handle = D2RL::Input::InvalidHandle;
		if (g_input->registerAction(context, &action, &handle) == D2RL::Input::Result::Success) {
			g_actions.push_back(handle);
		}
	}
	AsyncLog::Info(context, "[target] Target health readout registered.");
	return true;
}

void Unregister() {
	if (g_input && g_context)
		for (auto handle : g_actions) (void)g_input->unregisterAction(g_context, handle);
	g_actions.clear();
	g_input = nullptr;
	g_context = nullptr;
	g_command = Command::None;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_context) return;
	const auto command = g_command.exchange(Command::None);
	if (command == Command::None) return;
	if (command == Command::Status) {
		const NativeMemory::ReadScope scope;
		const uintptr_t panel = NativeWidgets::IsAvailable() ? NativeWidgets::FindPanel(Panel) : 0;
		const uintptr_t bar = panel ? NativeWidgets::Find(Panel, HealthPath) : 0;
		AsyncLog::InfoF(context, "[target] status panel=%p visible=%u bar=%p percent=%d",
			reinterpret_cast<void*>(panel), panel ? NativeUi::Visible(panel) : 0,
			reinterpret_cast<void*>(bar), bar ? Percent(bar) : -1);
		return;
	}
	Report(context);
}

}
