#include "launcher_log.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>

namespace D2RAccess::Launcher::Log {

namespace {
std::wstring g_path;
}

std::string Narrow(const std::wstring& text) {
	if (text.empty()) return {};
	const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	std::string result(static_cast<std::size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
	return result;
}

void Open(const std::wstring& path) {
	g_path = path;
	std::error_code ignored;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ignored);
}

void Line(std::string_view message) {
	if (g_path.empty()) return;
	FILE* file = nullptr;
	if (_wfopen_s(&file, g_path.c_str(), L"ab") != 0 || file == nullptr) return;
	SYSTEMTIME t;
	GetLocalTime(&t);
	std::fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u.%03u %.*s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
		t.wMilliseconds, static_cast<int>(message.size()), message.data());
	std::fclose(file);
}

}
