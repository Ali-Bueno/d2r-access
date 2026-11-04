#pragma once

#include <string>
#include <string_view>

// Speech for the launcher: PRISM from the plugin's folder, loaded on the first sentence.
namespace D2RAccess::Launcher::Speech {

// Same place as the plugin's own copy (src/screen_reader.cpp). Held open from the first sentence.
inline constexpr std::wstring_view PrismRelativePath = L"d2rloader\\plugins\\d2r-access\\prism.dll";

void Configure(const std::wstring& gameFolder);
// The launcher has no window, so every focus change elsewhere cuts its speech (docs/launcher.md).
void Say(std::string_view text);          // queued behind the earlier sentences; silent when PRISM cannot load
void SayOverFocus(std::string_view text); // after a focus change settles, interrupting what the screen reader says of it
void Finish();                            // waits for the queued sentences to be heard, then releases PRISM

}
