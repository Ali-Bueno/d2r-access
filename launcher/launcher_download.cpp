#include "launcher_download.h"

#include "launcher_log.h"
#include "launcher_system.h"

#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <format>
#include <fstream>
#include <vector>

namespace D2RAccess::Launcher::Download {

namespace {

constexpr const wchar_t* UserAgent  = L"D2RAccess-launcher";
constexpr DWORD          HostChars  = 256;
constexpr DWORD          PathChars  = 2048;
constexpr DWORD          ChunkBytes = 64 * 1024;
constexpr DWORD          StatusOk   = 200;
constexpr const char*    HexDigits  = "0123456789abcdef";

struct Internet {
	HINTERNET handle;
	~Internet() { if (handle) WinHttpCloseHandle(handle); }
};

}

bool Fetch(std::string_view url, std::uint64_t stepMs, std::uint64_t totalMs, const Sink& sink) {
	const std::wstring wide = System::Widen(url);
	wchar_t            host[HostChars];
	wchar_t            path[PathChars];
	URL_COMPONENTS     parts{};
	parts.dwStructSize     = sizeof(parts);
	parts.lpszHostName     = host;
	parts.dwHostNameLength = HostChars;
	parts.lpszUrlPath      = path;
	parts.dwUrlPathLength  = PathChars;
	if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
		Log::Line(std::format("download: bad URL, error {}", GetLastError()));
		return false;
	}
	const std::uint64_t deadline = System::NowMs() + totalMs;
	const int           step     = static_cast<int>(stepMs);
	Internet session{WinHttpOpen(UserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
	if (!session.handle || !WinHttpSetTimeouts(session.handle, step, step, step, step)) {
		Log::Line(std::format("download: no session, error {}", GetLastError()));
		return false;
	}
	Internet    connection{WinHttpConnect(session.handle, host, parts.nPort, 0)};
	const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
	Internet    request{connection.handle ? WinHttpOpenRequest(connection.handle, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : nullptr};
	if (!request.handle || !WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(request.handle, nullptr)) {
		Log::Line(std::format("download: request failed, error {}", GetLastError()));
		return false;
	}
	DWORD status = 0;
	DWORD size   = sizeof(status);
	if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) ||
		status != StatusOk) {
		Log::Line(std::format("download: HTTP status {}", status));
		return false;
	}
	std::vector<char> buffer(ChunkBytes);
	for (;;) {
		DWORD read = 0;
		if (!WinHttpReadData(request.handle, buffer.data(), ChunkBytes, &read)) {
			Log::Line(std::format("download: read failed, error {}", GetLastError()));
			return false;
		}
		if (read == 0) return true;
		if (!sink(buffer.data(), read)) return false;
		if (System::NowMs() >= deadline) {
			Log::Line("download: timed out");
			return false;
		}
	}
}

std::optional<std::string> Sha256Hex(const std::filesystem::path& file) {
	std::ifstream input(file, std::ios::binary);
	if (!input) return std::nullopt;
	BCRYPT_ALG_HANDLE          algorithm = nullptr;
	BCRYPT_HASH_HANDLE         hash      = nullptr;
	std::optional<std::string> result;
	DWORD                      hashLength = 0, got = 0;
	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return std::nullopt;
	if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &got, 0) >= 0 &&
		BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
		std::vector<char> chunk(ChunkBytes);
		bool              ok = true;
		while (ok && input.read(chunk.data(), static_cast<std::streamsize>(chunk.size())).gcount() > 0) {
			ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(chunk.data()), static_cast<ULONG>(input.gcount()), 0) >= 0;
		}
		std::vector<unsigned char> digest(hashLength);
		if (ok && input.eof() && BCryptFinishHash(hash, digest.data(), hashLength, 0) >= 0) {
			std::string hex;
			for (const unsigned char byte : digest) {
				hex += HexDigits[byte >> 4];
				hex += HexDigits[byte & 0x0F];
			}
			result = hex;
		}
	}
	if (hash) BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	return result;
}

}
