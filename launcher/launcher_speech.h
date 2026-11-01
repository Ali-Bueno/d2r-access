#pragma once

#include <string>
#include <string_view>

// Speech for the launcher: PRISM from the plugin's folder, loaded on the first sentence.
namespace D2RAccess::Launcher::Speech {

void Configure(const std::wstring& gameFolder);
void Say(std::string_view text); // silent when PRISM cannot load
void Finish();                   // waits for the last sentence to be heard, then releases PRISM

}
