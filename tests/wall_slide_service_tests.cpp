#include "wall_slide_service.h"
#include "native_wall_slide.h"
#include "radar_config.h"
#include "screen_reader.h"
#include "speech_strings.h"
#include "test_support.h"
#include "wall_bump.h"

#include <optional>
#include <string>
#include <vector>

using namespace D2RAccess;
namespace {
D2RL::ConsoleCommandCallback console = nullptr;
std::optional<bool> configured;
bool initializeResult = true, directional = true;
int initializeCalls = 0, directionalCalls = 0;
std::vector<std::string> speech;
bool interrupted = false;

bool RegisterCommand(const D2RL::PluginContext*, const D2RL::ConsoleCommandRegistration* registration) noexcept {
	Check(std::string_view(registration->name) == "access-movement", "movement command uses the documented name");
	console = registration->callback;
	return true;
}
const D2RL::PluginApi api{.apiSize = D2RL::PluginApiSize, .registerConsoleCommand = RegisterCommand};
const D2RL::PluginContext context{.contextSize = sizeof(D2RL::PluginContext), .api = &api};

auto Command(const char* argument) {
	D2RL::ConsoleCommandContext command{};
	command.plugin = &context;
	command.args = argument;
	command.argsLength = static_cast<uint32_t>(std::char_traits<char>::length(argument));
	const auto result = console(nullptr, &command, nullptr);
	WallSlideService::Tick();
	return result;
}
void Reset() {
	WallSlideService::Unregister();
	console = nullptr; configured.reset(); initializeResult = true; directional = true;
	initializeCalls = directionalCalls = 0; speech.clear(); interrupted = false;
}
}
namespace D2RAccess::NativeWallSlide {
bool Initialize(const D2RL::PluginContext*) { ++initializeCalls; return initializeResult; }
bool IsLocalDirectionalAttempt(uintptr_t) noexcept { ++directionalCalls; return directional; }
}
namespace D2RAccess::RadarConfig {
std::optional<bool> ReadEnabled(const D2RL::PluginContext*, std::string_view section) {
	Check(section == "[movement]", "movement reads its own configuration section");
	return configured;
}
}
namespace D2RAccess::ScreenReader {
void Speak(std::string_view text, bool interrupt) { speech.emplace_back(text); interrupted = interrupt; }
}
namespace D2RAccess::AsyncLog { void InfoF(const D2RL::PluginContext*, const char*, ...) noexcept {} }

int main() {
	Strings::Select(Strings::Language::enUS);
	WallSlideService::Register(&context, true);
	Check(console && initializeCalls == 1, "supported native movement registers its command and initializes once");
	Check(WallSlideService::Prevent(0x1234) && directionalCalls == 1, "feature defaults on and checks native attempt identity");
	Check(WallSlideService::Enabled(), "the radar is told that blocked moves stop instead of sliding");
	WallSlideService::Tick();
	Check(speech.empty(), "blocked movement never speaks automatically");
	Check(Command("status") == D2RL::ConsoleCommandResult::Handled && speech.back() == Strings::Current().movementEnabled && interrupted,
		"status speaks the enabled state as a context update");
	Check(Command("off") == D2RL::ConsoleCommandResult::Handled && speech.back() == Strings::Current().movementDisabled,
		"off disables prevention and announces the state");
	directionalCalls = 0;
	const auto before = WallSlideService::LastBlocked().attempts;
	const float push[2]{0.6f, 0.8f};
	Check(!WallSlideService::Prevent(0x1234, push) && directionalCalls == 1, "disabled prevention passes through");
	const auto blocked = WallSlideService::LastBlocked();
	Check(blocked.attempts == before + 1 && blocked.x == 0.6f && blocked.y == 0.8f,
		"a refused attempt is counted with its push even when sliding is allowed (the wall bump)");
	directional = false;
	Check(!WallSlideService::Prevent(0x1234) && WallSlideService::LastBlocked().attempts == before + 1,
		"another unit's attempt is never counted");
	directional = true;
	Check(!WallSlideService::Enabled(), "with prevention off the radar is told the game slides again");
	Check(Command("on") == D2RL::ConsoleCommandResult::Handled && WallSlideService::Prevent(0x1234), "on restores supported prevention");
	Check(Command("bad") == D2RL::ConsoleCommandResult::InvalidArguments, "invalid movement command is rejected");
	WallSlideService::Unregister(); directionalCalls = 0;
	Check(!WallSlideService::Prevent(0x1234) && directionalCalls == 0, "shutdown restores pass-through behavior");
	Check(WallSlideService::LastBlocked().attempts == 0, "and forgets the attempts counted");

	Reset(); configured = false;
	WallSlideService::Register(&context, true);
	Check(!WallSlideService::Prevent(1), "explicit configuration can start prevention off");
	Check(Command("on") == D2RL::ConsoleCommandResult::Handled && WallSlideService::Prevent(1), "runtime command overrides an off default");

	Reset(); initializeResult = false;
	WallSlideService::Register(&context, true);
	Check(!WallSlideService::Prevent(1), "failed native initialization cannot enable prevention");
	Command("on");
	Check(!WallSlideService::Prevent(1) && speech.back() == Strings::Current().movementUnavailable,
		"unsupported status remains unavailable after an enable request");

	Reset(); configured = true;
	WallSlideService::Register(&context, false);
	Check(initializeCalls == 0 && !WallSlideService::Prevent(1) && !WallSlideService::Enabled(),
		"disabled native hooks cannot be enabled by configuration");
	Command("status");
	Check(speech.back() == Strings::Current().movementUnavailable, "status reports unavailable without native hooks");
	Reset();
	WallSlideService::Register(&context, true);
	directional = false;
	Check(!WallSlideService::Prevent(1) && directionalCalls == 1, "enabled prevention passes through unrelated native attempts");
	directional = true;

	// On a stairway along +x+y (way 1), the game's 45-degree retries still move the player
	// (a tester, 2026-10-23: stuck on the Arcane Sanctuary's stairs).
	const float diagonal[2]{0.6f, 0.8f};
	Check(WallBump::Way({0.6, 0.8}) == 1 && WallBump::Way({-1, 0}) == 4 && WallBump::Way({0.1, -1}) == 6,
		"a push heads one of eight ways, 45 degrees apart");
	Check(WallSlideService::Prevent(1, diagonal, true), "out in the open a retry is still refused");
	WallSlideService::SetSqueezes(1u << 1 | 1u << 5);
	Check(!WallSlideService::Prevent(1, diagonal, true), "in a passage along the push the game's retry slides");
	Check(WallSlideService::Prevent(1, diagonal, false), "while its eight-way fallback is still refused");
	Check(WallSlideService::Prevent(1, nullptr, true), "a retry with no known push is refused");
	const float across[2]{0.8f, -0.6f};
	Check(WallSlideService::Prevent(1, across, true), "a push across the passage, into its wall, stops");
	WallSlideService::SetNarrowSlide(false);
	Check(WallSlideService::Prevent(1, diagonal, true), "with the switch off stairs stop too");
	WallSlideService::Unregister();
	Check(WallSlideService::NarrowSlide(), "the switch starts on");
	Passed();
}
