#include "trade_service.h"
#include "async_log.h"
#include "localization.h"
#include "native_trade.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "trade.h"
#include <windows.h>
#include <atomic>

namespace D2RAccess::TradeService {
namespace {
constexpr uint64_t SampleMs = 100;
// The game's own prompt and notice lines (local/lng/strings/ui.json).
constexpr const char* AgreeKey = "strTradeAgreeTo";
constexpr const char* AskingKey = "strWaitingForOtherPlayer";
constexpr const char* BusyKey = "strTradeBusy";
std::atomic<bool> g_enabled = true;
bool g_supported = false;
uint64_t g_nextSample = 0;
Trade::Tracker g_tracker;

auto Console(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!command || !command->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view args(command->args ? command->args : "", command->args ? command->argsLength : 0);
	if (args == "on") g_enabled = true;
	else if (args == "off") g_enabled = false;
	else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}

void Name(std::string& line) {
	auto named = Trade::NamedPrompt(line, UiText::Clean(Localization::Text(AgreeKey)),
		UiText::Clean(Localization::Text(AskingKey)), PartnerName());
	if (!named.empty()) line = std::move(named);
}
}

bool Register(const D2RL::PluginContext* context, bool nativeHooks) {
	if (!context) return false;
	g_supported = NativeTrade::Initialize(context);
	if (g_supported && nativeHooks) (void)NativeTrade::InstallCompletionHook(context);
	g_enabled = RadarConfig::ReadEnabled(context, "[trade]").value_or(true);
	(void)context->RegisterConsoleCommand("access-trade", Console, "Trade announcements: on|off.");
	return g_supported;
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_supported) return;
	const uint64_t now = GetTickCount64();
	if (now < g_nextSample) return;
	g_nextSample = now + SampleMs;
	const auto state = NativeTrade::Read();
	const bool completed = NativeTrade::TakeCompleted();
	if (!state) { g_tracker.Reset(); return; }
	const auto events = g_tracker.Update(*state, completed, NativeTrade::CompletionHooked());
	if (events.empty() || !g_enabled) return;
	const std::string partner = NativeTrade::PartnerName();
	for (const auto event : events) {
		const std::string phrase = Trade::Phrase(event, *state, partner);
		AsyncLog::InfoF(context, "[trade] %s", phrase.c_str());
		ScreenReader::Speak(phrase, false);
	}
}

void NamePrompt(UiText::Readout& readout) {
	if (!g_supported || !g_enabled) return;
	Name(readout.body);
	Name(readout.option);
}

std::string FixNotice(std::string notice) {
	if (!g_supported || !g_enabled) return notice;
	return Trade::SpacedNameSuffix(std::move(notice), UiText::Clean(Localization::Text(BusyKey)));
}

std::string PartnerName() {
	const auto state = g_supported ? NativeTrade::Read() : std::nullopt;
	return state && (state->pending || state->active) ? NativeTrade::PartnerName() : std::string{};
}

// For the keyboard's settings list (settings_list.h): the switch as it stands,
// and the same change the console command makes.
bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) { g_enabled = enabled; }
}
