#include "launcher_speech.h"
#include "launcher_log.h"

#include <prism.h>
#include <windows.h>

#include <algorithm>
#include <string>

namespace D2RAccess::Launcher::Speech {

namespace {

// Same place as the plugin's own copy (src/screen_reader.cpp).
constexpr const wchar_t* PrismRelativePath = L"d2rloader\\plugins\\d2r-access\\prism.dll";
constexpr DWORD MsPerCharacter   = 70; // a screen reader reads roughly 14 characters a second
constexpr DWORD MaxSpeechWaitMs  = 15'000;
// A screen reader announces a new foreground window within this; speaking after it wins over it.
constexpr DWORD FocusSettleMs    = 500;
constexpr unsigned char Utf8ContinuationMask = 0xC0, Utf8ContinuationBits = 0x80;

struct Api {
	decltype(&prism_config_init)           configInit          = nullptr;
	decltype(&prism_init)                  init                = nullptr;
	decltype(&prism_shutdown)              shutdown            = nullptr;
	decltype(&prism_registry_acquire_best) registryAcquireBest = nullptr;
	decltype(&prism_backend_initialize)    backendInitialize   = nullptr;
	decltype(&prism_backend_speak)         backendSpeak        = nullptr;
	decltype(&prism_backend_free)          backendFree         = nullptr;
};

std::wstring  g_path;
bool          g_tried   = false;
HMODULE       g_library = nullptr;
Api           g_api;
PrismContext* g_context = nullptr;
PrismBackend* g_backend = nullptr;
ULONGLONG     g_doneAt  = 0; // tick when the queued sentences should have been read

template <typename Fn>
bool Resolve(const char* name, Fn& out) {
	out = reinterpret_cast<Fn>(GetProcAddress(g_library, name));
	return out != nullptr;
}

void Release() {
	if (g_backend) g_api.backendFree(g_backend);
	if (g_context) g_api.shutdown(g_context);
	if (g_library) FreeLibrary(g_library);
	g_backend = nullptr;
	g_context = nullptr;
	g_library = nullptr;
}

bool Acquire() {
	g_library = LoadLibraryW(g_path.c_str());
	if (!g_library) {
		Log::Line("speech: cannot load prism.dll, continuing silently");
		return false;
	}
	if (!(Resolve("prism_config_init", g_api.configInit) && Resolve("prism_init", g_api.init) && Resolve("prism_shutdown", g_api.shutdown)
			&& Resolve("prism_registry_acquire_best", g_api.registryAcquireBest) && Resolve("prism_backend_initialize", g_api.backendInitialize)
			&& Resolve("prism_backend_speak", g_api.backendSpeak) && Resolve("prism_backend_free", g_api.backendFree))) {
		Log::Line("speech: prism.dll lacks an expected export");
		Release();
		return false;
	}
	PrismConfig config = g_api.configInit();
	g_context          = g_api.init(&config);
	g_backend          = g_context ? g_api.registryAcquireBest(g_context) : nullptr;
	if (!g_backend) {
		Log::Line("speech: no screen reader backend available");
		Release();
		return false;
	}
	// acquire_best hands out an already initialized backend (docs: prism-prebuilt.md).
	const PrismError error = g_api.backendInitialize(g_backend);
	if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
		Log::Line("speech: the backend failed to initialize");
		Release();
		return false;
	}
	return true;
}

DWORD SpeakingTimeMs(std::string_view text) {
	DWORD characters = 0;
	for (const char c : text) {
		if ((static_cast<unsigned char>(c) & Utf8ContinuationMask) != Utf8ContinuationBits) ++characters;
	}
	return std::min(characters * MsPerCharacter, MaxSpeechWaitMs);
}

}

void Configure(const std::wstring& gameFolder) {
	g_path = gameFolder + L"\\" + PrismRelativePath;
}

void Speak(std::string_view text, bool interrupt) {
	Log::Line("say: " + std::string(text));
	if (!g_tried) {
		g_tried = true;
		Acquire();
	}
	if (!g_backend) return;
	(void)g_api.backendSpeak(g_backend, std::string(text).c_str(), interrupt);
	const ULONGLONG now = GetTickCount64();
	g_doneAt            = (interrupt ? now : std::max(now, g_doneAt)) + SpeakingTimeMs(text);
}

void Say(std::string_view text) { Speak(text, false); }

void SayOverFocus(std::string_view text) {
	Sleep(FocusSettleMs);
	Speak(text, true);
}

void Finish() {
	const ULONGLONG now = GetTickCount64();
	if (g_backend && g_doneAt > now) Sleep(static_cast<DWORD>(g_doneAt - now));
	Release();
}

}
