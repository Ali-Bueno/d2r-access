#include "control_hints.h"

#include "async_log.h"
#include "auto_detail.h"
#include "screen_reader.h"

namespace D2RAccess::ControlHints {

namespace {
std::string g_current;
}

void Offer(std::string_view text) {
	if (text != g_current) g_current = text;
}

const std::string& Current() {
	return g_current;
}

bool Automatic() {
	return AutoDetail::Enabled() && AutoDetail::ControlTooltips();
}

void SpeakAfter(const D2RL::PluginContext* context, std::string_view text) {
	if (text.empty() || !Automatic()) return;
	AsyncLog::InfoF(context, "[hints] after -> \"%.*s\"", static_cast<int>(text.size()), text.data());
	ScreenReader::Speak(text, false);
}

void Reset() {
	g_current.clear();
}

}
