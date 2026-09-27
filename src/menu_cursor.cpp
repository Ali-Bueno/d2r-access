#include "menu_cursor.h"

#include "menu_navigation_policy.h"

#include <algorithm>

namespace D2RAccess::MenuCursor {
namespace {
constexpr Direction All[] = {Direction::Left, Direction::Right, Direction::Up, Direction::Down};

bool IsGrid(const Target& target) { return target.grid.width > 0 && target.grid.height > 0; }
bool Inside(const GridJump::Grid& grid, Cell cell) {
	return cell.x >= 0 && cell.y >= 0 && cell.x < grid.width && cell.y < grid.height;
}

// What stands on a cell: the item covering it from its top-left cell, or the cell alone.
Place OnCell(const Target& target, Cell cell) {
	const int item = GridJump::ItemAt(target.grid, cell);
	if (item == GridJump::Empty) return {target.widget, cell, cell};
	const auto& found = target.grid.items[static_cast<size_t>(item)];
	return {target.widget, {found.x, found.y}, cell};
}

// A span of cells along one axis of a grid, in the units the grid is drawn in.
int Edge(int origin, int length, int cells, int cell) {
	return origin + static_cast<int>(static_cast<int64_t>(length) * cell / cells);
}

// The cell of the grid a step from `origin` enters it at: on the near edge,
// as close to where the step came from as the grid has.
Cell Entry(const Target& target, const Rect& origin, Direction direction) {
	const auto& grid = target.grid;
	const auto nearest = [](int centre, int start, int length, int cells) {
		if (length <= 0) return 0;
		const int64_t cell = (static_cast<int64_t>(centre) - start) * cells / length;
		return static_cast<int>(std::clamp<int64_t>(cell, 0, cells - 1));
	};
	Cell cell{nearest(origin.x + origin.width / 2, target.rect.x, target.rect.width, grid.width),
		nearest(origin.y + origin.height / 2, target.rect.y, target.rect.height, grid.height)};
	switch (direction) {
	case Direction::Left:  cell.x = grid.width - 1; break;
	case Direction::Right: cell.x = 0; break;
	case Direction::Up:    cell.y = grid.height - 1; break;
	case Direction::Down:  cell.y = 0; break;
	}
	return cell;
}

// The place as the one thing it is, whatever cell of it was travelled through.
Place Anchored(const Place& place) { return {place.widget, place.anchor, place.anchor}; }
bool Known(const std::vector<Place>& places, const Place& place) {
	return std::find(places.begin(), places.end(), Anchored(place)) != places.end();
}
}

const Target* Find(std::span<const Target> targets, uintptr_t widget) {
	const auto found = std::find_if(targets.begin(), targets.end(), [widget](const Target& target) {
		return widget && target.widget == widget;
	});
	return found == targets.end() ? nullptr : &*found;
}

Rect RectOf(const Target& target, const Place& place) {
	if (!IsGrid(target) || !Inside(target.grid, place.anchor)) return target.rect;
	int width = 1, height = 1;
	if (const int item = GridJump::ItemAt(target.grid, place.anchor); item != GridJump::Empty) {
		const auto& found = target.grid.items[static_cast<size_t>(item)];
		if (found.x == place.anchor.x && found.y == place.anchor.y) {
			width = std::min(found.width, target.grid.width - found.x);
			height = std::min(found.height, target.grid.height - found.y);
		}
	}
	const int left = Edge(target.rect.x, target.rect.width, target.grid.width, place.anchor.x);
	const int top = Edge(target.rect.y, target.rect.height, target.grid.height, place.anchor.y);
	const int right = Edge(target.rect.x, target.rect.width, target.grid.width, place.anchor.x + width);
	const int bottom = Edge(target.rect.y, target.rect.height, target.grid.height, place.anchor.y + height);
	return {left, top, right - left, bottom - top};
}

Place Settle(std::span<const Target> targets, const Place& place) {
	const auto* target = Find(targets, place.widget);
	if (!target) return {};
	if (!IsGrid(*target)) return {target->widget, {}, {}};
	const Cell cell{std::clamp(place.travel.x, 0, target->grid.width - 1), std::clamp(place.travel.y, 0, target->grid.height - 1)};
	return OnCell(*target, cell);
}

Place First(std::span<const Target> targets) {
	const Target* first = nullptr;
	for (const auto& target : targets) {
		if (!target.widget || target.rect.width <= 0 || target.rect.height <= 0) continue;
		if (!first || target.rect.y < first->rect.y || (target.rect.y == first->rect.y && target.rect.x < first->rect.x)) first = &target;
	}
	if (!first) return {};
	return IsGrid(*first) ? OnCell(*first, {0, 0}) : Place{first->widget, {}, {}};
}

Place Move(std::span<const Target> targets, const Place& from, Direction direction, bool byCell) {
	const auto* current = Find(targets, from.widget);
	if (!current) return First(targets);
	if (IsGrid(*current) && Inside(current->grid, from.travel)) {
		if (byCell) {
			Cell next = from.travel;
			switch (direction) {
			case Direction::Left:  --next.x; break;
			case Direction::Right: ++next.x; break;
			case Direction::Up:    --next.y; break;
			case Direction::Down:  ++next.y; break;
			}
			if (Inside(current->grid, next)) return {current->widget, next, next};
		} else if (const auto step = GridJump::Move(current->grid, from.travel, direction); step.moved) {
			return {current->widget, step.anchor, step.cursor};
		}
	}
	// Nothing further in this grid, or not in one: the nearest thing that way.
	// The place stands in the list as itself, and the rest by their place in it.
	constexpr uintptr_t Here = 1, Offset = 2;
	const Rect origin = byCell && IsGrid(*current)
		? RectOf(*current, {current->widget, from.travel, from.travel}) : RectOf(*current, from);
	std::vector<MenuNavigationPolicy::Node> nodes{{Here, origin.x, origin.y, origin.width, origin.height}};
	for (size_t i = 0; i < targets.size(); ++i) {
		const auto& target = targets[i];
		if (&target == current || !target.widget) continue;
		nodes.push_back({i + Offset, target.rect.x, target.rect.y, target.rect.width, target.rect.height});
	}
	const int dx = direction == Direction::Left ? -1 : direction == Direction::Right ? 1 : 0;
	const int dy = direction == Direction::Up ? -1 : direction == Direction::Down ? 1 : 0;
	const uintptr_t chosen = MenuNavigationPolicy::Move(nodes, Here, dx, dy);
	if (chosen < Offset) return from;
	const auto& target = targets[chosen - Offset];
	if (!IsGrid(target)) return {target.widget, {}, {}};
	const Cell cell = Entry(target, origin, direction);
	return byCell ? Place{target.widget, cell, cell} : OnCell(target, cell);
}

std::vector<Place> Everything(std::span<const Target> targets) {
	std::vector<Place> places;
	for (const auto& target : targets) {
		if (!target.widget || target.rect.width <= 0 || target.rect.height <= 0) continue;
		if (!IsGrid(target)) { places.push_back({target.widget, {}, {}}); continue; }
		for (int y = 0; y < target.grid.height; ++y) {
			for (int x = 0; x < target.grid.width; ++x) {
				const auto place = Anchored(OnCell(target, {x, y}));
				if (!Known(places, place)) places.push_back(place);
			}
		}
	}
	return places;
}

std::vector<Place> Reachable(std::span<const Target> targets, const Place& from) {
	std::vector<Place> met, seen, queue;
	if (!Find(targets, from.widget)) return met;
	met.push_back(Anchored(from));
	seen.push_back(from);
	queue.push_back(from);
	for (size_t head = 0; head < queue.size(); ++head) {
		for (const auto direction : All) {
			const auto next = Move(targets, Place(queue[head]), direction, false);
			// A cell travelled through is a state of its own: the same item
			// entered on another row goes on along that row.
			if (std::find(seen.begin(), seen.end(), next) != seen.end()) continue;
			seen.push_back(next);
			queue.push_back(next);
			if (!Known(met, next)) met.push_back(Anchored(next));
		}
	}
	return met;
}
}
