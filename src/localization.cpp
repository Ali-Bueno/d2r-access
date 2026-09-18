#include "localization.h"

#include "speech_strings.h"

namespace D2RAccess::Localization {

namespace {

const D2RL::LocalizationService* g_service = nullptr;
const D2RL::PluginContext*         g_context = nullptr;

template<class Lookup> std::string CopyString(Lookup lookup) {
	// The SDK's two-call contract includes the terminating null in required.
	uint32_t required = 0;
	if (lookup(nullptr, 0, &required) != D2RL::Localization::Result::BufferTooSmall || !required) return {};
	std::string text(required, '\0');
	if (lookup(text.data(), required, &required) != D2RL::Localization::Result::Success) return {};
	while (!text.empty() && text.back() == '\0') text.pop_back();
	return text;
}

}

bool Initialize(const D2RL::PluginContext* context) {
	if (context == nullptr) {
		return false;
	}
	if (context->QueryService(&g_service) != D2RL::ServiceQueryResult::Success) {
		g_service = nullptr;
		return false;
	}
	if (!D2RL::HasLocalizationServiceField(g_service, D2RL::LocalizationServiceRequiredSize)) {
		g_service = nullptr;
		return false;
	}
	g_context = context;
	return true;
}

void Shutdown() {
	g_service = nullptr;
	g_context = nullptr;
}

bool g_speechLanguageDetected = false;

void ForgetSpeechLanguage() {
	g_speechLanguageDetected = false;
}

void DetectSpeechLanguage(const D2RL::PluginContext* context) {
	if (g_speechLanguageDetected) {
		return;
	}
	const std::string probe = Text(Strings::TextLocaleProbe.key);
	if (probe.empty()) {
		return; // string tables not loaded yet
	}
	g_speechLanguageDetected = true;
	const auto language = Strings::FromProbeValue(probe);
	Strings::Select(language.value_or(Strings::Language::enUS));
	const std::string key(Strings::TextLocaleProbe.key);
	if (language) {
		D2RL::LogInfoF(context, "Speech language: %s (probe \"%s\" = \"%s\")", std::string(Strings::LocaleCode(*language)).c_str(), key.c_str(), probe.c_str());
	} else {
		D2RL::LogWarnF(context, "Speech language: unrecognised probe \"%s\" = \"%s\", falling back to English", key.c_str(), probe.c_str());
	}
}

std::string Text(std::string_view key) {
	if (g_service == nullptr || key.empty()) {
		return {};
	}
	// Plugin ABI 2 gave the string tables namespaces: a bare key now searches
	// the namespace named by PluginInfo.id — this plugin's own, which holds
	// nothing. Every key the mod looks up is one of the game's, so they are
	// asked for by name. A key that already carries a namespace is passed
	// through unchanged (localization.h in the SDK: "Use d2r:Key or
	// d2rloader:Key for game or loader text").
	constexpr std::string_view GameNamespace = "d2r:";
	const std::string owned = key.find(':') == std::string_view::npos
		? std::string(GameNamespace) + std::string(key)
		: std::string(key);

	return CopyString([&](char* output, uint32_t size, uint32_t* required) {
		return g_service->getStringByKey(g_context, owned.c_str(), output, size, required);
	});
}

std::string Text(uint32_t id) {
	if (!g_service) return {};
	return CopyString([&](char* output, uint32_t size, uint32_t* required) {
		return g_service->getStringById(g_context, id, output, size, required);
	});
}

}
