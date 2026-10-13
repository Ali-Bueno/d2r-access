#include "grid_edge_service.h"
#include "async_log.h"
#include "entity_cue_audio.h"
#include "grid_edge.h"
#include "input_direction.h"
#include "native_ui.h"

#include <windows.h>
#include <algorithm>
#include <cmath>

namespace D2RAccess::GridEdgeService {
namespace {
// Sound design: a side edge is heard on that side, a top or bottom edge centred.
constexpr double EdgePan = 0.8;
bool g_enabled = true;
GridEdge::Detector g_detector;
// Only grids knock: the backpack, a vendor's, the stash's and the cube's (a tester,
// 2026-10-12: equipment slots must not). A push from one panel's grid into another's,
// the border between the backpack and a vendor or the stash, knocks as well.
GridEdge::CrossingTracker g_crossings;

// The direction the D-pad, the arrow keys or the left stick push, on its dominant axis.
std::pair<int, int> Push() {
	for (const auto& pad : {InputDirection::Dpad(), InputDirection::ArrowKeys()})
		if (pad.held && (pad.x || pad.y)) return {pad.x, pad.y};
	const auto stick = InputDirection::LeftStick();
	if (!stick.connected) return {0, 0};
	const float ax = std::abs(stick.x), ay = std::abs(stick.y);
	if (std::max(ax, ay) < InputDirection::StickPushFraction) return {0, 0};
	return ax >= ay ? std::pair{stick.x > 0 ? 1 : -1, 0} : std::pair{0, stick.y > 0 ? 1 : -1};
}

const char* OutcomeName(GridEdge::Outcome::Kind kind) {
	switch (kind) {
	case GridEdge::Outcome::Kind::Knock: return "stayed, knock";
	case GridEdge::Outcome::Kind::Left: return "focus left the grid";
	default: return "";
	}
}
}

void Tick(const D2RL::PluginContext* context) {
	if (!g_enabled) return;
	GridEdge::Sample sample;
	sample.now = GetTickCount64();
	const auto focus = NativeUi::GameFocus();
	uintptr_t panel = 0;
	const auto [pushX, pushY] = Push();
	if (const auto cell = focus.widget ? NativeUi::GridCell(focus.widget) : NativeUi::Cell{}; cell.x >= 0) {
		const auto size = NativeUi::GridSize(focus.widget);
		panel = focus.panel;
		sample = {focus.widget, cell.x, cell.y, size.x, size.y, 0, 0, sample.now};
		sample.dx = pushX;
		sample.dy = pushY;
	}
	// The crossing is judged on its own: the game may switch grids within one frame or via a blank focus.
	if (const auto crossing = g_crossings.Update(panel, pushX, pushY, sample.now); crossing.crossed) {
		AsyncLog::InfoF(context, "[grid-edge] crossed %p -> %p", reinterpret_cast<void*>(crossing.from), reinterpret_cast<void*>(crossing.to));
		EntityCueAudio::Play(EntityCueAudio::Sound::Edge, {crossing.dx * EdgePan, 1});
	}
	const auto outcome = g_detector.Update(sample);
	if (outcome.kind == GridEdge::Outcome::Kind::None) return;
	// Logged with every outcome: what the game does at the edge is what the knock rests on.
	AsyncLog::InfoF(context, "[grid-edge] push %d,%d from cell %d,%d of %dx%d: %s", outcome.dx, outcome.dy, outcome.x, outcome.y,
		sample.width, sample.height, OutcomeName(outcome.kind));
	if (outcome.kind == GridEdge::Outcome::Kind::Knock)
		EntityCueAudio::Play(EntityCueAudio::Sound::Edge, {outcome.dx * EdgePan, 1});
}

bool Enabled() { return g_enabled; }
void SetEnabled(bool enabled) {
	g_enabled = enabled;
	g_detector.Reset();
	g_crossings.Reset();
}
}
