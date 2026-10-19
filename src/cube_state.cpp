#include "cube_state.h"

#include "async_log.h"
#include "controller_text.h"
#include "inventory_cursor.h"
#include "localization.h"
#include "native_ground_items.h"
#include "native_ui.h"
#include "native_widgets.h"
#include "panel_state.h"
#include "speech_strings.h"

#include <windows.h>
#include <D2RLPlugin/item.h>
#include <algorithm>
#include <cstdio>

namespace D2RAccess::CubeState {
namespace {
constexpr uint32_t CubeCode = D2RL::Items::MakeItemCode("box");
// ui-controller.json: "Hold <A> to Open", the line misc.txt gives the cube.
constexpr std::string_view OpenHintKey = "OpenHoradricCubeController";
constexpr uint64_t CarriedCacheMs = 500;
}

bool PanelOpen() {
	const auto& open = PanelState::OpenPanels();
	return std::find(open.begin(), open.end(), Panel) != open.end();
}

bool StashShown(uintptr_t stashPanel) {
	const uintptr_t cube = stashPanel ? NativeWidgets::FindChild(stashPanel, StashCube) : 0;
	return cube && NativeUi::Visible(cube);
}

bool Up() { return PanelOpen() || StashShown(NativeWidgets::FindPanel(StashPanel)); }

bool Carried(const D2RL::PluginContext* context) {
	static uint64_t checked = 0;
	static bool carried = false;
	const uint64_t now = GetTickCount64();
	if (checked && now - checked < CarriedCacheMs) return carried;
	checked = now;
	return carried = InventoryCursor::Holds(context, D2RL::Items::ItemContainer::Inventory, CubeCode);
}

std::string Name() {
	if (const auto classId = NativeGroundItems::ClassOfCode(CubeCode)) {
		auto name = NativeGroundItems::ClassLabel(*classId, 0).baseName;
		if (!name.empty()) return name;
	}
	return Strings::Current().horadricCube;
}

std::string WithStashStatus(const D2RL::PluginContext* context, const std::string& body, uintptr_t stashPanel) {
	static int logged = -1;
	const bool carried = Carried(context), shown = StashShown(stashPanel);
	if (const int state = (carried ? 2 : 0) | (shown ? 1 : 0); state != logged) {
		logged = state;
		AsyncLog::InfoF(context, "[cube] stash carried=%d shown=%d", carried ? 1 : 0, shown ? 1 : 0);
	}
	if (body.empty() || !carried || !shown) return body;
	char text[200]{};
	std::snprintf(text, sizeof(text), Strings::Current().stashCubeOpen, body.c_str());
	return text;
}

std::string WithOpenHint(const D2RL::PluginContext* context, const std::string& help, uintptr_t item) {
	if (!item || NativeGroundItems::ItemTableCode(item) != CubeCode) return help;
	const auto hint = ControllerText::Read(Localization::Text(OpenHintKey));
	static std::string logged;
	if (const auto line = help + "|" + hint; line != logged) {
		logged = line;
		AsyncLog::InfoF(context, "[cube] open hint game=\"%s\" legend=\"%s\"", hint.c_str(), help.c_str());
	}
	if (hint.empty() || help.find(hint) != std::string::npos) return help;
	return help.empty() ? hint : help + ". " + hint;
}

}
