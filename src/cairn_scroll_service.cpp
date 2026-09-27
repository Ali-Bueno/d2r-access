#include "cairn_scroll_service.h"

#include "async_log.h"
#include "native_cairn.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "screen_reader.h"
#include "speech_strings.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace D2RAccess::CairnScrollService {
namespace {
constexpr const char* PanelName = "ScrollOfInifuss";
Reader g_reader;

bool ScrollListed() {
	const auto& panels = PanelState::OpenPanels();
	return std::find(panels.begin(), panels.end(), PanelName) != panels.end();
}

bool ScrollVisible() {
	const uintptr_t panel = NativeWidgets::FindPanel(PanelName);
	return panel && NativeUi::Visible(panel);
}

std::string Description(const CairnStones::Order& order) {
	const auto& strings = Strings::Current();
	std::string sequence;
	for (size_t rank = 0; rank < order.size(); ++rank) {
		if (!sequence.empty()) sequence += strings.listSeparator;
		sequence += strings.cairnSymbols[order[rank]];
	}
	const int required = std::snprintf(nullptr, 0, strings.cairnScrollOrder, sequence.c_str());
	if (required <= 0) return {};
	std::string text(static_cast<size_t>(required) + 1, '\0');
	std::snprintf(text.data(), text.size(), strings.cairnScrollOrder, sequence.c_str());
	text.resize(static_cast<size_t>(required));
	return text;
}
}

void Tick(const D2RL::PluginContext* context, uint64_t session, bool ready, bool foreground) {
	if (!ready || !NativeCairn::Available()) {
		g_reader.Update(session, false, {});
		return;
	}
	const bool open = ScrollListed();
	const auto order = open && foreground && ScrollVisible() ? NativeCairn::Read(session) : std::optional<CairnStones::Order>{};
	const auto announce = g_reader.Update(session, open, order);
	if (!announce) return;
	const std::string text = Description(*announce);
	if (text.empty()) return;
	AsyncLog::InfoF(context, "[cairn] Deciphered scroll order read for session %llu.",
		static_cast<unsigned long long>(session));
	ScreenReader::Speak(text, false);
}

void Reset() { g_reader = {}; }
}
