#include "npc_speech.h"

#include "control_state.h"
#include "generated/panel_widgets.h"

#include <cstring>
#include <string>

namespace D2RAccess::NpcSpeech {
namespace {
// SoundOptionsPanelHD.json: the NPCSpeech carousel's command and its audio-only state.
constexpr const char* Command = "SetNPCSpeech";
constexpr const char* AudioOnlyState = "OptionsMenuNPCSpeechAudioOnly";
}

bool AudioOnly() {
	for (const auto& panel : Generated::PanelCatalog) {
		for (uint32_t i = 0; i < panel.count; ++i) {
			const auto& widget = panel.widgets[i];
			if (!widget.updateCommand || std::strcmp(widget.updateCommand, Command) != 0 || !widget.states) continue;
			const auto index = ControlState::Index(widget, std::string(panel.panel) + "/" + widget.name);
			return index && *index >= 0 && static_cast<uint32_t>(*index) < widget.stateCount
				&& std::strcmp(widget.states[*index], AudioOnlyState) == 0;
		}
	}
	return false;
}

}
