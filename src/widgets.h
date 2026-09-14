#pragma once

#include <D2RLPlugin/api.h>

#include <optional>
#include <span>
#include <string_view>

// Wrapper over the SDK's WidgetService. Every function here must be called on
// the UI thread (see ui_thread_pump.h). Rects returned by the SDK are local to
// the widget's parent and do NOT include the layout's "anchor" (a fraction of
// the parent's size the local rect is offset from); AbsoluteRect applies the
// anchors along a name path so widgets from different panels share one frame.
namespace D2RAccess::Widgets {

bool Initialize(const D2RL::PluginContext* context);
void Shutdown();
bool IsAvailable();

struct Rect {
	int32_t x      = 0;
	int32_t y      = 0;
	int32_t width  = 0;
	int32_t height = 0;

	bool operator==(const Rect&) const = default;
};

// One step down the widget tree: the child's name and its layout anchor.
struct Step {
	const char* name    = nullptr;
	float       anchorX = 0.0f;
	float       anchorY = 0.0f;
};

std::optional<D2RL::Widgets::WidgetHandle> FindPanel(std::string_view panel);

// Rect of the panel's root widget as the SDK reports it (local to the screen).
std::optional<Rect> PanelRect(std::string_view panel);

// Screen-space rect of the widget reached from `panel` (whose root carries
// `rootAnchor`) by following `path`, outermost step first.
std::optional<Rect> AbsoluteRect(std::string_view panel, Step rootAnchor, std::span<const Step> path);

// Why the last FindPanel/PanelRect/AbsoluteRect returned nothing ("panel not
// found", "widget not found 'X'", "rect failed for 'X'"); empty after a success.
const char* LastError();

}
