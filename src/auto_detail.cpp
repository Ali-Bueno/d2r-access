#include "auto_detail.h"

#include "async_log.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <D2RLPlugin/api.h>

#include <atomic>

namespace D2RAccess::AutoDetail {

namespace {

const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService*  g_input   = nullptr;
D2RL::Input::ActionHandle  g_action  = D2RL::Input::InvalidHandle;
std::atomic<bool>          g_enabled { false };
std::atomic<bool>          g_toggled { false }; // a press waiting to be announced

auto Action(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void*) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed)
		return D2RL::Input::ActionResult::Ignored;
	g_enabled.store(!g_enabled.load());
	g_toggled.store(true);
	return D2RL::Input::ActionResult::Handled;
}

}

bool Register(const D2RL::PluginContext* context) {
	if (context == nullptr || context->QueryService(&g_input) != D2RL::ServiceQueryResult::Success
		|| !D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		g_input = nullptr;
		return false;
	}
	g_context = context;
	const D2RL::Input::ActionRegistration action{.structSize = D2RL::Input::ActionRegistrationSize, .logicalId = "auto-detail",
		.displayName = "Toggle reading the whole description on highlight", .category = "Accessibility",
		.defaultPrimary = {D2RL::Input::Key::D, D2RL::Input::Modifier::Control}, .callback = Action, .userData = nullptr};
	return g_input->registerAction(context, &action, &g_action) == D2RL::Input::Result::Success;
}

void Unregister() {
	if (g_input != nullptr && g_context != nullptr && g_action != D2RL::Input::InvalidHandle) {
		(void)g_input->unregisterAction(g_context, g_action);
	}
	g_action   = D2RL::Input::InvalidHandle;
	g_input    = nullptr;
	g_context  = nullptr;
}

bool Enabled() {
	return g_enabled.load();
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_toggled.exchange(false)) return;
	const bool enabled = g_enabled.load();
	AsyncLog::InfoF(context, "[detail] auto %s", enabled ? "on" : "off");
	ScreenReader::Speak(enabled ? Strings::Current().autoDetailEnabled : Strings::Current().autoDetailDisabled, true);
}

}
