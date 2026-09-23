#include "belt_reader.h"
#include "controller_text.h"
#include "localization.h"
#include "native_belt.h"
#include "speech_strings.h"
#include <cstdio>

namespace D2RAccess::BeltReader {
namespace {
void Append(std::string& text, const std::string& addition) {
	if (addition.empty()) return;
	if (!text.empty()) text += ". ";
	text += addition;
}
}
std::string Location(int slot) {
	if (slot < 0 || slot >= Columns * Rows) return {};
	const auto key = "ControllerHelpBelt" + std::to_string(slot % Columns + 1);
	auto text = ControllerText::Read(Localization::Text(key));
	char row[100]{};
	std::snprintf(row, sizeof(row), Strings::Current().beltRow, slot / Columns + 1);
	Append(text, row);
	return text;
}
Readout Read(const D2RL::PluginContext* context, uintptr_t widget) {
	Readout result;
	result.item = NativeUi::FocusedItem(widget);
	if (!result.item.known || result.item.slot < 0 || result.item.slot >= Columns * Rows) return result;
	result.location = Location(result.item.slot);
	int count = 0;
	bool known = true;
	for (int slot = result.item.slot % Columns; slot < Columns * Rows; slot += Columns) {
		const auto cell = slot == result.item.slot ? result.item : NativeUi::BeltItem(widget, slot);
		if (!cell.known) { known = false; break; }
		if (cell.item) ++count;
	}
	if (known) {
		char phrase[100]{};
		std::snprintf(phrase, sizeof(phrase), Strings::Current().beltColumnCount, count);
		result.summary = phrase;
	}
	if (!result.item.item && result.item.slot < Columns) {
		const auto automatic = NativeBelt::AutomaticItem(context, widget, result.item.slot);
		if (automatic && *automatic) {
			result.item.item = *automatic;
			Append(result.summary, Strings::Current().beltAutomatic);
		}
	}
	return result;
}
}
