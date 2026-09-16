#include "localization.h"

#include "speech_strings.h"

namespace D2RAccess::Localization {

namespace {

const D2RL::LocalizationServiceV1* g_service = nullptr;
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
	if (context->QueryService(D2RL::ServiceId::Localization, D2RL::LocalizationServiceV1Version, &g_service) != D2RL::ServiceQueryResult::Success) {
		g_service = nullptr;
		return false;
	}
	if (!D2RL::HasLocalizationServiceV1Field(g_service, D2RL::LocalizationServiceV1RequiredSize)) {
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
	const std::string cancel = Text(Strings::CancelProbeKey);
	if (cancel.empty()) {
		return; // string tables not loaded yet
	}
	g_speechLanguageDetected = true;
	const bool spanish = cancel == Strings::CancelProbeSpanish;
	Strings::Select(spanish ? Strings::Language::Spanish : Strings::Language::English);
	D2RL::LogInfoF(context, "Speech language: %s (probe \"%s\" = \"%s\")", spanish ? "Spanish" : "English", std::string(Strings::CancelProbeKey).c_str(), cancel.c_str());
}

std::string Text(std::string_view key) {
	if (g_service == nullptr || key.empty()) {
		return {};
	}
	const std::string owned(key);

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
