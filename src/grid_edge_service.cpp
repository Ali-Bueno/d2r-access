#include "grid_edge_service.h"
#include "entity_cue_audio.h"
#include "grid_edge.h"
#include "input_direction.h"
#include "native_ui.h"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace D2RAccess::GridEdgeService {
namespace {
// Sound design: a side edge is heard on that side, a top or bottom edge centred.
constexpr double EdgePan = 0.8;
bool g_enabled = true;
GridEdge::Detector g_detector;

// The direction the D-pad or the left stick pushes, on its dominant axis.
std::pair<int, int> Push() {
	const auto dpad = InputDirection::Dpad();
	if (dpad.held && (dpad.x || dpad.y)) return {dpad.x, dpad.y};
	const auto stick = InputDirection::LeftStick();
	if (!stick.connected) return {0, 0};
	const float ax = std::abs(stick.x), ay = std::abs(stick.y);
	if (std::max(ax, ay) < InputDirection::StickPushFraction) return {0, 0};
	return ax >= ay ? std::pair{stick.x > 0 ? 1 : -1, 0} : std::pair{0, stick.y > 0 ? 1 : -1};
}
}

void Tick() {
	if (!g_enabled) return;
	GridEdge::Sample sample;
	sample.now = GetTickCount64();
	const auto focus = NativeUi::GameFocus();
	if (const auto cell = focus.widget ? NativeUi::GridCell(focus.widget) : NativeUi::Cell{}; cell.x >= 0) {
		const auto size = NativeUi::GridSize(focus.widget);
		sample = {focus.widget, cell.x, cell.y, size.x, size.y, 0, 0, sample.now};
		std::tie(sample.dx, sample.dy) = Push();
	}
	if (const auto edge = g_detector.Update(sample))
		EntityCueAudio::Play(EntityCueAudio::Sound::Edge, {edge->first * EdgePan, 1});
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) {
	g_enabled = enabled;
	g_detector.Reset();
}
}
