#pragma once

#include <string>
#include <string_view>

// Speech for the launcher: PRISM from the plugin's folder, loaded on the first sentence.
namespace D2RAccess::Launcher::Speech {

void Configure(const std::wstring& gameFolder);
// The launcher has no window, so every focus change elsewhere cuts its speech (docs/launcher.md).
void Say(std::string_view text);          // queued behind the earlier sentences; silent when PRISM cannot load
void SayOverFocus(std::string_view text); // after a focus change settles, interrupting what the screen reader says of it
void Finish();                            // waits for the queued sentences to be heard, then releases PRISM

}
