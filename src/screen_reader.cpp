#include "screen_reader.h"

#include <prism.h>
#include <windows.h>

#include <cstdio>
#include <string>

namespace D2RAccess::ScreenReader {

namespace {

// prism.dll lives in a subfolder of the plugins directory: D2RLoader treats
// every DLL directly inside d2rloader/plugins as a plugin and rejects it.
constexpr const wchar_t* PrismRelativePath = L"d2r-access\\prism.dll";

// Function-pointer types taken from prism.h so a signature change in a new
// PRISM release is a compile error here instead of a silent ABI mismatch.
struct PrismApi {
	decltype(&prism_config_init)           configInit          = nullptr;
	decltype(&prism_init)                  init                = nullptr;
	decltype(&prism_shutdown)              shutdown            = nullptr;
	decltype(&prism_registry_acquire_best) registryAcquireBest = nullptr;
	decltype(&prism_backend_initialize)    backendInitialize   = nullptr;
	decltype(&prism_backend_name)          backendName         = nullptr;
	decltype(&prism_backend_speak)         backendSpeak        = nullptr;
	decltype(&prism_backend_stop)          backendStop         = nullptr;
	decltype(&prism_backend_free)          backendFree         = nullptr;
	decltype(&prism_error_string)          errorString         = nullptr;
};

struct State {
	HMODULE       library = nullptr;
	PrismApi      api {};
	PrismContext* context = nullptr;
	PrismBackend* backend = nullptr;
	std::string   lastError;
};

State g_state {};

template <typename Fn>
bool Resolve(HMODULE library, const char* name, Fn& out) {
	out = reinterpret_cast<Fn>(GetProcAddress(library, name));
	if (out == nullptr) {
		g_state.lastError = std::string("export not found: ") + name;
	}
	return out != nullptr;
}

bool ResolveApi(HMODULE library, PrismApi& api) {
	return Resolve(library, "prism_config_init", api.configInit)
		&& Resolve(library, "prism_init", api.init)
		&& Resolve(library, "prism_shutdown", api.shutdown)
		&& Resolve(library, "prism_registry_acquire_best", api.registryAcquireBest)
		&& Resolve(library, "prism_backend_initialize", api.backendInitialize)
		&& Resolve(library, "prism_backend_name", api.backendName)
		&& Resolve(library, "prism_backend_speak", api.backendSpeak)
		&& Resolve(library, "prism_backend_stop", api.backendStop)
		&& Resolve(library, "prism_backend_free", api.backendFree)
		&& Resolve(library, "prism_error_string", api.errorString);
}

std::string Narrow(const std::wstring& text) {
	if (text.empty()) {
		return {};
	}
	const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string out(size > 0 ? static_cast<size_t>(size - 1) : 0, '\0');
	if (size > 0) {
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), size, nullptr, nullptr);
	}
	return out;
}

std::wstring BuildPrismPath(const wchar_t* pluginDirectory) {
	std::wstring path = pluginDirectory;
	if (!path.empty() && path.back() != L'\\' && path.back() != L'/') {
		path += L'\\';
	}
	path += PrismRelativePath;
	return path;
}

}

bool Initialize(const wchar_t* pluginDirectory) {
	if (g_state.backend != nullptr) {
		return true;
	}
	if (pluginDirectory == nullptr) {
		g_state.lastError = "plugin directory unknown";
		return false;
	}

	// Absolute path: the game's DLL search path must not decide which prism.dll loads.
	const std::wstring path = BuildPrismPath(pluginDirectory);
	g_state.library         = LoadLibraryW(path.c_str());
	if (g_state.library == nullptr) {
		char detail[64] {};
		std::snprintf(detail, sizeof(detail), " (LoadLibrary error %lu)", GetLastError());
		g_state.lastError = "cannot load " + Narrow(path) + detail;
		return false;
	}
	if (!ResolveApi(g_state.library, g_state.api)) {
		Shutdown();
		return false;
	}

	PrismConfig config = g_state.api.configInit();
	g_state.context    = g_state.api.init(&config);
	if (g_state.context == nullptr) {
		g_state.lastError = "prism_init returned null";
		Shutdown();
		return false;
	}

	g_state.backend = g_state.api.registryAcquireBest(g_state.context);
	if (g_state.backend == nullptr) {
		g_state.lastError = "no screen reader backend available (is NVDA/JAWS running?)";
		Shutdown();
		return false;
	}
	// Since PRISM v0.18 acquire_best hands out a shared backend that is already
	// initialized, so ALREADY_INITIALIZED is the normal outcome here, not a failure.
	const PrismError error = g_state.api.backendInitialize(g_state.backend);
	if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
		const char* name = g_state.api.backendName(g_state.backend);
		g_state.lastError = std::string("backend ") + (name != nullptr ? name : "?") + " failed to initialize: " + g_state.api.errorString(error);
		Shutdown();
		return false;
	}
	g_state.lastError.clear();
	return true;
}

void Shutdown() {
	if (g_state.backend != nullptr) {
		g_state.api.backendFree(g_state.backend);
		g_state.backend = nullptr;
	}
	if (g_state.context != nullptr) {
		g_state.api.shutdown(g_state.context);
		g_state.context = nullptr;
	}
	if (g_state.library != nullptr) {
		FreeLibrary(g_state.library);
		g_state.library = nullptr;
	}
	g_state.api = {};
}

bool IsAvailable() {
	return g_state.backend != nullptr;
}

const char* LastError() {
	return g_state.lastError.c_str();
}

const char* BackendName() {
	if (!IsAvailable()) {
		return "";
	}
	const char* name = g_state.api.backendName(g_state.backend);
	return name != nullptr ? name : "";
}

void Speak(std::string_view text, bool interrupt) {
	if (!IsAvailable() || text.empty()) {
		return;
	}
	const std::string owned(text);
	(void)g_state.api.backendSpeak(g_state.backend, owned.c_str(), interrupt);
}

void Stop() {
	if (IsAvailable()) {
		(void)g_state.api.backendStop(g_state.backend);
	}
}

}
