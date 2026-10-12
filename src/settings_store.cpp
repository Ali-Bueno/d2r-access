#include "settings_store.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace D2RAccess::SettingsStore {
std::string Encode(const std::map<std::string, int>& values) {
	std::string text;
	for (const auto& [name, value] : values) text += name + "=" + std::to_string(value) + "\n";
	return text;
}

std::map<std::string, int> Decode(std::string_view text) {
	std::map<std::string, int> values;
	while (!text.empty()) {
		const size_t end = text.find('\n');
		std::string_view line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
		const size_t equals = line.find('=');
		if (equals == 0 || equals == std::string_view::npos) continue;
		const auto digits = line.substr(equals + 1);
		if (digits.empty() || digits.size() > MaxValueDigits) continue;
		int value = 0;
		bool number = true;
		for (const char c : digits) {
			if (c < '0' || c > '9') { number = false; break; }
			value = value * 10 + (c - '0');
		}
		if (number) values[std::string(line.substr(0, equals))] = value;
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
