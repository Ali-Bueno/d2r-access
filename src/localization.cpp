#include "localization.h"

#include "speech_strings.h"

namespace D2RAccess::Localization {

namespace {

const D2RL::LocalizationServiceV1* g_service = nullptr;
const D2RL::PluginContext*         g_context = nullptr;

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

void DetectSpeechLanguage(const D2RL::PluginContext* context) {
	static bool detected = false;
	if (detected) {
		return;
	}
	const std::string cancel = Text(Strings::CancelProbeKey);
	if (cancel.empty()) {
		return; // string tables not loaded yet
	}
	detected = true;
	const bool spanish = cancel == Strings::CancelProbeSpanish;
	Strings::Select(spanish ? Strings::Language::Spanish : Strings::Language::English);
	D2RL::LogInfoF(context, "Speech language: %s (probe \"%s\" = \"%s\")", spanish ? "Spanish" : "English", std::string(Strings::CancelProbeKey).c_str(), cancel.c_str());
}

std::string Text(std::string_view key) {
	if (g_service == nullptr || key.empty()) {
		return {};
	}
	const std::string owned(key);

	// Two-call pattern documented in localization.h: size query, then copy.
	uint32_t required = 0;
	if (g_service->getStringByKey(g_context, owned.c_str(), nullptr, 0, &required) != D2RL::Localization::Result::BufferTooSmall || required == 0) {
		return {};
	}
	std::string text(required, '\0');
	if (g_service->getStringByKey(g_context, owned.c_str(), text.data(), required, &required) != D2RL::Localization::Result::Success) {
		return {};
	}
	// required includes the trailing null byte.
	while (!text.empty() && text.back() == '\0') {
		text.pop_back();
	}
	return text;
}

}
