#include "launcher_install.h"

#include "launcher_log.h"
#include "update_logic.h"

#include <windows.h>

#include <format>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace D2RAccess::Launcher::Install {

namespace {

constexpr const wchar_t* OldSuffix = L".old";

struct Journal {
	std::vector<fs::path>                      created;  // targets that did not exist
	std::vector<std::pair<fs::path, fs::path>> replaced; // target, backup
	fs::path                                   movedFrom, movedTo; // the running exe and where it went
};

bool Copy(const fs::path& from, const fs::path& to) {
	std::error_code ec;
	fs::create_directories(to.parent_path(), ec);
	if (!ec) fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
	if (ec) Log::Line(std::format("install: cannot copy to {}: {}", Log::Narrow(to.wstring()), ec.message()));
	return !ec;
}

void Rollback(const Journal& journal) {
	std::error_code ec;
	for (const auto& target : journal.created) fs::remove(target, ec);
	for (const auto& [target, backup] : journal.replaced) Copy(backup, target);
	if (!journal.movedFrom.empty()) {
		fs::remove(journal.movedFrom, ec);
		fs::rename(journal.movedTo, journal.movedFrom, ec);
		if (ec) Log::Line(std::format("install: cannot move the launcher back: {}", ec.message()));
	}
	Log::Line("install: rolled back");
}

}

bool Replace(const fs::path& extracted, const fs::path& gameFolder, const fs::path& backup, std::wstring_view runningExe) {
	Journal         journal;
	std::error_code ec;
	for (const auto& entry : fs::recursive_directory_iterator(extracted, ec)) {
		if (!entry.is_regular_file()) continue;
		const fs::path relative = fs::relative(entry.path(), extracted);
		const fs::path target   = gameFolder / relative;
		const bool     exists   = fs::exists(target);
		if (exists && Update::IsProtectedPath(relative.wstring())) continue;
		bool ok = true;
		if (_wcsicmp(relative.c_str(), std::wstring(runningExe).c_str()) == 0) {
			// Windows lets a running exe be renamed, not overwritten.
			const fs::path old = fs::path(target).concat(OldSuffix);
			std::error_code rename;
			fs::remove(old, rename);
			if (exists) fs::rename(target, old, rename);
			ok = !rename;
			if (ok && exists) { journal.movedFrom = target; journal.movedTo = old; }
			if (!ok) Log::Line(std::format("install: cannot rename the launcher: {}", rename.message()));
		} else if (exists) {
			const fs::path saved = backup / relative;
			ok = Copy(target, saved);
			if (ok) journal.replaced.emplace_back(target, saved);
		} else {
			journal.created.push_back(target);
		}
		if (!ok || !Copy(entry.path(), target)) {
			Rollback(journal);
			return false;
		}
	}
	if (ec) {
		Log::Line(std::format("install: cannot list the release: {}", ec.message()));
		Rollback(journal);
		return false;
	}
	Log::Line(std::format("install: {} replaced, {} created", journal.replaced.size() + (journal.movedFrom.empty() ? 0 : 1), journal.created.size()));
	return true;
}

void DeleteLeftover(const fs::path& exe) {
	const fs::path old = fs::path(exe).concat(OldSuffix);
	for (int attempt = 0; attempt < Update::LeftoverTries && fs::exists(old); ++attempt) {
		std::error_code ec;
		if (fs::remove(old, ec)) {
			Log::Line("deleted the previous launcher");
			return;
		}
		Sleep(static_cast<DWORD>(Update::LeftoverWaitMs));
	}
}

}
