#include "settings_store.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace D2RAccess::SettingsStore {
std::string Encode(const std::map<std::string, bool>& values) {
	std::string text;
	for (const auto& [name, on] : values) text += name + (on ? "=1\n" : "=0\n");
	return text;
}

std::map<std::string, bool> Decode(std::string_view text) {
	std::map<std::string, bool> values;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		const size_t equals = line.find('=');
		if (equals == 0 || equals == std::string_view::npos || equals + 2 != line.size()) continue;
		const char value = line[equals + 1];
		if (value != '0' && value != '1') continue;
		values[std::string(line.substr(0, equals))] = value == '1';
	}
	return values;
}

std::optional<std::string> Load(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in) return std::nullopt;
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

bool Save(const std::filesystem::path& file, const std::string& text) {
	std::error_code error;
	std::filesystem::create_directories(file.parent_path(), error);
	if (error) return false;
	auto temp = file;
	temp += L".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out << text;
		out.flush();
		if (!out) return false;
	}
	return MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
}
