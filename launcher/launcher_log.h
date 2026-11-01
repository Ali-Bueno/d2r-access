#pragma once

#include <string>
#include <string_view>

// Timestamped lines appended to d2rloader/logs/d2r-access-launcher.log.
namespace D2RAccess::Launcher::Log {

void Open(const std::wstring& path);
void Line(std::string_view message);

std::string Narrow(const std::wstring& text); // UTF-8

}
