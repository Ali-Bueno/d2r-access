#include "widgets.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace D2RAccess::Widgets {

namespace {

// Layout fact (Controller/FocusManagerHD.json): the focus manager is a
// full-screen root panel, so its rect doubles as the screen size in layout
// units when the SDK reports one.
constexpr const char* ScreenPanelNames[] = { "FocusManager", "Controller/FocusManager" };

// HD layouts are authored on a 3840x2160 canvas. The SDK reports root panels
// as 0x0, so this is the frame their anchors resolve against. Derived from
// in-game measurements (2026-09-14): MainMenuPanel/QuickPlayButton anchor 0.5,
// local x -710 -> indicator x 1211; CinematicsModal anchor y 0.1, Option0 y 170
// -> indicator y 386.
constexpr Rect LayoutCanvas { 0, 0, 3840, 2160 };

const D2RL::WidgetServiceV1* g_service = nullptr;
const D2RL::PluginContext*   g_context = nullptr;
char                         g_lastError[160] {};

// Name lookups walk the widget tree inside the loader, so a panel's handle
// and each child's handle are kept until the panel set changes
// (ForgetHandles). A handle whose rect can no longer be read is dropped, so
// a stale entry costs one failed call.
std::unordered_map<std::string, D2RL::Widgets::WidgetHandle> g_panelHandles; // panel name -> root
std::unordered_map<std::string, D2RL::Widgets::WidgetHandle> g_childHandles; // "<parent handle>/<name>" -> child

// Screen-space rects computed during the current poll, keyed by handle:
// widgets under one parent share its placement, and the screen rect is asked
// once. Cleared by BeginPoll.
std::unordered_map<D2RL::Widgets::WidgetHandle, Rect> g_placed;
std::optional<Rect>                                   g_screen;

void Fail(const char* what, const char* name, unsigned code) {
	std::snprintf(g_lastError, sizeof(g_lastError), "%s '%s' (result %u)", what, name, code);
}

std::string ChildKey(D2RL::Widgets::WidgetHandle parent, const char* name) {
	return std::to_string(parent) + "/" + name;
}

std::optional<Rect> LocalRect(D2RL::Widgets::WidgetHandle handle, const char* name) {
	D2RL::Widgets::Rect rect {};
	const auto          result = g_service->getWidgetRect(g_context, handle, &rect);
	if (result != D2RL::Widgets::Result::Success) {
		Fail("rect failed for", name, static_cast<unsigned>(result));
		return std::nullopt;
	}
	return Rect { rect.x, rect.y, rect.width, rect.height };
}

std::optional<D2RL::Widgets::WidgetHandle> FindChild(D2RL::Widgets::WidgetHandle parent, const char* name) {
	const std::string key = ChildKey(parent, name);
	if (const auto cached = g_childHandles.find(key); cached != g_childHandles.end()) {
		return cached->second;
	}
	D2RL::Widgets::WidgetHandle child  = D2RL::Widgets::InvalidHandle;
	const auto                  result = g_service->findWidget(g_context, parent, name, &child);
	if (result != D2RL::Widgets::Result::Success) {
		Fail("widget not found", name, static_cast<unsigned>(result));
		return std::nullopt;
	}
	g_childHandles[key] = child;
	return child;
}

// Places `local` inside `parent`. Measured behaviour: the SDK's local rect
// already includes the anchor offset whenever the parent has a size on that
// axis; a 0-sized parent (root panels, the screen) contributes nothing, so
// the anchor is applied here against the layout canvas instead.
int32_t PlaceAxis(int32_t parentOrigin, int32_t parentSize, int32_t canvasSize, float anchor, int32_t local) {
	const int32_t anchorOffset = parentSize > 0 ? 0 : static_cast<int32_t>(std::lround(anchor * static_cast<float>(canvasSize)));
	return parentOrigin + anchorOffset + local;
}

Rect Place(const Rect& parent, const Rect& local, float anchorX, float anchorY) {
	return Rect {
		PlaceAxis(parent.x, parent.width, LayoutCanvas.width, anchorX, local.x),
		PlaceAxis(parent.y, parent.height, LayoutCanvas.height, anchorY, local.y),
		local.width,
		local.height,
	};
}

// Screen frame every root panel is anchored in. Reported as 0x0 by the SDK
// (see Place), so it resolves to the canvas unless the focus manager root
// ever reports a real size.
Rect ScreenRect() {
	if (g_screen) {
		return *g_screen;
	}
	g_screen = Rect { 0, 0, 0, 0 };
	for (const char* name : ScreenPanelNames) {
		const auto rect = PanelRect(name);
		if (rect && rect->width > 0 && rect->height > 0) {
			g_screen = Rect { 0, 0, rect->width, rect->height };
			break;
		}
	}
	return *g_screen;
}

// Screen-space placement of `handle` (already known or computed from its
// local rect inside `parent`), memoized for this poll.
std::optional<Rect> Placed(D2RL::Widgets::WidgetHandle handle, const char* name, const Rect& parent, float anchorX, float anchorY) {
	if (const auto cached = g_placed.find(handle); cached != g_placed.end()) {
		return cached->second;
	}
	const auto local = LocalRect(handle, name);
	if (!local) {
		return std::nullopt;
	}
	const Rect placed = Place(parent, *local, anchorX, anchorY);
	g_placed[handle]  = placed;
	return placed;
}

}

bool Initialize(const D2RL::PluginContext* context) {
	if (context == nullptr) {
		return false;
	}
	if (context->QueryService(D2RL::ServiceId::Widget, D2RL::WidgetServiceV1Version, &g_service) != D2RL::ServiceQueryResult::Success) {
		g_service = nullptr;
		return false;
	}
	if (!D2RL::HasWidgetServiceV1Field(g_service, D2RL::WidgetServiceV1RequiredSize)) {
		g_service = nullptr;
		return false;
	}
	g_context = context;
	return true;
}

void Shutdown() {
	ForgetHandles();
	g_service = nullptr;
	g_context = nullptr;
}

bool IsAvailable() {
	return g_service != nullptr;
}

void BeginPoll() {
	g_placed.clear();
	g_screen.reset();
}

void ForgetHandles() {
	g_panelHandles.clear();
	g_childHandles.clear();
	BeginPoll();
}

std::optional<D2RL::Widgets::WidgetHandle> FindPanel(std::string_view panel) {
	if (!IsAvailable()) {
		return std::nullopt;
	}
	const std::string name(panel);
	if (const auto cached = g_panelHandles.find(name); cached != g_panelHandles.end()) {
		return cached->second;
	}
	D2RL::Widgets::WidgetHandle handle = D2RL::Widgets::InvalidHandle;
	const auto                  result = g_service->findPanel(g_context, name.c_str(), &handle);
	if (result != D2RL::Widgets::Result::Success) {
		Fail("panel not found", name.c_str(), static_cast<unsigned>(result));
		return std::nullopt;
	}
	g_panelHandles[name] = handle;
	g_lastError[0]       = '\0';
	return handle;
}

std::optional<Rect> PanelRect(std::string_view panel) {
	const auto handle = FindPanel(panel);
	if (!handle) {
		return std::nullopt;
	}
	const std::string name(panel);
	const auto        rect = LocalRect(*handle, name.c_str());
	if (!rect) {
		g_panelHandles.erase(name); // the panel went away: look it up again next time
	}
	return rect;
}

std::optional<Rect> AbsoluteRect(std::string_view panel, Step rootAnchor, std::span<const Step> path) {
	const auto panelHandle = FindPanel(panel);
	if (!panelHandle) {
		return std::nullopt;
	}
	// A root keeps its reported size (0x0 for most panels): that is what tells
	// Place whether its children's anchors were already applied by the SDK.
	const std::string panelName(panel);
	const auto        current = Placed(*panelHandle, panelName.c_str(), ScreenRect(), rootAnchor.anchorX, rootAnchor.anchorY);
	if (!current) {
		g_panelHandles.erase(panelName);
		return std::nullopt;
	}
	Rect                        placed = *current;
	D2RL::Widgets::WidgetHandle parent = *panelHandle;
	for (const Step& step : path) {
		const auto child = FindChild(parent, step.name);
		if (!child) {
			return std::nullopt;
		}
		const auto rect = Placed(*child, step.name, placed, step.anchorX, step.anchorY);
		if (!rect) {
			g_childHandles.erase(ChildKey(parent, step.name)); // stale handle: re-find next time
			return std::nullopt;
		}
		placed = *rect;
		parent = *child;
	}
	g_lastError[0] = '\0';
	return placed;
}

const char* LastError() {
	return g_lastError;
}

}
