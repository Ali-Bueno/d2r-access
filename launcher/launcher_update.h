#pragma once

#include <string>

// The launcher's self-update at start; flow and constants in docs/launcher.md, "Self-update".
namespace D2RAccess::Launcher::Update {

// Looks for a newer release and installs it if the player agrees; true when the new launcher was started
// and this one must exit. Every other outcome (skipped, declined, failed) returns false.
bool Offer(const std::wstring& gameFolder, const std::wstring& arguments);

// Deletes the D2RAccess.exe.old an earlier update left behind.
void CleanLeftover(const std::wstring& gameFolder);

}
