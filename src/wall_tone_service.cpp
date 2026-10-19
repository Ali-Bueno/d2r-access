#include "wall_tone_service.h"
#include "wall_tone_audio.h"
#include "wall_tones.h"
#include "async_log.h"
#include "mod_sounds.h"
#include "radar_config.h"

#include <atomic>
#include <string_view>

namespace D2RAccess::WallTones {
namespace {
const D2RL::PluginContext* g_context = nullptr;
std::atomic<bool> g_enabled{false};
double g_range = DefaultRange;

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (arg == "on") SetEnabled(true);
	else if (arg == "off") SetEnabled(false);
	else if (arg != "status") return D2RL::ConsoleCommandResult::InvalidArguments;
	AsyncLog::InfoF(g_context, "[wall-tones] enabled=%u range=%.1f", Enabled() ? 1u : 0u, g_range);
	return D2RL::ConsoleCommandResult::Handled;
}
}

bool Register(const D2RL::PluginContext* context) {
	g_context = context;
	const auto config = RadarConfig::ReadWallTones(context);
	g_enabled = config.enabled.value_or(false);
	g_range = config.range.value_or(DefaultRange);
	WallToneAudio::SetLevel(static_cast<float>(config.volume.value_or(RadarConfig::MaxVolume)) / RadarConfig::MaxVolume);
	if (!WallToneAudio::Start(context)) { g_context = nullptr; return false; }
	(void)context->RegisterConsoleCommand("access-wall-tones", Console, "Wall tones: on|off|status.");
	AsyncLog::InfoF(context, "[wall-tones] registered enabled=%u range=%.1f", g_enabled.load() ? 1u : 0u, g_range);
	return true;
}

void Update(const Radar::World* world) {
	if (!g_context) return;
	if (!world || !Enabled() || !ModSounds::Enabled()) { WallToneAudio::Silence(); return; }
	WallToneAudio::Gains gains{};
	const auto clearances = Clearances(*world, g_range);
	for (size_t i = 0; i < gains.size(); ++i) gains[i] = Gain(clearances[i], g_range);
	WallToneAudio::Update(gains);
}

void Unregister() {
	WallToneAudio::Stop();
	g_context = nullptr;
}

bool Enabled() { return g_enabled.load(); }
void SetEnabled(bool enabled) {
	g_enabled = enabled;
	if (!enabled) WallToneAudio::Silence();
}
}
