#include "skill_bar_service.h"
#include "async_log.h"
#include "controller_text.h"
#include "input_direction.h"
#include "localization.h"
#include "native_skill_bar.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "radar_ui.h"
#include "screen_reader.h"
#include "skill_bar.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <charconv>
#include <string>

namespace D2RAccess::SkillBarService {
namespace {
constexpr const char* HudPanel = "HUDPanel";
std::atomic<bool> g_enabled = true;
bool g_supported = false;
SkillBar::PeekGate g_gate;

// The count drawn on the HUD button; 0x239380 shows QuantityText only with one.
int HudQuantity(int slot) {
	const std::string path[] = {"SkillButton" + std::to_string(slot + 1), "QuantityText"};
	const auto widget = NativeWidgets::Find(HudPanel, path);
	if (!widget || !NativeUi::Visible(widget)) return 0;
	const auto text = NativeWidgets::ReadText(widget);
	int quantity = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), quantity);
	return error == std::errc{} && end == text.data() + text.size() ? quantity : 0;
}

void Read(const D2RL::PluginContext* context) {
	const auto bar = NativeSkillBar::AlternateBar();
	if (!bar) { AsyncLog::Info(context, "[skill-bar] LT bar unreadable."); return; }
	// Counts are only the LT bar's while the HUD itself shows that bar.
	const bool counts = NativeSkillBar::BackBarShown();
	std::array<SkillBar::Slot, NativeSkillBar::Buttons> slots;
	for (int i = 0; i < NativeSkillBar::Buttons; ++i) {
		// ControllerHelpSkill1..6 name SkillButton1..6's control (controllersettings.json variant 0).
		slots[i].button = ControllerText::FirstButton(Localization::Text("ControllerHelpSkill" + std::to_string(i + 1)));
		slots[i].bound = (*bar)[i].bound;
		slots[i].skill = (*bar)[i].name;
		slots[i].quantity = counts && slots[i].bound ? HudQuantity(i) : 0;
	}
	AsyncLog::InfoF(context, "[skill-bar] LT bar skills=%d,%d,%d,%d,%d,%d hud=%d.", (*bar)[0].skill, (*bar)[1].skill,
		(*bar)[2].skill, (*bar)[3].skill, (*bar)[4].skill, (*bar)[5].skill, counts ? 1 : 0);
	if (const auto text = SkillBar::Compose(slots); !text.empty()) ScreenReader::Speak(text, true);
}

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") g_enabled = true;
	else if (args == "off") g_enabled = false;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	if (!context) return false;
	g_supported = NativeSkillBar::Initialize(context);
	(void)context->RegisterConsoleCommand("access-skill-bar", Console, "Hold LT alone to hear the LT skill bar: on|off.");
	return g_supported;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_supported || !g_enabled) return;
	const auto trigger = InputDirection::LeftTrigger();
	// InPlay walks the open panels, so it is only asked while LT is down.
	const bool playing = trigger.held && RadarUi::InPlay();
	switch (g_gate.Update(GetTickCount64(), playing, trigger.held, trigger.otherButton)) {
	case SkillBar::Action::Speak: Read(context); break;
	case SkillBar::Action::Stop: ScreenReader::Stop(); break;
	case SkillBar::Action::None: break;
	}
}
}
