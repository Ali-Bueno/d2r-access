#include "launcher_install.h"
#include "test_support.h"

#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace D2RAccess::Launcher;

namespace {

constexpr std::wstring_view Exe = L"D2RAccess.exe";
constexpr std::wstring_view Prism = L"d2rloader\\plugins\\d2r-access\\prism\\prism.dll";
constexpr std::array<std::wstring_view, 2> InUse{Exe, Prism};

void Write(const fs::path& file, std::string_view text) {
	fs::create_directories(file.parent_path());
	std::ofstream(file, std::ios::binary) << text;
}

std::string Read(const fs::path& file) {
	std::ostringstream text;
	text << std::ifstream(file, std::ios::binary).rdbuf();
	return text.str();
}

// A real library at the plugin's path, loaded as the launcher loads prism.dll to speak.
HMODULE LoadAt(const fs::path& target) {
	std::wstring system(MAX_PATH, L'\0');
	system.resize(GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size())));
	fs::create_directories(target.parent_path());
	fs::copy_file(fs::path(system) / L"version.dll", target, fs::copy_options::overwrite_existing);
	return LoadLibraryW(target.c_str());
}

void ReplacesFilesInUse(const fs::path& root) {
	const fs::path game = root / L"game", release = root / L"release", backup = root / L"backup";
	const fs::path prism = game / Prism;
	Write(game / Exe, "old launcher");
	Write(game / L"D2RCore.dll", "old core");
	Write(game / L"d2rloader/plugins/d2r-access/settings.txt", "the player's");
	const HMODULE held = LoadAt(prism);
	Check(held != nullptr, "the library loads");
	const std::string before = Read(prism);

	std::error_code ec;
	fs::copy_file(game / Exe, prism, fs::copy_options::overwrite_existing, ec);
	Check(static_cast<bool>(ec), "a loaded library cannot be overwritten, as every launcher from 1.0.1 to 1.0.4 tried");

	Write(release / Exe, "new launcher");
	Write(release / L"D2RCore.dll", "new core");
	Write(release / Prism, "new prism");
	Write(release / L"d2rloader/plugins/d2r-access/settings.txt", "the release's");
	Write(release / L"zzz-blocked.dll", "cannot land");
	fs::create_directories(game / L"zzz-blocked.dll");
	Check(!Install::Replace(release, game, backup, InUse), "a file that cannot land fails the install");
	Check(Read(game / Exe) == "old launcher" && Read(prism) == before && Read(game / L"D2RCore.dll") == "old core",
		"a failed install leaves every file as it was");
	Check(!fs::exists(fs::path(prism).concat(L".old")), "and nothing moved aside");

	fs::remove(release / L"zzz-blocked.dll");
	fs::remove_all(backup);
	Check(Install::Replace(release, game, backup, InUse), "the install goes through while the library is loaded");
	Check(Read(prism) == "new prism" && Read(game / Exe) == "new launcher" && Read(game / L"D2RCore.dll") == "new core",
		"every file is the release's");
	Check(Read(game / L"d2rloader/plugins/d2r-access/settings.txt") == "the player's", "the player's files are kept");
	Check(Read(fs::path(prism).concat(L".old")) == before, "the library in use waits as .old");

	FreeLibrary(held);
	Install::DeleteLeftover(prism);
	Install::DeleteLeftover(game / Exe);
	Check(!fs::exists(fs::path(prism).concat(L".old")) && !fs::exists(fs::path(game / Exe).concat(L".old")),
		"the next start removes what was moved aside");
}

}

int main() {
	const fs::path root = fs::temp_directory_path() / (L"d2r-install-tests-" + std::to_wstring(GetCurrentProcessId()));
	fs::remove_all(root);
	ReplacesFilesInUse(root);
	fs::remove_all(root);
	Passed();
	return 0;
}
