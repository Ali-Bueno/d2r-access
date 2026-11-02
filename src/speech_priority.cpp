#include "speech_priority.h"

#include "screen_reader.h"

#include <windows.h>
#include <algorithm>
#include <atomic>

namespace D2RAccess::SpeechPriority {
namespace {
struct Levels {
	std::array<std::atomic<Level>, KindCount> at;
	Levels() { for (size_t i = 0; i < KindCount; ++i) at[i] = Defaults[i]; }
};
Levels g_levels;
}

Level Get(Kind kind) { return g_levels.at[static_cast<size_t>(kind)].load(); }

void Set(Kind kind, Level level) {
	if (level >= Level::Queued && level < Level::Count) g_levels.at[static_cast<size_t>(kind)] = level;
}

void Say(Kind kind, std::string_view text, bool urgent) {
	const auto delivery = For(Get(kind), urgent);
	if (delivery.holdMs) ScreenReader::Urge(text, GetTickCount64() + delivery.holdMs);
	else ScreenReader::Speak(text, delivery.interrupt);
}

void SayWhole(Kind kind, std::string_view text, bool first) {
	const auto until = GetTickCount64() + std::max(HoldMs, SpokenMs(text));
	if (Get(kind) == Level::Queued) ScreenReader::Lead(text, until);
	else ScreenReader::Urge(text, until, first);
}

}
